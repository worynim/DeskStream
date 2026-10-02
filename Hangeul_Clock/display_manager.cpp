// worynim@gmail.com
/**
 * @file display_manager.cpp
 * @brief 고수준 디스플레이 및 UI 스테이지 관리 클래스 구현
 * @details 4개 OLED 디스플레이 제어, 부저 피드백, UI 상태 전환 로직 구현
 * @note [SYNC] ENG_Clock/display_manager.cpp — applyTimezone() 추가
 */
#include "display_manager.h"
#include "LittleFS.h"
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

DisplayManager display;

// 시보 표시용 종 모양 아이콘 (8x8)
static const uint8_t bell_icon[] = { 0x18, 0x3C, 0x3C, 0x3C, 0xFF, 0xDB, 0x18, 0x00 };

// 하위 레벨 I2C 콜백 및 설정은 i2c_platform.cpp로 이관됨

/**
 * @brief 눈 조립용 결정적 시드 조합
 * @details 전환마다 transitionId가 증가하므로 매초 다른 눈이 오고,
 *         같은 화면 안에서도 문자 슬롯과 x 위치에 따라 시선이 흩어진다.
 *         조립(k)과 소멸(k + offset) 시드가 겹치지 않도록 소멸 슬롯만 뒤로 민다.
 */
#define SNOW_DISPERSE_SLOT_OFFSET 8

static uint16_t snowSeed(uint8_t transitionId, int screenIdx, int slot, int x) {
    return (uint16_t)(transitionId * 7919 + screenIdx * 131 + slot * 17 + x * 3);
}

static void buzzerTimerCallback(TimerHandle_t xTimer) {
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
}

// DisplayManager 메서드 구현
DisplayManager::DisplayManager() : 
    u8g2_1(U8G2_R2, 255, 255, U8X8_PIN_NONE), u8g2_2(U8G2_R2, 255, 255, U8X8_PIN_NONE),
    u8g2_3(U8G2_R2, SW_SCL_PIN, SW_SDA_PIN, U8X8_PIN_NONE), u8g2_4(U8G2_R2, SW_SCL_PIN, SW_SDA_PIN, U8X8_PIN_NONE) {
    screens[0] = &u8g2_1; screens[1] = &u8g2_2; screens[2] = &u8g2_3; screens[3] = &u8g2_4;
}

void DisplayManager::begin() {
    applyFlip();
    main_task_handle = xTaskGetCurrentTaskHandle();
    for (int i = 0; i < NUM_SCREENS; i++) screens[i]->getU8g2()->tile_buf_ptr = u8g2_buffers[i];
    
    // 1. I2C 플랫폼 초기화
    i2cPlatform.begin();
    
    u8g2_1.getU8x8()->byte_cb = u8x8_byte_esp32_idf_0; u8g2_1.begin();
    u8g2_2.getU8x8()->byte_cb = u8x8_byte_esp32_idf_1; u8g2_2.setI2CAddress(I2C_ADDR_HW_1 * 2); u8g2_2.begin();
    u8g2_3.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast; u8g2_3.begin();
    u8g2_4.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast; u8g2_4.setI2CAddress(I2C_ADDR_HW_1 * 2); u8g2_4.begin();
    
    // 2. 렌더러 초기화
    renderer.setScreens(screens);

    // 3. 설정 매니저 초기화 및 로드
    configManager.begin();
    
    for (int i = 0; i < 4; i++) {
        screens[i]->setFlipMode(configManager.get().is_flipped);
        screens[i]->sendF("c", configManager.get().is_inverted ? 0xA7 : 0xA6);
        screens[i]->setContrast(configManager.get().brightness);
        screens[i]->setBitmapMode(1);
        screens[i]->clearBuffer();
        lastTexts[i] = "";
    }
    pushParallel();

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    buzzerTimer = xTimerCreate("BuzzerTimer", pdMS_TO_TICKS(50), pdFALSE, (void*)0, buzzerTimerCallback);

    // 슬롯 이름 초기 캐싱
    for (int i = 0; i < 5; i++) {
        String p = "/f" + String(i) + "/name.txt";
        if (LittleFS.exists(p)) {
            File f = LittleFS.open(p, "r");
            if (f) {
                _slotNames[i] = f.readString();
                f.close();
            }
        } else {
            _slotNames[i] = "Empty";
        }
    }
}

