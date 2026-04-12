#include "display_manager.h"
#include "LittleFS.h"
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

DisplayManager display;

// 시보 표시용 종 모양 아이콘 (8x8)
static const uint8_t bell_icon[] = { 0x18, 0x3C, 0x3C, 0x3C, 0xFF, 0xDB, 0x18, 0x00 };

// 하위 레벨 I2C 콜백 및 설정은 i2c_platform.cpp로 이관됨

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
        screens[i]->setBitmapMode(1);
        screens[i]->clearBuffer();
        lastTexts[i] = "";
    }
    pushParallel();

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    buzzerTimer = xTimerCreate("BuzzerTimer", pdMS_TO_TICKS(50), pdFALSE, (void*)0, buzzerTimerCallback);
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

void DisplayManager::setFontName(String name) {
    configManager.get().font_name = name;
    configManager.setDirty();
    setForceUpdate(true);
}

// saveConfig()은 ConfigManager로 대체됨

void DisplayManager::loadBitmapCache() {
    renderer.loadBitmapCache();
}

// findChar, getHexKey 로직은 Renderer로 이동됨

void DisplayManager::clearAll() {
    for (int i = 0; i < NUM_SCREENS; i++) {
        screens[i]->clearBuffer();
        lastTexts[i] = "";
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
void DisplayManager::drawCenterText(int idx, const String& text, bool centered) {
    U8G2* u8g2 = screens[idx];
    u8g2->clearBuffer();
    CharData chars[8]; int count;
    renderer.getCharData(text, chars, count, centered || (text == "정각"));
    for (int i = 0; i < count; i++) {
        renderer.drawSingleChar(idx, chars[i].c, chars[i].x, 0);
    }
    bool isTitleScreen = configManager.get().is_flipped ? (idx == 3) : (idx == 0);
    if (isTitleScreen && configManager.get().chime_enabled) u8g2->drawXBM(0, 0, 8, 8, bell_icon);
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

    CharData oldChars[4][8], newChars[4][8];
    int oldCount[4], newCount[4];
    for (int i = 0; i < 4; i++) {
        if (changed[i]) {
            bool isCentered = configManager.get().is_flipped ? (i == 3) : (i == 0);
            // [Step 5.4] '정각'일 경우 화면 성격과 무관하게 항상 중앙 정렬 수행 (사용자 요청)
            bool forceCenter = (texts[i] == "정각");
            
            renderer.getCharData(lastTexts[i], oldChars[i], oldCount[i], isCentered || (lastTexts[i] == "정각"));
            renderer.getCharData(texts[i], newChars[i], newCount[i], isCentered || forceCenter);
        }
    }

    for (int step = 0; step <= 16; step++) {
        // [Step 4.1] 시계 모드가 아니면 즉시 탈출 (버튼 입력에 의한 Race Condition 방지)
        if (uiStage != 0) return;
        for (int i = 0; i < 4; i++) {
            if (!changed[i]) continue;
            screens[i]->clearBuffer();
            
            for (int j = 0; j < newCount[i]; j++) {
                String nc = newChars[i][j].c; int nx = newChars[i][j].x;
                String oc = ""; bool isStatic = false;
                for (int k = 0; k < oldCount[i]; k++) {
                    if (oldChars[i][k].x == nx) {
                        oc = oldChars[i][k].c;
                        if (oc == nc) isStatic = true;
                        break;
                    }
                }

                if (isStatic) {
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

            for (int k = 0; k < oldCount[i]; k++) {
                int ox = oldChars[i][k].x; String oc = oldChars[i][k].c;
                bool stillHasPos = false;
                for (int j = 0; j < newCount[i]; j++) { if (newChars[i][j].x == ox) { stillHasPos = true; break; } }
                if (stillHasPos) continue;

                switch (configManager.get().anim_mode) {
                    case ANIMATION_TYPE_SCROLL_UP:   if (step < 16) renderer.drawSingleChar(i, oc, ox, -(step * 4)); break;
                    case ANIMATION_TYPE_SCROLL_DOWN: if (step < 16) renderer.drawSingleChar(i, oc, ox, (step * 4)); break;
                    case ANIMATION_TYPE_VERTICAL_FLIP: if (step <= 8) renderer.drawScaledChar(i, oc, ox, ((8 - step) * 64) / 8); break;
                    case ANIMATION_TYPE_DITHERED_FADE: if (step <= 8) renderer.drawDitheredChar(i, oc, ox, 16 - (step * 2)); break;
                    case ANIMATION_TYPE_ZOOM:          if (step <= 8) renderer.drawZoomedChar(i, oc, ox, ((8 - step) * 100) / 8); break;
                }
            }

            bool isIconScreen = configManager.get().is_flipped ? (i == 3) : (i == 0);
            if (isIconScreen && configManager.get().chime_enabled) screens[i]->drawXBM(0, 0, 8, 8, bell_icon);
        }
        pushParallel();
        if (step < 16) {
            unsigned long startDelay = millis();
            while(millis() - startDelay < ANIMATION_STEP_DELAY_MS) {
                if (on_yield_callback) on_yield_callback();
                delay(1);
            }
        }
    }
    for (int i = 0; i < 4; i++) if (changed[i]) lastTexts[i] = texts[i];
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
            screens[i]->setFont(u8g2_font_4x6_tf);
            screens[i]->drawStr(0, 7, "SETTING ADDR");
        }
    }
    pushParallel();
}

void DisplayManager::showButtonHelp() {
    const char* titles[4] = {"BTN 1", "BTN 2", "BTN 3", "BTN 4"};
    char chimeStr[20], animStr[32];
    sprintf(chimeStr, "S:CHIME(%s)", configManager.get().chime_enabled ? "ON" : "OFF");
    sprintf(animStr, "S:ANIMATION MODE %d", configManager.get().anim_mode);
    const char* shorts[4] = {chimeStr, "S:NUM <> HAN", animStr, "S:NEXT PAGE"};
    char flipStr[16];
    sprintf(flipStr, "L:FLIP (%s)", configManager.get().is_flipped ? "ON" : "OFF");
    const char* longs[4]  = {flipStr,  "L:12/24", "L:-",  "L:-"};
    for (int i = 0; i < 4; i++) {
        screens[i]->clearBuffer();
        screens[i]->setFont(u8g2_font_7x14_tf);
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
