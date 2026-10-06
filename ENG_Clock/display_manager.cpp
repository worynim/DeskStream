// worynim@gmail.com
/**
 * @file display_manager.cpp
 * @brief 고수준 디스플레이 및 UI 스테이지 관리 클래스 구현
 * @details 4개 OLED 디스플레이 제어, 부저 피드백, UI 상태 전환 로직 구현
 * @note [SYNC] 원본: Hangeul_Clock/display_manager.cpp — 1줄(정각 고정)을 2줄 어절 레이아웃으로
 *       변경하고, 애니메이션 문자 정체성을 x → (line, x)로 확장
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

// === 화면 판정 규칙 (단일 진실원) ===
// [리뷰 §1.1/§2.2] 아래 두 식이 display_manager.cpp 안에서 7번 복제돼 있었고,
// 그 복제 때문에 refreshNow()가 정적 경로와 **반대**인 값을 넘겨
// 숫자 모드에서 BTN3 롱프레스 직후 "02 H"가 2줄로 잠깐 깨졌다.
// 정적 경로(updateAll)·애니메이션 경로·refreshNow가 **반드시 같은 함수**를 써야
// 글자 위치가 어긋나지 않는다. 새 경로를 만들 때 이 두 함수를 우회하지 않는다.

/** @brief 이 화면이 제목(날짜+요일 / AM·PM) 화면인가 */
static bool isTitleScreenOf(int idx) {
    return configManager.get().is_flipped ? (idx == 3) : (idx == 0);
}

/**
 * @brief 이 화면의 텍스트를 한 줄로 배치하는가
 * @details 숫자 모드("02 H")이고 제목 화면이 아닐 때만 한 줄이다.
 *          제목 화면은 날짜+요일이라 2줄 어절 레이아웃을 유지한다.
 */
static bool isSingleLineLayout(int idx) {
    // [리뷰 §2.4] ENG_Clock.ino의 isWord와 같은 식 — 여기 한 곳으로 모았다.
    const bool isWord = (configManager.get().display_mode == CLOCK_MODE_WORD) && renderer.isCacheLoaded();
    return !isWord && !isTitleScreenOf(idx);
}

/**
 * @brief 애니메이션 한 전환의 총 스텝 수
 * @details drawAnimPair()/drawAnimExit()의 이동량 나눗셈(`LINE_HEIGHT / 16`)과
 *          종료 판정(`currentStep > 16`), 스크롤의 퇴장 시점(`step < 16`)이
 *          전부 이 값에 의존한다. 마직에 이름 없이 16으로 흩어져 있었다 (리뷰 §2.3).
 * @note 웹 미리보드의 animStep 범위와 1:1 대응한다.
 */
static const int ANIM_STEPS = 16;

/**
 * @brief 글자를 그릴 밴드 (세로 자르기 범위)
 * @details [여백 수정 v5] 48×64 래스터가 이웃 줄 밴드로 새는 것을 막는다.
 *          1줄 레이아웃(baseY = 세로 중앙)은 화면 전체를 쓴다 — 큰 폰트(잉크 64px)도
 *          위가 잘리지 않는다. 2줄 레이아웃(baseY 0/32)은 자기 줄 밴드로 제한한다.
 * @note drawAnimPair()와 drawAnimExit()가 **같은 규칙**을 써야 등장/퇴장 밴드가 어긋나지 않는다.
 */