void DisplayManager::setFlipDisplay(bool flip) {
    configManager.get().is_flipped = flip;
    configManager.setDirty();
    for (int i = 0; i < 4; i++) {
        screens[i]->setFlipMode(flip);
    }
    setForceUpdate(true);
}

void DisplayManager::applyFlip() {
    screens[0] = &u8g2_1; screens[1] = &u8g2_2; screens[2] = &u8g2_3; screens[3] = &u8g2_4;
}

void DisplayManager::setChime(bool enable) {
    configManager.get().chime_enabled = enable;
    configManager.setDirty();
    setForceUpdate(true);
}

void DisplayManager::setForceUpdate(bool force) {
    _needsForceUpdate = force;
}

bool DisplayManager::checkForceUpdate() {
    bool f = _needsForceUpdate;
    _needsForceUpdate = false;
    return f;
}

void DisplayManager::setDisplayMode(uint8_t mode) {
    configManager.get().display_mode = mode;
    configManager.setDirty();
    setForceUpdate(true);
}

void DisplayManager::setHourFormat(uint8_t format) {
    configManager.get().hour_format = format;
    configManager.setDirty();
    setForceUpdate(true);
}

void DisplayManager::setAnimMode(uint8_t mode) {
    configManager.get().anim_mode = mode;
    configManager.setDirty();
    setForceUpdate(true);
}

void DisplayManager::applyTimezone() {
    const char* tz = configManager.get().timezone;
    // configTime(0, 0, ...)을 쓰면 코어(esp32-hal-time.c)가 마지막에 setTimeZone(-0, 0)을
    //   호출해 setenv("TZ", "UTC0DST0")로 **방금 설정한 TZ를 덮어쓴다**. GMT 오프셋 0을
    //   POSIX TZ로 강제 변환하기 때문에 어떤 타임존을 골라도 항상 UTC로 표시된다.
    // configTzTime()은 SNTP 서버 설정 후 setenv("TZ", tz) + tzset()까지 해 주는
    //   코어 제공 함수이므로 별도의 setenv/tzset이 필요 없다.
    configTzTime(tz, NTP_SERVER1, NTP_SERVER2);
    Serial.printf("[TZ] Applied: %s\n", tz);
    setForceUpdate(true);   // 다음 루프에서 새 시각으로 즉시 재렌더링
}

String DisplayManager::getSlotName(uint8_t slot) {
    if (slot >= 5) return "";
    return _slotNames[slot];
}

void DisplayManager::setFontName(const String& name) {
    if (configManager.get().font_name == name) return;
    
    configManager.get().font_name = name;
    _slotNames[configManager.get().font_slot] = name; // 캐시 업데이트
    configManager.setDirty();
    
    // 현재 슬롯 폴더에 name.txt 저장
    String path = "/f" + String(configManager.get().font_slot) + "/name.txt";
    File f = LittleFS.open(path, "w");
    if (f) {
        f.print(name);
        f.close();
    }
}

void DisplayManager::setFontSlot(uint8_t slot) {
    if (slot >= 5) slot = 0;
    
    // 이미 해당 슬롯이면 중복 로딩 방지
    if (configManager.get().font_slot == slot && renderer.isCacheLoaded()) {
        return;
    }
    
    configManager.get().font_slot = slot;
    
    // 새 슬롯의 이름 로드
    String path = "/f" + String(slot) + "/name.txt";
    if (LittleFS.exists(path)) {
        File f = LittleFS.open(path, "r");
        if (f) {
            configManager.get().font_name = f.readString();
            f.close();
        }
    } else {
        configManager.get().font_name = "Empty Slot";
    }
    
    configManager.setDirty();
    
    // 애니메이션 중이면 즉시 중단 (폰트 맵이 바뀌면 애니메이션 데이터가 무효화됨)
    if (_animState.active) {
        _animState.active = false;
        refreshNow();
    }
    
    // 렌더러 캐시 갱신
    renderer.loadBitmapCache(slot);
    
    // 폰트 로드 후 화면 강제 갱신 (잔상 제거)
    refreshNow();
    
    setForceUpdate(true);
}

