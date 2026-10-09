// worynim@gmail.com
/**
 * @file display_manager.cpp
 * @brief 고수준 디스플레이 및 UI 스테이지 관리 클래스 구현
 * @details 4개 OLED 디스플레이 제어, 부저 피드백, UI 상태 전환 로직 구현
 * @note [SYNC] ENG_Clock/display_manager.cpp — applyTimezone() 추가
 */
#include "display_manager.h"
#include "config.h"          // FONT_SLOT_COUNT
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

#if HAS_BUZZER
static void buzzerTimerCallback(TimerHandle_t xTimer) {
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
}
#endif

// DisplayManager 메서드 구현
// [BB v1.0.0] 4화면 전부 SSD1315 드라이버 + 커스텀 byte_cb(4버스 동시 BitBang).
//   핀 번호는 U8X8_PIN_NONE으로 둔다 — U8g2가 핀을 만지지 않고, 우리 콜백이
//   화면 인덱스로 버스를 고른다. (U8g2는 콜백에 버스 번호를 넘겨주지 않으므로
//   화면마다 byte_cb를 따로 등록하는 수밖에 없다)
DisplayManager::DisplayManager() : 
    u8g2_1(U8G2_R2, 255, 255, U8X8_PIN_NONE), u8g2_2(U8G2_R2, 255, 255, U8X8_PIN_NONE),
    u8g2_3(U8G2_R2, 255, 255, U8X8_PIN_NONE), u8g2_4(U8G2_R2, 255, 255, U8X8_PIN_NONE) {
    screens[0] = &u8g2_1; screens[1] = &u8g2_2; screens[2] = &u8g2_3; screens[3] = &u8g2_4;
}