struct Band {
    int top;
    int h;
};
static Band bandFor(int baseY) {
    const bool oneLine = (baseY == (SCREEN_HEIGHT - LINE_HEIGHT) / 2);
    return { oneLine ? 0 : baseY, oneLine ? SCREEN_HEIGHT : LINE_HEIGHT };
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

void DisplayManager::setDateOrder(uint8_t order) {
    if (order > DATE_ORDER_MONTH_DAY) return;   // enum 범위 밖 값은 무시
    configManager.get().date_order = order;
    configManager.setDirty();
    setForceUpdate(true);
}

void DisplayManager::applyTimezone() {
    const char* tz = configManager.get().timezone;
    // [bug 2 수정 - 원인 2] configTime(0, 0, ...)을 쓰면 코어(esp32-hal-time.c)가
    //   마지막에 setTimeZone(-0, 0)을 호출해 setenv("TZ", "UTC0DST0")로
    //   **방금 설정한 TZ를 덮어쓴다**. GMT 오프셋 0을 POSIX TZ로 강제 변환하기
    //   때문에 어떤 타임존을 골라도 항상 UTC로 표시되었다
    //   (서울 16시에 7시로 나오는 증상의 원인).
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

    // [리뷰 §1.3 승계 / A-2④ 수정] 이 값은 (1) JSON 응답에 붙고 (2) "/fN/name.txt" 의
    //   **내용**으로 쓰인다. 경로는 리터럴이라 이름이 경로 조작을 일으키지 않으므로
    //   (config.h FONT_NAME_MAX_LEN 주석 참조) 상한은 **길이 폭주 방지** 목적이다.
    //   ⚠ 예전의 `>= 32`(실질 31바이트)는 "이름을 파일명으로도 쓴다"는 잘못된 전제에서
    //     나온 값이었고, 35바이트짜리 실제 폰트 이름을 **조용히** 거부해 슬롯이
    //     "Empty Slot"으로 보이게 했다.
    //   `"` `\` `/` 제어문자 금지는 그대로 둔다 — 이름은 사람이 읽는 값이고 여기 들어올
    //   이유가 없다 (경로 조작 방어가 아니라 **출력 위생** 목적이다).
    if (name.length() == 0 || name.length() > FONT_NAME_MAX_LEN) return false;
    for (size_t i = 0; i < name.length(); i++) {
        const char c = name[i];
        if (c == '"' || c == '\\' || c == '/' || c < 0x20) return false;
    }

    // 이미 그 슬롯의 이름이면 파일을 다시 쓰지 않는다 — 글리프 업로드가 38회 부른다.
    if (_slotNames[slot] == name) return true;

    const String path = "/f" + String(slot) + "/name.txt";
    File f = LittleFS.open(path, "w");
    if (!f) return false;      // 슬롯 폴더가 없거나 열 수 없다 — 캐시를 갱신하지 않고 알린다
    f.print(name);
    f.close();

    _slotNames[slot] = name;   // 드롭다운이 읽는 곳

    // 현재 슬롯의 이름표를 바꾼 경우에만 config까지 — 다른 슬롯을 고치는 것이
    //   지금 화면의 "Current font" 표시를 흔들면 안 된다.
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
    for (int i = 0; i < 4; i++) {
        // [리뷰 §1.1] 이전엔 isTitleScreen를 그대로 넘겨 updateAll()과 반대 규칙을 썼다.
        drawCenterText(i, lastTexts[i], isSingleLineLayout(i));
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
// @param singleLine [수정할 사항 1] true면 "어절 2개 이상 → 2줄" 규칙을 건너뛰고 한 줄로 배치한다.
//        숫자 모드의 "02 H" 전용. (이전의 미사용 `centered` 인자 자리를 교체했다)
void DisplayManager::drawCenterText(int idx, const String& text, bool singleLine) {
    U8G2* u8g2 = screens[idx];
    u8g2->clearBuffer();

    CharData chars[LAYOUT_MAX_CHARS]; int count = 0;
    bool complete = renderer.getCharData(text, chars, count, singleLine);

    // 영어 시각 표현은 전수 검증에서 잘림이 없음이 증명되어 있다 (test_layout).
    // 여기서는 커스텀 텍스트 등 예상 밖 입력에 대한 방어 로깅만 수행한다.
    if (!complete) {
        Serial.printf("[LAYOUT] Truncated (%d/%d chars): %s\n", count, LAYOUT_MAX_CHARS, text.c_str());
    }

    // [여백 수정 v5 — PLAN §6.14] 2줄 레이아웃이면 각 줄 밴드로 잘라 그린다 —
    // 48×64 래스터가 이웃 줄 밴드로 새는 것을 막는다 (구 32px 래스터는 밴드 안이라 결과 동일).
    // 1줄 레이아웃은 화면 전체를 쓴다 — 큰 폰트(잉크 64px)도 위가 잘리지 않는다.
    bool multiLine = false;
    for (int i = 0; i < count; i++) {
        if (chars[i].line == 1) { multiLine = true; break; }
    }
    for (int i = 0; i < count; i++) {
        if (multiLine) {
            renderer.drawSingleCharClipped(idx, chars[i].c, chars[i].x, chars[i].y,
                                           chars[i].y, LINE_HEIGHT);
        } else {
            renderer.drawSingleChar(idx, chars[i].c, chars[i].x, chars[i].y);
        }
    }
    if (isTitleScreenOf(idx) && configManager.get().chime_enabled) u8g2->drawXBM(0, 0, 8, 8, bell_icon);
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
                drawCenterText(i, texts[i], isSingleLineLayout(i));
                lastTexts[i] = texts[i];
            }
        }
        pushParallel();
        return;
    }

    // [Step 3.1] 비차단 애니메이션 상태 초기화

    // 이전 전환이 끝나기 전에 새 전환이 시작될 수 있다(매초 호출되기 때문).
    // 이번에 변경되지 않은 화면이 이전 전환에서 애니메이션 중이었다면, 아래 루프에서
    // changed가 false로 덮어써져 렌더링이 완전히 멈추고 중간 프레임이 화면에 남는다
    // (ghost column). 그 잔상을 먼저 정착된 최종 상태로 지운다.
    // [한글판 이식] Hangeul_Clock/display_manager.cpp:331-340. 한글판의 눈 조립 모드에
    //   필요한 transitionId/maxStep는 이쪽에 없으므로 이식하지 않았다.
    for (int i = 0; i < 4; i++) {
        if (_animState.screens[i].changed && !changed[i]) {
            // 정적 경로·애니메이션 경로와 **같은 배치 규칙**을 써야 글자 위치가 어긋나지 않는다.
            drawCenterText(i, lastTexts[i], isSingleLineLayout(i));
        }
    }

    _animState.active = true;
    _animState.currentStep = 0;
    _animState.lastUpdateMs = 0; // 즉시 첫 틱 실행

    for (int i = 0; i < 4; i++) {
        _animState.screens[i].changed = changed[i];
        if (changed[i]) {
            // [수정할 사항 1] 타이틀 화면(날짜+요일 / AM·PM)은 2줄 규칙을 유지하고,
            // 나머지 세 화면은 숫자 모드일 때 한 줄로 배치한다. 애니메이션 경로와
            // 정적 경로가 같은 규칙을 써야 글자 위치가 어긋나지 않는다.
            const bool singleLine = isSingleLineLayout(i);
            renderer.getCharData(lastTexts[i], _animState.screens[i].oldChars,
                                 _animState.screens[i].oldCount, singleLine);
            renderer.getCharData(texts[i], _animState.screens[i].newChars,
                                 _animState.screens[i].newCount, singleLine);
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
    if (now - _animState.lastUpdateMs < ANIMATION_STEP_DELAY_MS) {
        // 대기 시간 동안 yield 콜백 실행하여 타 서비스 기회 제공
        if (on_yield_callback) on_yield_callback();
        return;
    }

    _animState.lastUpdateMs = now;
    _animState.currentStep++;

    if (_animState.currentStep > ANIM_STEPS) {
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

// === 애니메이션 프레임 헬퍼 ===
// renderAnimFrame()이 50 LOC 제한을 넘지 않도록, 그리기 로직을 함수로 분리한다.
// 두 함수 모두 "줄 Y 오프셋(baseY)"을 받아 2줄 레이아웃에 대응한다.

/**
 * @brief 한 위치의 새 글자와 그 자리를 대체하는 이전 글자를 함께 그린다
 * @param baseY 해당 줄의 Y 오프셋 (line * LINE_HEIGHT)
 * @param oc    이전 글자. 빈 문자열이면 첫 등장(퇴장 대상 없음).
 *
 * @details [버그 3] 스크롤이 SCREEN_HEIGHT(64px)를 이동해서, 2줄일 때 아래줄 글자가 위로
 *          올라가며 위줄 글자와 겹쳤다 (예: TWO가 TWENTY 위를 지나간다).
 *          두 가지를 함께 고친다 — (1) 이동량을 한 줄 높이로 줄이고
 *          (2) Renderer::drawSingleCharClipped()로 자기 줄 밴드로 잘라 그린다.
 *          (1)만으로는 이전 글자가 y ∈ [0,32) 로 나가 위줄 밴드를 그대로 침범한다.
 * @note  웹 미리보기 drawByLine() + `off = animStep * (LINE_HEIGHT / 16)` 과 1:1 대응한다.
 */
static void drawAnimPair(int i, uint8_t mode, int step,
                         const String& nc, int nx, int baseY, const String& oc) {
    // 스크롤은 한 줄 높이(LINE_HEIGHT) 전체를 이동시키는 것으로 본다.
    // 이전 글자가 위로 -LINE_HEIGHT, 새 글자가 아래에서 +LINE_HEIGHT → 0으로 이동한다.
    const int off = step * (LINE_HEIGHT / ANIM_STEPS);
    const Band band = bandFor(baseY);
    switch (mode) {
        case ANIMATION_TYPE_SCROLL_UP:
            if (step < ANIM_STEPS && oc != "") renderer.drawSingleCharClipped(i, oc, nx, baseY - off, band.top, band.h);
            renderer.drawSingleCharClipped(i, nc, nx, baseY + LINE_HEIGHT - off, band.top, band.h);
            break;
        case ANIMATION_TYPE_SCROLL_DOWN:
            if (step < ANIM_STEPS && oc != "") renderer.drawSingleCharClipped(i, oc, nx, baseY + off, band.top, band.h);
            renderer.drawSingleCharClipped(i, nc, nx, baseY - LINE_HEIGHT + off, band.top, band.h);
            break;
        case ANIMATION_TYPE_VERTICAL_FLIP:
            // 한 줄 높이를 기준으로 0 ↔ LINE_HEIGHT로 스케일한다.
            // (원본의 하드코딩 64는 1줄 기준이었고, 2줄에서는 상단 줄이 넘친다)
            if (step <= 8) { if (oc != "") renderer.drawScaledChar(i, oc, nx, ((8 - step) * LINE_HEIGHT) / 8, baseY); }
            else { renderer.drawScaledChar(i, nc, nx, ((step - 8) * LINE_HEIGHT) / 8, baseY); }
            break;
        case ANIMATION_TYPE_DITHERED_FADE:
            if (step <= 8) { if (oc != "") renderer.drawDitheredChar(i, oc, nx, 16 - (step * 2), baseY, band.top, band.h); }
            else { renderer.drawDitheredChar(i, nc, nx, (step - 8) * 2, baseY, band.top, band.h); }
            break;
        case ANIMATION_TYPE_ZOOM:
            if (step <= 8) { if (oc != "") renderer.drawZoomedChar(i, oc, nx, ((8 - step) * 100) / 8, baseY, band.top, band.h); }
            else {
                int sc = (step <= 12) ? ((step - 8) * 150 / 4) : (150 - (step - 12) * 50 / 4);
                renderer.drawZoomedChar(i, nc, nx, sc, baseY, band.top, band.h);
            }
            break;
        default: break;
    }
}

/** @brief 새 텍스트에 동일 위치가 없어 사라진 이전 글자를 퇴장시킨다. */
static void drawAnimExit(int i, uint8_t mode, int step,
                         const String& oc, int ox, int baseY) {
    const int off = step * (LINE_HEIGHT / ANIM_STEPS);
    // drawAnimPair와 같은 밴드 규칙 (bandFor 단일 진실원)
    const Band band = bandFor(baseY);
    switch (mode) {
        case ANIMATION_TYPE_SCROLL_UP:   if (step < ANIM_STEPS) renderer.drawSingleCharClipped(i, oc, ox, baseY - off, band.top, band.h); break;
        case ANIMATION_TYPE_SCROLL_DOWN: if (step < ANIM_STEPS) renderer.drawSingleCharClipped(i, oc, ox, baseY + off, band.top, band.h); break;
        case ANIMATION_TYPE_VERTICAL_FLIP: if (step <= 8) renderer.drawScaledChar(i, oc, ox, ((8 - step) * LINE_HEIGHT) / 8, baseY); break;
        case ANIMATION_TYPE_DITHERED_FADE: if (step <= 8) renderer.drawDitheredChar(i, oc, ox, 16 - (step * 2), baseY, band.top, band.h); break;
        case ANIMATION_TYPE_ZOOM:          if (step <= 8) renderer.drawZoomedChar(i, oc, ox, ((8 - step) * 100) / 8, baseY, band.top, band.h); break;
        default: break;
    }
}

void DisplayManager::renderAnimFrame(int i, int step) {
    ScreenAnimData& sd = _animState.screens[i];
    screens[i]->clearBuffer();
    const uint8_t mode = configManager.get().anim_mode;

    for (int j = 0; j < sd.newCount; j++) {
        const uint8_t nl = sd.newChars[j].line;
        const int nx = sd.newChars[j].x;
        const int baseY = sd.newChars[j].y;   // layoutWrap()이 세로 중앙까지 계산해 준다

        // 문자 정체성 = (줄, x). 2줄에서 x만으로는 상·하단 줄이 구분되지 않는다.
        String oc = ""; bool isStatic = false;
        for (int k = 0; k < sd.oldCount; k++) {
            if (sd.oldChars[k].line == nl && sd.oldChars[k].x == nx) {
                oc = sd.oldChars[k].c;
                if (oc == sd.newChars[j].c) isStatic = true;
                break;
            }
        }

        if (isStatic) renderer.drawSingleChar(i, sd.newChars[j].c, nx, baseY);
        else          drawAnimPair(i, mode, step, sd.newChars[j].c, nx, baseY, oc);
    }

    for (int k = 0; k < sd.oldCount; k++) {
        const uint8_t ol = sd.oldChars[k].line;
        const int ox = sd.oldChars[k].x;
        bool hasSamePos = false;
        for (int j = 0; j < sd.newCount; j++) {
            if (sd.newChars[j].line == ol && sd.newChars[j].x == ox) { hasSamePos = true; break; }
        }
        if (hasSamePos) continue;
        drawAnimExit(i, mode, step, sd.oldChars[k].c, ox, sd.oldChars[k].y);
    }

    if (isTitleScreenOf(i) && configManager.get().chime_enabled) screens[i]->drawXBM(0, 0, 8, 8, bell_icon);
}

/**
 * @brief 한 화면의 U8g2 버퍼를 셰도 버퍼와 비교해, 바뀐 페이지 구간만 콜백에 넘긴다
 * @details [리뷰 §2.1] 이 함수는 HW 버스(0·1)와 SW 버스(2·3)가 **똑같이** 도는 부분이다.
 *          이전엔 15줄짜리 블록이 두 번 복제돼 있어서, 한쪽만 고치면
 *          화면 0·1과 화면 2·3의 전송 범위가 어긋나는 형태였다.
 *          diff는 순수 계산이고 전송은 버스마다 다르므로, 여기서는 diff만 하고
 *          실제 전송은 onPageDirty에 위임한다 (side effect를 경계에 격리).
 * @param onPageDirty (screen, screenIdx, page, firstTile, tileCount) — 해당 페이지가 더러울 때 한 번 호출
 * @note 셰도 버퍼는 **비교 직후** 갱신한다. 전송은 HW 태스크가 나중에 하므로,
 *      여기서 갱신하지 않으면 같은 프레임이 두 번 전송된다.
 * @note screen를 넘기는 이유: 이 함수들은 파일 스코프 정적 함수라 DisplayManager의
 *      private 멤버 `screens`에 접근할 수 없다. 대상 포인터를 넘겨야 SW 콜백이
 *      updateDisplayArea()를 부를 수 있다.
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

static bool g_any_hw_dirty = false;   // HW 전송 알림 여부 — onHwPageDirty()가 갱신한다

static void onHwPageDirty(U8G2* screen, int screenIdx, int page, int firstTile, int tileCount) {
    (void)screen;   // HW는 I2C 태스크로 미룬다 — U8G2 포인터가 필요 없다
    i2cPlatform.preparePageUpdate(screenIdx, page, firstTile, tileCount);
    g_any_hw_dirty = true;
}

static void onSwPageDirty(U8G2* screen, int screenIdx, int page, int firstTile, int tileCount) {
    (void)screenIdx;
    screen->updateDisplayArea(firstTile, page, tileCount, 1);
}

void DisplayManager::pushParallel() {
    i2cPlatform.waitForSync(I2C_SYNC_TIMEOUT_MS);

    g_any_hw_dirty = false;
    for (int s = 0; s < 2; s++) diffAndForEachPage(s, screens[s], onHwPageDirty);
    // 새 diff뿐 아니라 **이전 회차에 전송에 실패해 남은 페이지**가 있어도 알린다.
    //   실패한 페이지는 셰도 버퍼가 이미 갱신된 상태라 위 diff가 잡지 못하므로,
    //   이 조건이 없으면 남은 dirty 비트가 다음 글자 변경 때까지 방치된다.
    if (g_any_hw_dirty || i2cPlatform.hasPendingHwUpdate()) { i2cPlatform.notifyTransmission(); }

    for (int s = 2; s < 4; s++) diffAndForEachPage(s, screens[s], onSwPageDirty);
}

// i2c_hw_task 구현은 i2c_platform.cpp로 이관됨

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
    // 원본은 한글 32px 글리프 폭을 하드코딩했다. 기하 테이블에서 읽어 폰트 교체에도 대응한다.
    // [v5 — PLAN §6.14] 도트 앵커는 실제 숫자가 쓰는 기하를 따른다 — 캐시에 폰트가 있으면
    //   그 글리프의 기하, 없으면 기본 기하(drawSingleChar의 폴백과 같은 선택).
    const CellGeometry* dg = nullptr;
    for (int i = 0; i < 4 && !dg; i++) {
        String seg = String(ip[configManager.get().is_flipped ? (3 - i) : i]);
        for (int j = 0; j < seg.length() && !dg; j++) dg = renderer.geometryOf(seg.substring(j, j + 1));
    }
    const CellGeometry& g = dg ? *dg : defaultGeometry();
    const int pitch = g.glyphW;

    // [bug fix] 도트는 마지막 숫자 옆("192."처럼)에, 숫자 하단에 맞춰 찍는다.
    // 이전에는 도트를 화면 가로 중앙 아래에 내려 숫자와 분리돼 보였다.
    // 묶음 폭 = 숫자들 + 간격 + 도트 → 이 묶음을 가로 중앙에 놓는다.
    // [여백 수정 v5 — PLAN §6.14] drawSingleChar의 y_offset은 **밴드 상단**이다(래스터를
    //   밴드 중앙에 놓는다). 1줄 밴드 상단(=LINE_HEIGHT/2)을 넘기면 어떤 기하든 래스터가
    //   화면 세로 중앙에 온다 — 구 형식(glyphH=32)은 y=16, 신형 384B(glyphH=64)는 y=0.
    //   도트는 래스터 하단에 맞춘다: 구 형식 = digitY + glyphH − IP_DOT_SIZE와 같은 값(44).
    const int digitY = LINE_HEIGHT / 2;                                   // 1줄 밴드 상단
    const int dotY   = (SCREEN_HEIGHT + g.glyphH) / 2 - IP_DOT_SIZE;      // 래스터 하단 정렬

    for (int i = 0; i < 4; i++) {
        screens[i]->clearBuffer();
        int ip_idx = configManager.get().is_flipped ? (3 - i) : i;
        String segment = String(ip[ip_idx]);
        int charCount = segment.length();
        bool hasDot = (i < 3);
        int blockW = charCount * pitch + (hasDot ? IP_DOT_GAP + IP_DOT_SIZE : 0);
        int startX = (SCREEN_WIDTH - blockW) / 2;   // 가로 중앙 (도트 포함)
        int dotX   = startX + charCount * pitch + IP_DOT_GAP;
        for (int j = 0; j < charCount; j++) {
            renderer.drawSingleChar(i, segment.substring(j, j + 1), startX + (j * pitch), digitY);
        }
        if (hasDot) screens[i]->drawBox(dotX, dotY, IP_DOT_SIZE, IP_DOT_SIZE);
        if (isTitleScreenOf(i)) {
            screens[i]->setFont(u8g2_font_4x6_tf);
            screens[i]->drawStr(0, 7, "SETTING ADDR");
        }
    }
    pushParallel();
}

void DisplayManager::showButtonHelp() {
    const char* titles[4] = {"BTN 1", "BTN 2", "BTN 3", "BTN 4"};
    // modeStr은 "S:MODE(WORD)"(12자) + NUL = 13바이트가 필요하다. (원본 "HAN"은 11바이트)
    char chimeStr[20], animStr[32], modeStr[16], fmtStr[16], fontStr[20];
    sprintf(chimeStr, "S:CHIME(%s)", configManager.get().chime_enabled ? "ON" : "OFF");
    sprintf(animStr, "S:ANIMATION MODE %d", configManager.get().anim_mode);
    sprintf(modeStr, "S:MODE(%s)", configManager.get().display_mode == CLOCK_MODE_WORD ? "WORD" : "NUM");
    sprintf(fmtStr, "L:12/24 (%s)", configManager.get().hour_format == HOUR_FORMAT_24H ? "24H" : "12H");
    const char* shorts[4] = {chimeStr, modeStr, animStr, "S:NEXT PAGE"};
    char flipStr[16];
    sprintf(flipStr, "L:FLIP (%s)", configManager.get().is_flipped ? "ON" : "OFF");
    sprintf(fontStr, "L:FONT CHANGE(%d)", configManager.get().font_slot);
    const char* longs[4]  = {flipStr,  fmtStr, fontStr,  "L:INVERT"};
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