void DisplayManager::refreshNow() {
    for (int i = 0; i < 4; i++) {
        bool isTitleScreen = configManager.get().is_flipped ? (i == 3) : (i == 0);
        drawCenterText(i, lastTexts[i], isTitleScreen);
    }
    pushParallel();
}

void DisplayManager::setInversion(bool invert) {
    configManager.get().is_inverted = invert;
    configManager.setDirty();
    for (int i = 0; i < 4; i++) {
        screens[i]->sendF("c", invert ? 0xA7 : 0xA6);
    }
    setForceUpdate(true);
}

void DisplayManager::setBrightness(uint8_t brightness) {
    configManager.get().brightness = brightness;
    configManager.setDirty();
    for (int i = 0; i < 4; i++) {
        screens[i]->setContrast(brightness);
    }
}

// saveConfig()은 ConfigManager로 대체됨

void DisplayManager::loadBitmapCache() {
    renderer.loadBitmapCache();
}

// findChar, getHexKey 로직은 Renderer로 이동됨

void DisplayManager::clearAll() {
    u8g2_1.clearBuffer();
    u8g2_2.clearBuffer();
    u8g2_3.clearBuffer();
    u8g2_4.clearBuffer();
    
    // 섀도우 버퍼를 무효화하여 pushParallel()이 모든 0픽셀을 강제로 전송하게 함
    for(int i=0; i<NUM_SCREENS; i++) {
        i2cPlatform.invalidateShadow(i);
    }
    
    pushParallel(); // 비동기 전송으로 경합 방지
    setForceUpdate(true); // 즉시 다음 프레임 그리기 예약
}

void DisplayManager::beep(int duration, int freq) {
    if (buzzerTimer == NULL) return;
    tone(BUZZER_PIN, freq);
    xTimerChangePeriod(buzzerTimer, pdMS_TO_TICKS(duration), 0);
    xTimerStart(buzzerTimer, 0);
}

void DisplayManager::setYieldCallback(void (*cb)()) {
    on_yield_callback = cb;
}

// drawDitheredChar, drawZoomedChar, drawSingleChar, drawScaledChar, getCharData 로직 Renderer로 이관됨
void DisplayManager::drawChimeIcon(int idx) {
    bool isTitleScreen = configManager.get().is_flipped ? (idx == 3) : (idx == 0);
    if (isTitleScreen && configManager.get().chime_enabled) screens[idx]->drawXBM(0, 0, 8, 8, bell_icon);
}

int DisplayManager::findOldIndexAtX(const ScreenAnimData& sd, int x) const {
    for (int k = 0; k < sd.oldCount; k++) {
        if (sd.oldChars[k].x == x) return k;
    }
    return -1;
}

int DisplayManager::findNewIndexAtX(const ScreenAnimData& sd, int x) const {
    for (int j = 0; j < sd.newCount; j++) {
        if (sd.newChars[j].x == x) return j;
    }
    return -1;
}


void DisplayManager::drawCenterText(int idx, const String& text, bool centered) {
    U8G2* u8g2 = screens[idx];
    u8g2->clearBuffer();
    CharData chars[8]; int count;
    renderer.getCharData(text, chars, count, centered || (text == "정각"));
    for (int i = 0; i < count; i++) {
        renderer.drawSingleChar(idx, chars[i].c, chars[i].x, 0);
    }
    drawChimeIcon(idx);
}

extern int uiStage;