void DisplayManager::begin() {
    for (int i = 0; i < NUM_SCREENS; i++) screens[i]->getU8g2()->tile_buf_ptr = u8g2_buffers[i];
    
    // 1. I2C 플랫폼 초기화 (버스 진단 → 동시 전송 드라이버 → 워커 태스크)
    i2cPlatform.begin();
    
    // 2. 화면별 byte_cb 등록 후 U8g2 초기화 시퀀스 전송
    //    ⚠ 주소는 전부 0x3C다 — setI2CAddress()를 부르지 않는다.
    //    화면 i가 쓸 **버스**는 BB_BUS_OF_SCREEN(i)가 정한다 —
    //    좌→우 배치를 뒤집으려면 config.h의 BB_SCREEN_ORDER_REVERSED만 바꾼다.
    static uint8_t (*const kBbCb[NUM_SCREENS])(u8x8_t*, uint8_t, uint8_t, void*) = {
        u8x8_byte_bb_0, u8x8_byte_bb_1, u8x8_byte_bb_2, u8x8_byte_bb_3
    };
    for (int i = 0; i < NUM_SCREENS; i++) {
        screens[i]->getU8x8()->byte_cb = kBbCb[BB_BUS_OF_SCREEN(i)];
        screens[i]->begin();
    }
    
    // 3. 렌더러 초기화
    renderer.setScreens(screens);

    // 4. 설정 매니저 초기화 및 로드
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

#if HAS_BUZZER
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    buzzerTimer = xTimerCreate("BuzzerTimer", pdMS_TO_TICKS(50), pdFALSE, (void*)0, buzzerTimerCallback);
#endif

    // 슬롯 이름 초기 캐싱
    for (int i = 0; i < FONT_SLOT_COUNT; i++) {
        String p = "/f" + String(i) + "/name.txt";
        if (LittleFS.exists(p)) {
            File f = LittleFS.open(p, "r");
            if (f) {
                _slotNames[i] = f.readString();
                f.close();
            }
        } else {
            _slotNames[i] = "Empty Slot";   // [A-4 수정] setFontSlot()의 빈 슬롯 표기와 같은 문자열
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
    if (slot >= FONT_SLOT_COUNT) return "";
    return _slotNames[slot];
}

/**
 * @brief 이름표를 **지정한 슬롯**에 쓴다 — 파일·캐시·(현재 슬롯이면) config까지
 * @details [A-2③④ 수정 — 2026-10-06, 중국어판 §12.13/§12.14 승계] 검증과 파일 쓰기를
 *          이 한 곳에 모은다. 검증을 두 곳에 두면 한쪽만 고쳐졌을 때 어느 쪽이 진짜
 *          규칙인지 알 수 없다.
 */
bool DisplayManager::setSlotName(uint8_t slot, const String& name) {
    if (slot >= FONT_SLOT_COUNT) return false;

    // [리뷰 §3.3 승계 / A-2④ 수정] 이 값은 (1) JSON 응답에 붙고 (2) "/fN/name.txt" 의
    //   **내용**으로 쓰인다. 경로는 리터럴이라 이름이 경로 조작을 일으키지 않으므로
    //   (config.h FONT_NAME_MAX_LEN 주석 참조) 상한은 **길이 폭주 방지** 목적이다.
    //   ⚠ 예전의 `>= 32`(실질 31바이트)는 "이름을 파일명으로도 쓴다"는 잘못된 전제에서
    //     나온 값이었고, 실제 폰트 이름을 **조용히** 거부해 슬롯이 "Empty Slot"으로 보이게 했다.
    //   `"` `\` `/` 제어문자 금지는 그대로 둔다 — 이름은 사람이 읽는 값이고 여기 들어올
    //   이유가 없다 (경로 조작 방어가 아니라 **출력 위생** 목적이다).
    if (name.length() == 0 || name.length() > FONT_NAME_MAX_LEN) return false;
    for (size_t i = 0; i < name.length(); i++) {
        const char c = name[i];
        if (c == '"' || c == '\\' || c == '/' || c < 0x20) return false;
    }

    // 이미 그 슬롯의 이름이면 파일을 다시 쓰지 않는다 — 글리프 업로드가 40회 부른다.
    if (_slotNames[slot] == name) return true;

    const String path = "/f" + String(slot) + "/name.txt";
    File f = LittleFS.open(path, "w");
    if (!f) return false;      // 슬롯 폴더가 없거나 열 수 없다 — 캐시를 갱신하지 않고 알린다
    f.print(name);
    f.close();

    _slotNames[slot] = name;   // 드롭다운이 읽는 곳

    // 현재 슬롯의 이름표를 바꾼 경우에만 config까지 — 다른 슬롯을 고치는 것이
    //   지금 화면의 "현재 적용 폰트" 표시를 흔들면 안 된다.
    if (configManager.get().font_slot == slot && configManager.get().font_name != name) {
        configManager.get().font_name = name;
        configManager.setDirty();
    }
    return true;
}

void DisplayManager::setFontName(const String& name) {
    setSlotName(configManager.get().font_slot, name);
}

void DisplayManager::setFontSlot(uint8_t slot) {
    if (slot >= FONT_SLOT_COUNT) slot = 0;
    
    // 이미 해당 슬롯이면 중복 로딩 방지
    if (configManager.get().font_slot == slot && renderer.isCacheLoaded()) {
        return;
    }
    
    configManager.get().font_slot = slot;
    
    // 새 슬롯의 이름 로드 — config(현재 이름)와 _slotNames(슬롯 목록용)를 **같이** 갱신한다.
    //   [A-3 수정] 예전엔 configManager.font_name만 갱신했으므로, 슬롯을 옮겨도
    //   /api/config가 내보내는 slotNames는 **부팅 시점에 읽은 값**으로 남아 있었다.
    String path = "/f" + String(slot) + "/name.txt";
    String name;
    if (LittleFS.exists(path)) {
        File f = LittleFS.open(path, "r");
        if (f) {
            name = f.readString();
            f.close();
        }
    }
    // 파일이 없거나(빈 슬롯) 열지 못했거나(내용 없음) — 셋을 같은 문자열로 좁힌다.
    //   [A-4 수정] 부팅 캐시의 빈 슬롯 표기와 **같은 문자열**이어야 한다.
    //   다른 이름이 섞이면 같은 상태가 두 이름으로 보인다.
    if (name.length() == 0) name = "Empty Slot";
    configManager.get().font_name = name;
    _slotNames[slot] = name;
    
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
    for (int i = 0; i < 4; i++) drawCenterText(i, lastTexts[i]);
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
    // 전송이 동기(pushParallel 안에서 끝난다)이므로 대기가 필요 없다 —
    //   diff와 전송이 같은 태스크에서 순서대로 돈다.
    u8g2_1.clearBuffer();
    u8g2_2.clearBuffer();
    u8g2_3.clearBuffer();
    u8g2_4.clearBuffer();
    
    // 섀도우 버퍼를 무효화하여 pushParallel()이 모든 0픽셀을 강제로 전송하게 함
    for(int i=0; i<NUM_SCREENS; i++) {
        i2cPlatform.invalidateShadow(i);
    }
    
    pushParallel();
    setForceUpdate(true); // 즉시 다음 프레임 그리기 예약
}

void DisplayManager::beep(int duration, int freq) {
#if HAS_BUZZER
    if (buzzerTimer == NULL) return;
    tone(BUZZER_PIN, freq);
    xTimerChangePeriod(buzzerTimer, pdMS_TO_TICKS(duration), 0);
    xTimerStart(buzzerTimer, 0);
#else
    // [BB v1.0.0] 부저 미연결 → 완전 무음.
    //   호출부(버튼 콜백·시보)는 그대로 두었다. 부저를 다시 달면 config.h의
    //   HAS_BUZZER를 1로 바꾸는 것만으로 되살아난다 (단, GPIO 7은 지금 OLED3 SDA다).
    (void)duration;
    (void)freq;
#endif
}

void DisplayManager::setYieldCallback(void (*cb)()) {
    on_yield_callback = cb;
}

// === 화면 판정 규칙 (단일 진실원) ===
// [리뷰 §2.2] 아래 식이 display_manager.cpp 안에 3번 복제돼 있었다.

/** @brief 이 화면이 제목(자릿수) 화면인가 — 뒤집기면 3번 화면이 자릿수다 */
static bool isTitleScreenOf(int idx) {
    return configManager.get().is_flipped ? (idx == 3) : (idx == 0);
}

// drawDitheredChar, drawZoomedChar, drawSingleChar, drawScaledChar, getCharData 로직 Renderer로 이관됨
void DisplayManager::drawChimeIcon(int idx) {
    if (isTitleScreenOf(idx) && configManager.get().chime_enabled) screens[idx]->drawXBM(0, 0, 8, 8, bell_icon);
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


/**
 * @brief 이 화면의 텍스트를 가운데 정렬하는가 (단일 진실원)
 * @details [리뷰 §2.2] 제목 화면 판정과 '정각' 강제 정렬이 4곳에 복제돼 있었다.
 *          drawCenterText() 안에 `|| (text == "정각")`가 있고, updateAll()의
 *          세 호출부는 drawCenterText()를 거치지 않고 getCharData()를 직접 불러
 *          같은 규칙을 각자 다시 구현했다. 한 곳이 바뀌면 나머지가 어긋난다.
 * @param idx 화면 번호 (is_flipped면 3번이 제목)
 * @param text 이 화면에 그릴 문자열
 */
static bool isCenteredText(int idx, const String& text) {
    return isTitleScreenOf(idx) || (text == "정각");
}

/**
 * @brief 한 화면을 문자열로 즉시 그린다 (애니메이션 없는 최종 상태)
 * @details 가운데 정렬 여부는 isCenteredText()가 정한다. 호출부가 그 값을
 *          따로 만들어 넘기면 규칙이 두 곳으로 갈라지므로, 여기서는 받지 않는다.
 */
void DisplayManager::drawCenterText(int idx, const String& text) {
    U8G2* u8g2 = screens[idx];
    u8g2->clearBuffer();
    CharData chars[LAYOUT_MAX_CHARS]; int count;
    renderer.getCharData(text, chars, count, isCenteredText(idx, text));
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
                drawCenterText(i, texts[i]);
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
        if (_animState.screens[i].changed && !changed[i]) drawCenterText(i, lastTexts[i]);
    }

    _animState.active = true;
    _animState.currentStep = 0;
    _animState.maxStep = isSnow ? ANIMATION_STEPS_SNOW : ANIMATION_STEPS_DEFAULT;
    _animState.transitionId++;      // 이번 전환에만 쓰이는 눈 시드
    _animState.lastUpdateMs = 0; // 즉시 첫 틱 실행

    for (int i = 0; i < 4; i++) {
        _animState.screens[i].changed = changed[i];
        if (changed[i]) {
            // 정적 경로·drawCenterText()와 **같은 규칙**을 쓴다 (isCenteredText 단일 진실원).
            renderer.getCharData(lastTexts[i], _animState.screens[i].oldChars,
                                 _animState.screens[i].oldCount, isCenteredText(i, lastTexts[i]));
            renderer.getCharData(texts[i], _animState.screens[i].newChars,
                                 _animState.screens[i].newCount, isCenteredText(i, texts[i]));
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
    uint8_t mode = configManager.get().anim_mode;

    // 눈 조립(픽셀 단위)과 분할 플랩(한 열 안에서 옛·새 글자가 함께 접힘)은
    // 기존 글자 단위 전환 경로로는 표현할 수 없어 각각 분리한다.
    if (mode == ANIMATION_TYPE_SNOW_ASSEMBLE) { renderSnowFrame(i, step); return; }
    if (mode == ANIMATION_TYPE_SPLIT_FLAP)   { renderFlapFrame(i, step);   return; }

    ScreenAnimData& sd = _animState.screens[i];
    screens[i]->clearBuffer();

    for (int j = 0; j < sd.newCount; j++) {
        String nc = sd.newChars[j].c; int nx = sd.newChars[j].x;
        int oi = findOldIndexAtX(sd, nx);
        String oc = (oi >= 0) ? sd.oldChars[oi].c : "";

        if (oc == nc) {
            renderer.drawSingleChar(i, nc, nx, 0);
        } else {
            switch (mode) {
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

        switch (mode) {
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

void DisplayManager::renderFlapFrame(int i, int step) {
    ScreenAnimData& sd = _animState.screens[i];
    uint8_t progress = (uint8_t)((int)step * ANIM_PROGRESS_FULL / (int)_animState.maxStep);
    screens[i]->clearBuffer();

    // 새 글자가 놓인 열: 옛 글자와 새 글자의 접힘을 한 번의 호출로 함께 그린다.
    for (int j = 0; j < sd.newCount; j++) {
        int nx = sd.newChars[j].x;
        int oi = findOldIndexAtX(sd, nx);
        String oc = (oi >= 0) ? sd.oldChars[oi].c : "";
        if (oc == sd.newChars[j].c) {
            renderer.drawSingleChar(i, sd.newChars[j].c, nx, 0); // 그대로인 글자는 움직이지 않는다
            continue;
        }
        renderer.drawFlapChar(i, oc, sd.newChars[j].c, nx, progress);
    }

    // 새 글자가 없는 열은 사라지는 글자이므로 접어 닫는다.
    for (int k = 0; k < sd.oldCount; k++) {
        int ox = sd.oldChars[k].x;
        if (findNewIndexAtX(sd, ox) >= 0) continue; // 새 글자가 있는 열은 위에서 함께 접었다
        renderer.drawFlapChar(i, sd.oldChars[k].c, "", ox, progress);
    }

    drawChimeIcon(i);
}

/**
 * @brief 한 화면의 U8g2 버퍼를 셰도 버퍼와 비교해, 바뀐 페이지 구간만 콜백에 넘긴다
 * @details [리뷰 §2.1] 이 함수는 4화면이 **똑같이** 도는 부분이다. 이전엔 15줄짜리
 *          블록이 버스마다 복제돼 있어서, 한쪽만 고치면 화면별 전송 범위가 어긋났다.
 *          diff는 순수 계산이고 전송은 버스마다 다르므로, 여기서는 diff만 하고
 *          실제 전송은 onPageDirty에 위임한다 (side effect를 경계에 격리).
 * @param onPageDirty (screen, screenIdx, page, firstTile, tileCount) — 해당 페이지가 더러울 때 한 번 호출
 * @note 셰도 버퍼는 **비교 직후** 갱신한다. 전송은 워커 태스크가 나중에 하므로,
 *      여기서 갱신하지 않으면 같은 프레임이 두 번 전송된다.
 * @note [BB v1.0.0] 4화면이 모두 자기 버스를 가진 BitBang이 되어 콜백이 하나로
 *      통일됐다. `screen` 인자는 남아 있지만 onHwPageDirty는 쓰지 않는다 —
 *      시그니처를 바꾸면 diff 루프와 콜백 쌍을 다시 맞춰야 해서 얻는 것이 없다.
 */
static void diffAndForEachPage(int screenIdx, U8G2* screen,
                               void (*onPageDirty)(U8G2* screen, int screenIdx, int page, int firstTile, int tileCount)) {
    uint8_t* buf = screen->getBufferPtr();
    for (int p = 0; p < PAGES_PER_SCREEN; p++) {
        bool page_dirty = false;
        int first_tile = -1, last_tile = -1;
        for (int t = 0; t < TILES_PER_PAGE; t++) {
            bool tile_dirty = false;
            for (int tx = 0; tx < 8; tx++) {
                int idx = p * SCREEN_WIDTH + t * 8 + tx;
                if (buf[idx] != i2cPlatform.getShadowData(screenIdx, idx)) {
                    tile_dirty = true;
                    i2cPlatform.setShadowData(screenIdx, idx, buf[idx]);
                }
            }
            if (tile_dirty) { if (first_tile == -1) first_tile = t; last_tile = t; page_dirty = true; }
        }
        if (page_dirty) onPageDirty(screen, screenIdx, p, first_tile, last_tile - first_tile + 1);
    }
}

/** diff 결과를 dirty 장부에 등록만 한다 (전송은 pushParallel이 모아서 한다) */
#if BB_FRAME_PATH != BB_FRAME_PATH_U8G2
/**
 * @name 캡처 후 재생 경로의 상태
 * @details [v1.0.6] **dirty 장부(I2CPlatform)를 거치지 않는다.** v1.0.4에서 장부를
 *          경유하도록 바꿨다가 화면이 깨졌고, 진단 4가 "장부 없이 diff 결과를 바로
 *          캡처·재생"하는 형태로 **4버스 동시 전송이 정상 동작함**을 실기에서 확인했다.
 *          그래서 검증된 그 형태로 구현한다.
 */
static uint8_t g_frameDirty[NUM_SCREENS];                              // 이번 프레임에 보낼 페이지
static uint8_t g_pageFirst[NUM_SCREENS][PAGES_PER_SCREEN];             // 페이지별 시작 타일
static uint8_t g_pageCount[NUM_SCREENS][PAGES_PER_SCREEN];             // 페이지별 타일 수

/**
 * 동시 재생 실패를 **한 번만** 시리얼에 알린다.
 * @details 동시 전송이 죽고 순차 폴백으로만 돌면 "화면은 정상인데 느리다"가 되는데,
 *          그 상태를 사용자가 즉시 알 수 있어야 한다 (v1.0.6에서 정확히 이 상태였다).
 */
static bool g_reportedFallback = false;

/** diff가 페이지를 찾을 때마다 그 구간을 기록한다 (전송은 뒤에서 페이지 단위로 모아서) */
static void onPageDirtyMark(U8G2* screen, int screenIdx, int page, int firstTile, int tileCount) {
    (void)screen;
    g_frameDirty[screenIdx]        |= (uint8_t)(1u << page);
    g_pageFirst[screenIdx][page]    = (uint8_t)firstTile;
    g_pageCount[screenIdx][page]    = (uint8_t)tileCount;
}
#endif

#if BB_FRAME_PATH == BB_FRAME_PATH_U8G2
/**
 * @brief U8g2 CAD에 전송을 맡기는 콜백 — 검증된 경로
 * @details diff 도중에 즉시 전송한다. 전송 바이트(명령 순서·열 오프셋·제어바이트)는
 *          전부 U8g2가 만들고, 우리는 버스만 골라 준다.
 */
static void onU8g2PageDirty(U8G2* screen, int screenIdx, int page, int firstTile, int tileCount) {
    (void)screenIdx;
    screen->updateDisplayArea(firstTile, page, tileCount, 1);
}
#endif

/**
 * @brief dirty 페이지만 골라 화면들을 전송한다
 *
 * @details 전송 방식은 `config.h`의 `BB_FRAME_PATH`가 고른다. **diff는 두 경로가 동일**하다.
 *
 *  - `BB_FRAME_PATH_CONCURRENT` (**기본**) — **캡처 후 재생**
 *    ① 각 화면의 `updateDisplayArea()`를 캡처 모드로 호출해 **U8g2가 만들 바이트를
 *       그대로** 받는다(명령 순서·열 오프셋·제어바이트 규칙이 전부 U8g2 책임).
 *    ② 캡처분을 트랜잭션 번호끼리 짝지어 **4버스에 동시 전송**한다.
 *    SCL을 공유하므로 참여 화면은 같은 구간(합집합)을 보낸다.
 *    동시 재생이 실패하면 그 페이지는 **검증된 순차 경로로 다시 보낸다**(안전망).
 *
 *    @note 이 형태는 진단 4의 B 단계에서 **실기 정상 동작이 확인**되었다.
 *
 *  - `BB_FRAME_PATH_U8G2` — U8g2에 전송까지 맡긴다. 가장 단순하고 확실하지만
 *    버스별 순차라 화면이 여러 개 바뀔 때 느리다.
 */
void DisplayManager::pushParallel() {
#if BB_FRAME_PATH == BB_FRAME_PATH_U8G2
    // 검증된 경로 — diff가 페이지를 찾는 즉시 그 화면을 전송한다.
    for (int s = 0; s < NUM_SCREENS; s++) diffAndForEachPage(s, screens[s], onU8g2PageDirty);
#else
    // 1) diff — 바뀐 페이지와 구간을 모은다 (셰도 버퍼도 여기서 갱신된다)
    for (int s = 0; s < NUM_SCREENS; s++) {
        g_frameDirty[s] = 0;
        diffAndForEachPage(s, screens[s], onPageDirtyMark);
    }

    // 2) 페이지마다 참여 화면을 모아 4버스에 동시 전송
    for (uint8_t p = 0; p < PAGES_PER_SCREEN; p++) {
        // ⚠ 두 인덱스 공간을 섞지 않는다.
        //    · g_frameDirty / g_pageFirst / g_pageCount / screens[] = **화면** 인덱스
        //    · _cap_buf / replayCaptured(busMask) / bitBang            = **버스** 인덱스
        //   화면 순서를 뒤집으면 screens[i]가 쓰는 버스는 BB_BUS_OF_SCREEN(i)다.
        //   [v1.0.7 수정] 예전엔 화면 마스크를 그대로 버스 마스크로 넘겨서,
        //   일부 화면만 바뀔 때 **빈 캡처 버퍼**를 재생하려다 실패하고 매번 순차
        //   경로로 되돌아갔다(화면은 정상, 속도만 느림). 전체가 바뀔 때(0x0F)만
        //   우연히 두 공간이 일치해 동시 전송이 됐다.
        uint8_t busMask = 0;
        uint8_t first[NUM_SCREENS];   // 화면 인덱스 기준
        uint8_t count[NUM_SCREENS];
        for (uint8_t s = 0; s < NUM_SCREENS; s++) {
            const bool d = (g_frameDirty[s] & (uint8_t)(1u << p)) != 0;
            first[s] = d ? g_pageFirst[s][p] : 0;
            count[s] = d ? g_pageCount[s][p] : 0;
            if (d) busMask |= (uint8_t)(1u << BB_BUS_OF_SCREEN(s));
        }
        if (busMask == 0) continue;

        // SCL을 공유하므로 참여 화면은 **같은 구간**을 보내야 한다 → 합집합(화면 기준)
        uint8_t uFirst = 0, uCount = 0;
        bbUnionColumnRange(first, count, NUM_SCREENS, &uFirst, &uCount);
        if (uCount == 0) continue;

        // ① U8g2가 만들 바이트를 캡처한다 (전송하지 않는다).
        //    각 화면의 캡처는 자기 버스 슬롯(_cap_buf[BB_BUS_OF_SCREEN(s)])에 담긴다.
        i2cPlatform.captureBegin();
        for (uint8_t s = 0; s < NUM_SCREENS; s++) {
            if (!(g_frameDirty[s] & (uint8_t)(1u << p))) continue;
            screens[s]->updateDisplayArea(uFirst, p, uCount, 1);
        }
        i2cPlatform.captureEnd();

        // ② 트랜잭션끼리 짝지어 4버스에 동시 전송 (마스크는 **버스** 기준)
        const uint8_t okBusMask = i2cPlatform.replayCaptured(busMask);

        // ③ 동시 재생이 실패한 버스가 있으면 **검증된 순차 경로**로 그 화면만 다시 보낸다.
        //    (한 화면의 배선 고장이 다른 화면까지 멈추지 않게 하는 안전망)
        if (okBusMask != busMask) {
            if (!g_reportedFallback) {
                g_reportedFallback = true;
                Serial.printf("[I2C] 동시 재생 실패 -> 순차 폴백 (bus=0x%02X ok=0x%02X) "
                              "err=%u\n", busMask, okBusMask,
                              (unsigned)i2cPlatform.errorCount());
            }
            for (uint8_t s = 0; s < NUM_SCREENS; s++) {
                if (!(g_frameDirty[s] & (uint8_t)(1u << p))) continue;
                const uint8_t b = (uint8_t)BB_BUS_OF_SCREEN(s);
                if (okBusMask & (uint8_t)(1u << b)) continue;
                screens[s]->updateDisplayArea(uFirst, p, uCount, 1);
            }
        }
    }

    // 오류가 누적되면 버스를 자가 복구한다 — SCL 펄스로 붙잡힌 SDA를 풀어준다.
    //   BitBang은 stuck-low를 ACK 성공으로 오독할 수 있어, 오류 카운터가 유일한 단서다.
    if (i2cPlatform.errorCount() > I2C_ERROR_THRESHOLD) i2cPlatform.recoverBus();
#endif
}

void DisplayManager::playStartupMelody() {
#if HAS_BUZZER
    int melody[] = {2093, 2637, 3136, 4186}; // Do-Mi-Sol-Do
    for (int i = 0; i < 4; i++) {
        tone(BUZZER_PIN, melody[i], 100);
        delay(120);
    }
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
#else
    // 부저 미연결 — 부팅 시에는 대신 짧게 쉬어 하드웨어가 안정될 시간을 준다.
    delay(200);
#endif
}

void DisplayManager::playChimeMelody() {
#if HAS_BUZZER
    // 경쾌한 '띠링~' 소리 (C7 -> G7)
    tone(BUZZER_PIN, 2093, 80);  // 도 (C7)
    delay(100);
    tone(BUZZER_PIN, 3136, 150); // 솔 (G7)
    delay(160);
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
#else
    // 부저 미연결 — 시보는 화면의 종 아이콘(drawChimeIcon)이 담당한다.
    //   웹 설정(chime_enabled)과 종 아이콘 로직은 그대로 살아 있다.
#endif
}


// IP 자릿수 표시 상수 — 원본에 128/120/56/4/4가 매직넘버로 박혀 있었다.
// [리뷰 §2.3] 자리 폭은 이미 GLYPH_CELL_W(32)로 정의돼 있으니 재사용한다.
//   점은 화면 오른쪽 아래에서 4px 안쪽에 4×4로 찍고, 세 화면(1~3번)에만 찍는다.
static const int IP_DOT_MARGIN  = 4;   // 화면 가장자리와 점 사이 여백
static const int IP_DOT_SIZE    = 4;   // 점 한 변의 픽셀 수
static const int IP_TITLE_BASELINE_Y = 10;   // 6x10 폰트의 baseline (y=7이면 윗부분이 잘린다)

void DisplayManager::showLargeIP(IPAddress ip) {
    const int dotX = SCREEN_WIDTH  - IP_DOT_MARGIN - IP_DOT_SIZE;  // 120
    const int dotY = SCREEN_HEIGHT - IP_DOT_MARGIN - IP_DOT_SIZE;  // 56
    for (int i = 0; i < 4; i++) {
        screens[i]->clearBuffer();
        int ip_idx = configManager.get().is_flipped ? (3 - i) : i;
        String segment = String(ip[ip_idx]);
        int charCount = segment.length();
        int startX = (SCREEN_WIDTH - charCount * GLYPH_CELL_W) / 2;
        for (int j = 0; j < charCount; j++) {
            renderer.drawSingleChar(i, segment.substring(j, j + 1), startX + j * GLYPH_CELL_W, 0);
        }
        if (i < 3) screens[i]->drawBox(dotX, dotY, IP_DOT_SIZE, IP_DOT_SIZE);
        if (isTitleScreenOf(i)) {
            // 폰트를 6x10으로 통일했다. 글자 높이가 6px→10px로 커지므로 baseline을
            //   y=7에서 y=10으로 내린다 (y=7이면 윗부분이 화면 밖으로 잘린다).
            screens[i]->setFont(STATUS_FONT);
            screens[i]->drawStr(0, IP_TITLE_BASELINE_Y, "SETTING ADDR");
        }
    }
    pushParallel();
}

void DisplayManager::showButtonHelp() {
    const char* titles[4] = {"BTN 1", "BTN 2", "BTN 3", "BTN 4"};
    char chimeStr[20], animStr[32], modeStr[12], fmtStr[16], fontStr[20];
    snprintf(chimeStr, sizeof(chimeStr), "S:CHIME(%s)", configManager.get().chime_enabled ? "ON" : "OFF");
    snprintf(animStr, sizeof(animStr), "S:ANIMATION MODE %d", configManager.get().anim_mode);
    snprintf(modeStr, sizeof(modeStr), "S:MODE(%s)", configManager.get().display_mode == CLOCK_MODE_HANGUL ? "HAN" : "NUM");
    snprintf(fmtStr, sizeof(fmtStr), "L:12/24 (%s)", configManager.get().hour_format == HOUR_FORMAT_24H ? "24H" : "12H");
    const char* shorts[4] = {chimeStr, modeStr, animStr, "S:NEXT PAGE"};
    char flipStr[16];
    snprintf(flipStr, sizeof(flipStr), "L:FLIP (%s)", configManager.get().is_flipped ? "ON" : "OFF");
    snprintf(fontStr, sizeof(fontStr), "L:FONT CHANGE(%d)", configManager.get().font_slot);
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