void DisplayManager::updateAll(String inTexts[4], bool force) {
    String texts[4];
    bool changed[4] = {false, false, false, false};
    bool anyChanged = false;
    
    for (int i = 0; i < 4; i++) {
        texts[i] = configManager.get().is_flipped ? inTexts[3 - i] : inTexts[i];
        if (force || texts[i] != lastTexts[i]) { changed[i] = true; anyChanged = true; }
    }
    if (!anyChanged) return;

    if (configManager.get().anim_mode == ANIMATION_TYPE_NONE) {
        for (int i = 0; i < 4; i++) {
            if (changed[i]) { 
                bool isTitleScreen = configManager.get().is_flipped ? (i == 3) : (i == 0);
                drawCenterText(i, texts[i], isTitleScreen); 
                lastTexts[i] = texts[i]; 
            }
        }
        pushParallel();
        return;
    }

    // [Step 3.1] 비차단 애니메이션 상태 초기화
    bool isSnow = (configManager.get().anim_mode == ANIMATION_TYPE_SNOW_ASSEMBLE);

    // 이전 전환이 끝나기 전에 새 전환이 시작될 수 있다(매초 호출되기 때문).
    // 이번에 변경되지 않은 화면이 이전 전환에서 애니메이션 중이었다면, 아래 루프에서
    // changed가 false로 덮어써져 렌더링이 완전히 멈추고 중간 프레임이 화면에 남는다.
    // 그 잔상을 먼저 정착된 최종 상태로 지운다.
    for (int i = 0; i < 4; i++) {
        if (_animState.screens[i].changed && !changed[i]) {
            bool isTitleScreen = configManager.get().is_flipped ? (i == 3) : (i == 0);
            drawCenterText(i, lastTexts[i], isTitleScreen);
        }
    }

    _animState.active = true;
    _animState.currentStep = 0;
    _animState.maxStep = isSnow ? ANIMATION_STEPS_SNOW : ANIMATION_STEPS_DEFAULT;
    _animState.transitionId++;      // 이번 전환에만 쓰이는 눈 시드
    _animState.lastUpdateMs = 0; // 즉시 첫 틱 실행

    for (int i = 0; i < 4; i++) {
        _animState.screens[i].changed = changed[i];
        if (changed[i]) {
            bool isCentered = configManager.get().is_flipped ? (i == 3) : (i == 0);
            bool forceCenter = (texts[i] == "정각");
            renderer.getCharData(lastTexts[i], _animState.screens[i].oldChars, _animState.screens[i].oldCount, isCentered || (lastTexts[i] == "정각"));
            renderer.getCharData(texts[i], _animState.screens[i].newChars, _animState.screens[i].newCount, isCentered || forceCenter);
            lastTexts[i] = texts[i]; // 타겟 텍스트 선점
        }
    }
}

void DisplayManager::updateTick() {
    if (!_animState.active) return;

    // [Step 4.1] 시계 모드가 아니면 애니메이션 중단 (UI 전환 대응)
    if (uiStage != 0) {
        _animState.active = false;
        return;
    }

    unsigned long now = millis();
    unsigned long stepDelay = (configManager.get().anim_mode == ANIMATION_TYPE_SNOW_ASSEMBLE)
                              ? ANIMATION_STEP_DELAY_SNOW_MS : ANIMATION_STEP_DELAY_MS;
    if (now - _animState.lastUpdateMs < stepDelay) {
        // 대기 시간 동안 yield 콜백 실행하여 타 서비스 기회 제공
        if (on_yield_callback) on_yield_callback();
        return;
    }

    _animState.lastUpdateMs = now;
    _animState.currentStep++;

    if (_animState.currentStep > _animState.maxStep) {
        _animState.active = false;
        return;
    }

    for (int i = 0; i < 4; i++) {
        if (_animState.screens[i].changed) {
            renderAnimFrame(i, _animState.currentStep);
        }
    }
    pushParallel();
}

void DisplayManager::renderAnimFrame(int i, int step) {
    // 눈 조립은 픽셀 단위 연산이라 기존 글자 단위 전환 경로와 분리한다.
    if (configManager.get().anim_mode == ANIMATION_TYPE_SNOW_ASSEMBLE) {
        renderSnowFrame(i, step);
        return;
    }

    ScreenAnimData& sd = _animState.screens[i];
    screens[i]->clearBuffer();

    for (int j = 0; j < sd.newCount; j++) {
        String nc = sd.newChars[j].c; int nx = sd.newChars[j].x;
        int oi = findOldIndexAtX(sd, nx);
        String oc = (oi >= 0) ? sd.oldChars[oi].c : "";

        if (oc == nc) {
            renderer.drawSingleChar(i, nc, nx, 0);
        } else {
            switch (configManager.get().anim_mode) {
                case ANIMATION_TYPE_SCROLL_UP:
                    if (step < 16 && oc != "") renderer.drawSingleChar(i, oc, nx, -(step * 4));
                    renderer.drawSingleChar(i, nc, nx, 64 - (step * 4));
                    break;
                case ANIMATION_TYPE_SCROLL_DOWN:
                    if (step < 16 && oc != "") renderer.drawSingleChar(i, oc, nx, (step * 4));
                    renderer.drawSingleChar(i, nc, nx, -64 + (step * 4));
                    break;
                case ANIMATION_TYPE_VERTICAL_FLIP:
                    if (step <= 8) { if (oc != "") renderer.drawScaledChar(i, oc, nx, ((8 - step) * 64) / 8); }
                    else { renderer.drawScaledChar(i, nc, nx, ((step - 8) * 64) / 8); }
                    break;
                case ANIMATION_TYPE_DITHERED_FADE:
                    if (step <= 8) { if (oc != "") renderer.drawDitheredChar(i, oc, nx, 16 - (step * 2)); }
                    else { renderer.drawDitheredChar(i, nc, nx, (step - 8) * 2); }
                    break;
                case ANIMATION_TYPE_ZOOM:
                    if (step <= 8) { if (oc != "") renderer.drawZoomedChar(i, oc, nx, ((8 - step) * 100) / 8); }
                    else {
                        int sc = (step <= 12) ? ((step - 8) * 150 / 4) : (150 - (step - 12) * 50 / 4);
                        renderer.drawZoomedChar(i, nc, nx, sc);
                    }
                    break;
            }
        }
    }

    for (int k = 0; k < sd.oldCount; k++) {
        int ox = sd.oldChars[k].x; String oc = sd.oldChars[k].c;
        if (findNewIndexAtX(sd, ox) >= 0) continue;

        switch (configManager.get().anim_mode) {
            case ANIMATION_TYPE_SCROLL_UP:   if (step < 16) renderer.drawSingleChar(i, oc, ox, -(step * 4)); break;
            case ANIMATION_TYPE_SCROLL_DOWN: if (step < 16) renderer.drawSingleChar(i, oc, ox, (step * 4)); break;
            case ANIMATION_TYPE_VERTICAL_FLIP: if (step <= 8) renderer.drawScaledChar(i, oc, ox, ((8 - step) * 64) / 8); break;
            case ANIMATION_TYPE_DITHERED_FADE: if (step <= 8) renderer.drawDitheredChar(i, oc, ox, 16 - (step * 2)); break;
            case ANIMATION_TYPE_ZOOM:          if (step <= 8) renderer.drawZoomedChar(i, oc, ox, ((8 - step) * 100) / 8); break;
        }
    }

    drawChimeIcon(i);
}

void DisplayManager::renderSnowFrame(int i, int step) {
    ScreenAnimData& sd = _animState.screens[i];
    uint8_t progress = (uint8_t)((int)step * ANIM_PROGRESS_FULL / (int)_animState.maxStep);
    screens[i]->clearBuffer();

    // 새 글자: 눈송이가 떨어져 쌓이며 조립된다.
    for (int j = 0; j < sd.newCount; j++) {
        int nx = sd.newChars[j].x;
        int oi = findOldIndexAtX(sd, nx);
        if (oi >= 0 && sd.oldChars[oi].c == sd.newChars[j].c) {
            renderer.drawSingleChar(i, sd.newChars[j].c, nx, 0); // 그대로인 글자는 움직이지 않는다
            continue;
        }
        uint16_t seed = snowSeed(_animState.transitionId, i, j, nx);
        renderer.drawAssemblingChar(i, sd.newChars[j].c, nx, progress, seed);
    }

    // 사라지는 옛 글자: 눈처럼 아래로 가라앉으며 흩어진다.
    // 같은 자리를 새 글자가 넘겨받더라도 함께 가라앉는다. 겹쳐 그려도 단색 픽셀이므로 문제가 없다.
    for (int k = 0; k < sd.oldCount; k++) {
        int ox = sd.oldChars[k].x;
        int ni = findNewIndexAtX(sd, ox);
        if (ni >= 0 && sd.newChars[ni].c == sd.oldChars[k].c) continue; // 같은 글자는 이미 정적으로 그렸다
        uint16_t seed = snowSeed(_animState.transitionId, i, k + SNOW_DISPERSE_SLOT_OFFSET, ox);
        renderer.drawDispersingChar(i, sd.oldChars[k].c, ox, progress, seed);
    }

    drawChimeIcon(i);
}

void DisplayManager::pushParallel() {
    i2cPlatform.waitForSync(I2C_SYNC_TIMEOUT_MS);

    bool any_hw_dirty = false;
    for (int s = 0; s < 2; s++) {
        uint8_t* buf = screens[s]->getBufferPtr();
        for (int p = 0; p < PAGES_PER_SCREEN; p++) {
            bool page_dirty = false; int first_tile = -1, last_tile = -1;
            for (int t = 0; t < TILES_PER_PAGE; t++) {
                bool tile_dirty = false;
                for (int tx = 0; tx < 8; tx++) {
                    int idx = p * SCREEN_WIDTH + t * 8 + tx;
                    if (buf[idx] != i2cPlatform.getShadowData(s, idx)) { 
                        tile_dirty = true; 
                        i2cPlatform.setShadowData(s, idx, buf[idx]); 
                    }
                }
                if (tile_dirty) { if (first_tile == -1) first_tile = t; last_tile = t; page_dirty = true; }
            }
            if (page_dirty) { 
                i2cPlatform.preparePageUpdate(s, p, first_tile, last_tile - first_tile + 1);
                any_hw_dirty = true; 
            }
        }
    }
    if (any_hw_dirty) { i2cPlatform.notifyTransmission(); }
    
    for (int s = 2; s < 4; s++) {
        uint8_t* buf = screens[s]->getBufferPtr();
        for (int p = 0; p < PAGES_PER_SCREEN; p++) {
            int first_tile = -1, last_tile = -1; bool page_dirty = false;
            for (int t = 0; t < TILES_PER_PAGE; t++) {
                bool tile_dirty = false;
                for (int tx = 0; tx < 8; tx++) {
                    int idx = p * SCREEN_WIDTH + t * 8 + tx;
                    if (buf[idx] != i2cPlatform.getShadowData(s, idx)) { 
                        tile_dirty = true; 
                        i2cPlatform.setShadowData(s, idx, buf[idx]); 
                    }
                }
                if (tile_dirty) { if (first_tile == -1) first_tile = t; last_tile = t; page_dirty = true; }
            }
            if (page_dirty) screens[s]->updateDisplayArea(first_tile, p, last_tile - first_tile + 1, 1);
        }
    }
}

// i2c_hw_task 구현은 i2c_platform.cpp로 이관됨

void DisplayManager::recoverI2CBus() {
    Serial.println("[I2C] Recovering HW Bus and Screens...");
    // 하위 레벨 소프트 초기화 루틴 호출 (필요 시 i2cPlatform.recoverBus() 등)
    
    // OLED 기기 재설정 및 재시작 (주소 및 콜백 필수 재할당)
    u8g2_1.getU8x8()->byte_cb = u8x8_byte_esp32_idf_0; 
    u8g2_1.begin();

    u8g2_2.getU8x8()->byte_cb = u8x8_byte_esp32_idf_1; 
    u8g2_2.setI2CAddress(I2C_ADDR_HW_1 * 2); 
    u8g2_2.begin();

    u8g2_3.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast; 
    u8g2_3.begin();

    u8g2_4.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast; 
    u8g2_4.setI2CAddress(I2C_ADDR_HW_1 * 2); 
    u8g2_4.begin();
    
    // 강제 업데이트 예약
    setForceUpdate(true);
}

void DisplayManager::playStartupMelody() {
    int melody[] = {2093, 2637, 3136, 4186}; // Do-Mi-Sol-Do
    for (int i = 0; i < 4; i++) {
        tone(BUZZER_PIN, melody[i], 100);
        delay(120);
    }
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW); 
}

void DisplayManager::playChimeMelody() {
    // 경쾌한 '띠링~' 소리 (C7 -> G7)
    tone(BUZZER_PIN, 2093, 80);  // 도 (C7)
    delay(100);
    tone(BUZZER_PIN, 3136, 150); // 솔 (G7)
    delay(160);
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
}


void DisplayManager::showLargeIP(IPAddress ip) {
    for (int i = 0; i < 4; i++) {
        screens[i]->clearBuffer();
        int ip_idx = configManager.get().is_flipped ? (3 - i) : i;
        String segment = String(ip[ip_idx]);
        int charCount = segment.length();
        int totalW = charCount * 32;
        int startX = (128 - totalW) / 2;
        for (int j = 0; j < charCount; j++) {
            renderer.drawSingleChar(i, segment.substring(j, j + 1), startX + (j * 32), 0);
        }
        if (i < 3) screens[i]->drawBox(120, 56, 4, 4);
        bool isTitleScreen = configManager.get().is_flipped ? (i == 3) : (i == 0);
        if (isTitleScreen) {
            // 폰트를 6x10으로 통일했다. 글자 높이가 6px→10px로 커지므로 baseline을
            //   y=7에서 y=10으로 내린다 (y=7이면 윗부분이 화면 밖으로 잘린다).
            screens[i]->setFont(STATUS_FONT);
            screens[i]->drawStr(0, 10, "SETTING ADDR");
        }
    }
    pushParallel();
}

void DisplayManager::showButtonHelp() {
    const char* titles[4] = {"BTN 1", "BTN 2", "BTN 3", "BTN 4"};
    char chimeStr[20], animStr[32], modeStr[12], fmtStr[16], fontStr[20];
    sprintf(chimeStr, "S:CHIME(%s)", configManager.get().chime_enabled ? "ON" : "OFF");
    sprintf(animStr, "S:ANIMATION MODE %d", configManager.get().anim_mode);
    sprintf(modeStr, "S:MODE(%s)", configManager.get().display_mode == CLOCK_MODE_HANGUL ? "HAN" : "NUM");
    sprintf(fmtStr, "L:12/24 (%s)", configManager.get().hour_format == HOUR_FORMAT_24H ? "24H" : "12H");
    const char* shorts[4] = {chimeStr, modeStr, animStr, "S:NEXT PAGE"};
    char flipStr[16];
    sprintf(flipStr, "L:FLIP (%s)", configManager.get().is_flipped ? "ON" : "OFF");
    sprintf(fontStr, "L:FONT CHANGE(%d)", configManager.get().font_slot);
    const char* longs[4]  = {flipStr,  fmtStr, fontStr,  "L:INVERT"};
    for (int i = 0; i < 4; i++) {
        screens[i]->clearBuffer();
        // 폰트를 6x10으로 통일했다. 14px→10px로 줄었으므로 3줄 baseline(y=15/35/55)은
        //   그대로 두어도 잘리지 않고 20px 간격 유지된다.
        screens[i]->setFont(STATUS_FONT);
        screens[i]->drawStr(0, 15, titles[i]);
        screens[i]->drawStr(0, 35, shorts[i]);
        screens[i]->drawStr(0, 55, longs[i]);
    }
    pushParallel();
}

void DisplayManager::showStatus(const String& msg) {
    U8G2* u8g2 = screens[0]; u8g2->clearBuffer(); u8g2->setFont(STATUS_FONT);
    u8g2->drawStr(0, 10, msg.c_str()); pushParallel();
}

// 하위 레벨 I2C 전송 콜백은 i2c_platform.cpp로 이관됨
