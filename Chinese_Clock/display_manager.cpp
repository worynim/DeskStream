// worynim@gmail.com
/**
 * @file display_manager.cpp
 * @brief 고수준 디스플레이 및 UI 스테이지 관리 클래스 구현
 * @details 4개 OLED 디스플레이 제어, 부저 피드백, UI 상태 전환 로직 구현
 * @note [SYNC] 원본: ENG_Clock/display_manager.cpp
 *       변경: (1) 2줄 어절 레이아웃 제거 → 순수 모듈 layoutLine() 1줄 배치로 대체 (PLAN §6.4)
 *             (2) CharData(18자) → AnimCell(4자). 셀 문자열을 소유해 애니메이션 중 할당 0회
 *             (3) setDateOrder()·date_order 제거 (날짜 순서 개념 없음)
 *             (4) drawAnimPair/Exit의 baseY 제거 — 1줄이라 Y는 LINE_TOP_Y에서 이동량만 더한다
 *             (5) bandFor()·Band·drawSingleCharClipped() 제거 — 1줄의 밴드는 화면 전체라 자를 것이 없다
 */
#include "display_manager.h"
#include "config.h"          // FONT_SLOT_COUNT
#include "chinese_time_core.h"   // scriptOnlyGlyph() — 문자판 전용 글자 판정
#include "LittleFS.h"
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

DisplayManager display;

// 시보 표시용 종 모양 아이콘 (8x8)
static const uint8_t bell_icon[] = { 0x18, 0x3C, 0x3C, 0x3C, 0xFF, 0xDB, 0x18, 0x00 };

/** 퇴장 대상이 없을 때 쓰는 빈 문자열 (프레임마다 새로 만들지 않는다) */
static const String kNoOldChar = "";

// === 화면 판정 규칙 (단일 진실원) ===
// [ENG 리뷰 §1.1/§2.2 승계] 아래 규칙이 한 파일 안에 복제되면 정적 경로와 애니메이션 경로가
// 서로 다른 값을 넘겨 글자 위치가 어긋난다. 새 경로를 만들 때 이 함수를 우회하지 않는다.
// [중국어판 변경] ENG판의 isSingleLineLayout()는 사라졌다 — 중국어 표현은 항상 1줄이고
//   항상 중앙 정렬이므로 "숫자 모드일 때만 한 줄"이라는 분기 자체가 존재하지 않는다.

/** @brief 이 화면이 제목(요일 / 오전·오후) 화면인가 — 시보 종 아이콘의 위치 */
static bool isTitleScreenOf(int idx) {
    return configManager.get().is_flipped ? (idx == 3) : (idx == 0);
}

/**
 * @brief 애니메이션 한 전환의 총 스텝 수
 * @details drawAnimPair()/drawAnimExit()의 이동량 나눗셈(`LINE_HEIGHT / 16`)과
 *          종료 판정(`currentStep > 16`), 스크롤의 퇴장 시점(`step < 16`)이
 *          전부 이 값에 의존한다. (ENG 리뷰 §2.3)
 * @note 웹 미리보드의 animStep 범위와 1:1 대응한다.
 */
static const int ANIM_STEPS = 16;

static void buzzerTimerCallback(TimerHandle_t xTimer) {
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
}

// === 배치 보조 (순수 모듈 layout_engine 경계) ===

/**
 * @brief 문자열을 1줄 배치한다
 * @return 배치된 셀 수. 셀 초과로 거부되면 0
 * @details layoutLine()은 x 좌표를 채우기 **전에** false를 반환하므로 실패 시 count는 0이다.
 *          반쪽 표현("三点零")을 그리는 것보다 빈 화면이 낫다 — 시계가 거짓말하지 않는다.
 *          실제 시계 표현은 §3 전수표에서 4자 이내임이 확인되어 있으므로 거부 경로는
 *          커스텀 상태 문자열 같은 예상 밖 입력에만 걸린다.
 */
static int layoutFor(const String& text, LayoutChar* out) {
    const CellGeometry& geom = defaultGeometry();
    int count = 0;
    if (!layoutLine(text.c_str(), text.length(), geom, out, LAYOUT_MAX_CHARS,
                    SCREEN_WIDTH, count)) {
        Serial.printf("[LAYOUT] Rejected (>%d cells): %s\n", (int)geom.maxPerLine, text.c_str());
        return 0;
    }
    return count;
}

/**
 * @brief 셀 하나가 가리키는 바이트 구간을 String으로 만든다
 * @details LayoutChar::text는 **NUL로 종료되지 않는** 원본 문자열의 부분 문자열이다.
 *          원본 문자열 안에 NUL이 있을 수 있으므로 strlen()으로 재면 안 된다 —
 *          layoutLine()이 그 NUL 자리를 이미 "깨진 셀"(공백 1칸)로 소비했다.
 */
static String cellText(const LayoutChar& c) {
    return String(c.text, c.len);
}

/**
 * @brief 문자열을 애니메이션 스냅샷으로 옮긴다
 * @details 셀 문자열을 **값으로 복사**한다. LayoutChar가 가리키는 원본이
 *          스택 String이면 함수 밖에서 dangling되므로, AnimCell이 소유해야 한다.
 */
static void snapshotText(const String& text, AnimCell* out, int& outCount) {
    LayoutChar chars[LAYOUT_MAX_CHARS];
    const int n = layoutFor(text, chars);
    for (int i = 0; i < n; i++) {
        out[i].text = cellText(chars[i]);
        out[i].x = chars[i].x;
    }
    outCount = n;
}

/**
 * @brief 같은 화면 X 좌표의 셀을 찾는다
 * @return 찾으면 그 셀의 문자열 포인터, 없으면 nullptr
 * @details **1줄이라 좌표만으로 셀 정체성이 결정된다.** (ENG판은 (line, x) 쌍이었다)
 *          반환 포인터는 _animState 안에 있고 애니메이션이 끝날 때까지 재사용되므로 안전하다.
 */
static const String* findSamePosition(const AnimCell* cells, int count, const AnimCell& target) {
    for (int i = 0; i < count; i++) {
        if (cells[i].x == target.x) return &cells[i].text;
    }
    return nullptr;
}

/** @brief 셀 목록에 동일 좌표 셀이 있는지 — 빠진 이전 셀의 퇴장 판정용 */
static bool hasSamePosition(const AnimCell* cells, int count, const AnimCell& target) {
    return findSamePosition(cells, count, target) != nullptr;
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
            _slotNames[i] = "Empty Slot";   // setFontSlot()의 빈 슬롯 표기와 같은 문자열
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

/**
 * @brief 두 설정 필드의 조합을 3단계 표시 방식으로 되올린다 (역매핑)
 * @details 순환 순서가 여기서 정해진다: 简体 → 繁體 → 數字.
 */
uint8_t DisplayManager::presentation() const {
    if (configManager.get().display_mode == CLOCK_MODE_NUMERIC) return PRESENTATION_NUMERIC;
    return (configManager.get().script_type == SCRIPT_TRADITIONAL) ? PRESENTATION_TRADITIONAL
                                                                   : PRESENTATION_SIMPLIFIED;
}

/**
 * @brief 표시 방식을 바꾼다 — BTN2 short 순환과 웹 선택지가 **이 함수 하나**를 쓴다
 * @details 한자 모드로는 **그릴 글자가 있을 때만** 들어간다. 지금 슬롯의 폰트에
 *          그 문자판 전용 글자(번체면 點·時·兩 / 간체면 점·시·两)가 없으면 거부하고
 *          이유를 알린다. 없는 글자로 넘어가면 drawFallbackChar()가 **아무것도 그리지
 *          않고** 조용히 빈칸을 남겨 "숫자는 보이는데 时가 없다"가 된다.
 *          數字는 라틴 폴백 폰트로도 그려지므로 **폰트가 없어도 허용**한다
 *          (그렇지 않으면 폰트를 한 번도 올리지 않은 기기에서 탈출할 수 없다).
 *
 * @note 2026-10-05: 문자판을 슬롯과 분리한 뒤(BTN3 long = 슬롯 순환), 사용자가
 *       "간체·번체·숫자가 BTN2 short로 돌아가게"를 요구해 두 필드를 한 값으로 묶었다.
 *       슬롯은 여전히 건드리지 않는다 — 슬롯과 문자판은 서로 다른 축이다.
 */
void DisplayManager::setPresentation(uint8_t p) {
    // 방어적 가드 — NVS 오염 값이 들어올 여지를 막는다. 잘못된 값이면 **기존 상태를
    //   그대로 둔다**(조용히 기본값으로 바꾸지 않는다 — 사용자가 번체를 쓰고 있는데
    //   갑자기 간체로 돌아가는 것이 더 나쁘다).
    if (p >= PRESENTATION_COUNT) {
        Serial.printf("[CONFIG] Rejected invalid presentation %u\n", (unsigned)p);
        return;
    }
    if (p == presentation()) return;

    if (p != PRESENTATION_NUMERIC) {
        const uint8_t script = (p == PRESENTATION_TRADITIONAL) ? SCRIPT_TRADITIONAL : SCRIPT_SIMPLIFIED;
        const char* missing = missingGlyphFor(script);
        if (missing) {
            Serial.printf("[CONFIG] Slot %u lacks '%s' — keeping %s\n",
                          (unsigned)configManager.get().font_slot, missing,
                          (script == SCRIPT_TRADITIONAL) ? "script_type" : "presentation");
            // OLED 상태줄은 라틴 폰트라 한자를 못 그린다 — 한자 자체를 알리면 빈칸이 된다.
            showStatus(script == SCRIPT_TRADITIONAL ? "No Trad Font" : "No Simp Font");
            return;
        }
        configManager.get().script_type = script;
    }
    // 숫자 모드로 **나갈 때만** script_type을 그대로 둔다 — 한자로 돌아오면 되살아난다.
    configManager.get().display_mode = (p == PRESENTATION_NUMERIC) ? CLOCK_MODE_NUMERIC : CLOCK_MODE_WORD;
    configManager.setDirty();
    setForceUpdate(true);
}

/**
 * @brief 이 문자판에서만 쓰이는 글자 중 **지금 슬롯 폰트에 없는 첫째**를 찾는다
 * @return 없으면 nullptr
 * @details 슬롯을 바꾸지 않으므로 판정 기준은 "지금 캐시에 있는가" 하나뿐이다 —
 *          문자집합 상수도, FS 조회도, 매핑 테이블도 필요 없다.
 * @note 캐시가 비어 있으면 전부 없는 것으로 나온다 → 전환이 거부된다(맞다 —
 *       그릴 폰트가 없는데 문자판만 바꾸면 전부 빈칸이 된다).
 */
const char* DisplayManager::missingGlyphFor(uint8_t script) const {
    const chtime::Script s = (script == SCRIPT_TRADITIONAL) ? chtime::CHT_SCRIPT_TRADITIONAL
                                                            : chtime::CHT_SCRIPT_SIMPLIFIED;
    for (size_t i = 0; i < chtime::scriptOnlyCount(s); i++) {
        const char* g = chtime::scriptOnlyGlyph(s, i);
        if (!g || !renderer.hasGlyph(String(g))) return g;
    }
    return nullptr;
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
    // [ENG bug 2 수정 승계] configTime(0, 0, ...)을 쓰면 코어가 마지막에 setTimeZone(-0, 0)을
    // 호출해 setenv("TZ", "UTC0DST0")로 **방금 설정한 TZ를 덮어쓴다**. configTzTime()은
    // setenv("TZ", tz) + tzset()까지 해 주는 코어 제공 함수다.
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
 * @details 검증과 파일 쓰기가 여기 한 곳에 모여 있다. setFontName()은 이 함수에 위임한다.
 *          검증을 두 곳에 두면 한쪽만 고쳐졌을 때 어느 쪽이 진짜 규칙인지 알 수 없다.
 */
bool DisplayManager::setSlotName(uint8_t slot, const String& name) {
    if (slot >= FONT_SLOT_COUNT) return false;

    // [ENG 리뷰 §1.3 승계 / 2026-10-05 수정] 이 값은 (1) JSON 응답에 붙고 (2) "/fN/name.txt" 의
    //   **내용**으로 쓰인다. 경로는 리터럴이라 이름이 경로 조작을 일으키지 않으므로
    //   (config.h FONT_NAME_MAX_LEN 주석 참조) 상한은 **길이 폭주 방지** 목적이다.
    //   ⚠ 예전의 `>= 32` 는 "이름을 파일명으로도 쓴다"는 잘못된 전제에서 나온 값이었고,
    //     35바이트짜리 실제 폰트 이름을 거부해 슬롯이 Empty Slot 으로 보이게 했다.
    //   `"` `\` 제어문자 금지는 그대로 둔다 — 이름은 사람이 읽는 값이고, 여기 들어올 이유가 없다.
    if (name.length() == 0 || name.length() > FONT_NAME_MAX_LEN) return false;
    for (size_t i = 0; i < name.length(); i++) {
        const char c = name[i];
        if (c == '"' || c == '\\' || c == '/' || c < 0x20) return false;
    }

    // 이미 그 슬롯의 이름이면 파일을 다시 쓰지 않는다 — 글리프 업로드가 37회 부른다.
    //   (캐시가 실제 파일과 어긋날 수 있는 경우는 없음: 캐시는 부팅 때 이 파일에서 읽고,
    //    이후 이 함수와 setFontSlot()만이 둘을 함께 갱신한다.)
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

/**
 * @brief 현재 슬롯의 이름표를 바꾼다 (웹 /api/config 의 font_name)
 * @note 실제 쓰기는 setSlotName()이 한다. **업로드 경로는 이 함수를 쓰면 안 된다** —
 *       업로드 슬롯과 현재 슬롯이 다르면 이름표가 엉뚱한 폴더로 간다(헤더 주석 참조).
 */
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
    //   예전엔 configManager.font_name만 갱신했으므로, 슬롯을 옮겨도
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
    //   다른 이름이 섞이면 화면과 API가 서로 다른 말을 하게 된다.
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
    // 정적 경로·애니메이션 경로가 **같은 배치 함수**(drawCenterText)를 쓴다.
    //   ENG판은 여기서 isSingleLineLayout()을 정적 경로와 반대로 넘겨 버리는 버그가 있었다.
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
    u8g2_1.clearBuffer();
    u8g2_2.clearBuffer();
    u8g2_3.clearBuffer();
    u8g2_4.clearBuffer();

    // 셰도우 버퍼를 무효화하여 pushParallel()이 모든 0픽셀을 강제로 전송하게 함
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

void DisplayManager::drawCenterText(int idx, const String& text) {
    U8G2* u8g2 = screens[idx];
    u8g2->clearBuffer();

    LayoutChar chars[LAYOUT_MAX_CHARS];
    const int count = layoutFor(text, chars);
    for (int i = 0; i < count; i++) {
        renderer.drawSingleChar(idx, cellText(chars[i]), chars[i].x, LINE_TOP_Y);
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
                drawCenterText(i, texts[i]);
                lastTexts[i] = texts[i];
            }
        }
        pushParallel();
        return;
    }

    // [ENG Step 3.1 승계] 비차단 애니메이션 상태 초기화.
    // 이전 전환이 끝나기 전에 새 전환이 시작될 수 있다(매초 호출되기 때문).
    // 이번에 변경되지 않은 화면이 이전 전환에서 애니메이션 중이었다면, 아래 루프에서
    // changed가 false로 덮여써져 렌더링이 완전히 멈추고 중간 프레임이 화면에 남는다
    // (ghost column). 그 잔상을 먼저 정착된 최종 상태로 지운다.
    for (int i = 0; i < 4; i++) {
        if (_animState.screens[i].changed && !changed[i]) {
            drawCenterText(i, lastTexts[i]);
        }
    }

    _animState.active = true;
    _animState.currentStep = 0;
    _animState.lastUpdateMs = 0; // 즉시 첫 틱 실행

    for (int i = 0; i < 4; i++) {
        _animState.screens[i].changed = changed[i];
        if (changed[i]) {
            snapshotText(lastTexts[i], _animState.screens[i].oldChars, _animState.screens[i].oldCount);
            snapshotText(texts[i], _animState.screens[i].newChars, _animState.screens[i].newCount);
            lastTexts[i] = texts[i]; // 타겟 텍스트 선점
        }
    }
}

void DisplayManager::updateTick() {
    if (!_animState.active) return;

    // 시계 모드가 아니면 애니메이션 중단 (UI 전환 대응)
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

/**
 * @brief 한 위치의 새 글자와 그 자리를 대체하는 이전 글자를 함께 그린다
 * @param nc 새 글자. **텍스트와 X 좌표를 함께 결정한다** — 이전 글자는 같은 X에 그려진다
 *                    (위치가 같은 쌍이므로).
 * @param oc 이전 글자. 빈 문자열이면 첫 등장(퇴장 대상 없음).
 * @details 스크롤은 한 줄 높이(LINE_HEIGHT = 48) 전체를 이동시키는 것으로 본다.
 *          이전 글자는 LINE_TOP_Y − off로, 새 글자는 LINE_TOP_Y + LINE_HEIGHT − off로
 *          상대 이동한다. 최대 이동량은 정확히 한 줄이므로 두 래스터가 겹치지 않는다.
 * @note [ENG 버그 3 승계] 이동량을 한 줄 높이로 제한해야 이전 글자가 y ∈ [0,32)로 나가
 *       이웃 줄 밴드를 침범하지 않는다. 1줄엔 이웃 줄이 없으므로 밴드 자르기는 불필요하고,
 *       화면 밖으로 나간 부분은 U8g2가 디스플레이 버퍼에서 자른다.
 * @note 웹 미리보기 drawByLine()의 `off = animStep * (LINE_HEIGHT / 16)` 과 1:1 대응.
 */
static void drawAnimPair(int screenIdx, uint8_t mode, int step,
                         const AnimCell& nc, const String& oc) {
    const int off = step * (LINE_HEIGHT / ANIM_STEPS);
    switch (mode) {
        case ANIMATION_TYPE_SCROLL_UP:
            if (step < ANIM_STEPS && oc.length()) {
                renderer.drawSingleChar(screenIdx, oc, nc.x, LINE_TOP_Y - off);
            }
            renderer.drawSingleChar(screenIdx, nc.text, nc.x, LINE_TOP_Y + LINE_HEIGHT - off);
            break;
        case ANIMATION_TYPE_SCROLL_DOWN:
            if (step < ANIM_STEPS && oc.length()) {
                renderer.drawSingleChar(screenIdx, oc, nc.x, LINE_TOP_Y + off);
            }
            renderer.drawSingleChar(screenIdx, nc.text, nc.x, LINE_TOP_Y - LINE_HEIGHT + off);
            break;
        case ANIMATION_TYPE_VERTICAL_FLIP:
            // 한 줄 높이를 기준으로 0 ↔ LINE_HEIGHT로 스케일한다.
            if (step <= 8) { if (oc.length()) renderer.drawScaledChar(screenIdx, oc, nc.x, LINE_TOP_Y, ((8 - step) * LINE_HEIGHT) / 8); }
            else { renderer.drawScaledChar(screenIdx, nc.text, nc.x, LINE_TOP_Y, ((step - 8) * LINE_HEIGHT) / 8); }
            break;
        case ANIMATION_TYPE_DITHERED_FADE:
            if (step <= 8) { if (oc.length()) renderer.drawDitheredChar(screenIdx, oc, nc.x, LINE_TOP_Y, 16 - (step * 2)); }
            else { renderer.drawDitheredChar(screenIdx, nc.text, nc.x, LINE_TOP_Y, (step - 8) * 2); }
            break;
        case ANIMATION_TYPE_ZOOM:
            if (step <= 8) { if (oc.length()) renderer.drawZoomedChar(screenIdx, oc, nc.x, LINE_TOP_Y, ((8 - step) * 100) / 8); }
            else {
                int sc = (step <= 12) ? ((step - 8) * 150 / 4) : (150 - (step - 12) * 50 / 4);
                renderer.drawZoomedChar(screenIdx, nc.text, nc.x, LINE_TOP_Y, sc);
            }
            break;
        default: break;
    }
}

/**
 * @brief 새 텍스트에 동일 위치가 없어 사라진 이전 셀을 퇴장시킨다
 * @note drawAnimPair()와 **같은 이동량식**을 쓴다. 두 수가 어긋나면 등장/퇴장 속도가 다르다.
 */
static void drawAnimExit(int screenIdx, uint8_t mode, int step, const AnimCell& oc) {
    const int off = step * (LINE_HEIGHT / ANIM_STEPS);
    switch (mode) {
        case ANIMATION_TYPE_SCROLL_UP:    if (step < ANIM_STEPS) renderer.drawSingleChar(screenIdx, oc.text, oc.x, LINE_TOP_Y - off); break;
        case ANIMATION_TYPE_SCROLL_DOWN:  if (step < ANIM_STEPS) renderer.drawSingleChar(screenIdx, oc.text, oc.x, LINE_TOP_Y + off); break;
        case ANIMATION_TYPE_VERTICAL_FLIP: if (step <= 8) renderer.drawScaledChar(screenIdx, oc.text, oc.x, LINE_TOP_Y, ((8 - step) * LINE_HEIGHT) / 8); break;
        case ANIMATION_TYPE_DITHERED_FADE: if (step <= 8) renderer.drawDitheredChar(screenIdx, oc.text, oc.x, LINE_TOP_Y, 16 - (step * 2)); break;
        case ANIMATION_TYPE_ZOOM:          if (step <= 8) renderer.drawZoomedChar(screenIdx, oc.text, oc.x, LINE_TOP_Y, ((8 - step) * 100) / 8); break;
        default: break;
    }
}

void DisplayManager::renderAnimFrame(int i, int step) {
    ScreenAnimData& sd = _animState.screens[i];
    screens[i]->clearBuffer();
    const uint8_t mode = configManager.get().anim_mode;

    for (int j = 0; j < sd.newCount; j++) {
        const AnimCell& nc = sd.newChars[j];
        const String* old = findSamePosition(sd.oldChars, sd.oldCount, nc);
        // 문자까지 같으면 완전히 정지한 셀 — 애니메이션 대상이 아니다.
        if (old && *old == nc.text) renderer.drawSingleChar(i, nc.text, nc.x, LINE_TOP_Y);
        else                          drawAnimPair(i, mode, step, nc, old ? *old : kNoOldChar);
    }

    for (int k = 0; k < sd.oldCount; k++) {
        if (hasSamePosition(sd.newChars, sd.newCount, sd.oldChars[k])) continue;
        drawAnimExit(i, mode, step, sd.oldChars[k]);
    }

    if (isTitleScreenOf(i) && configManager.get().chime_enabled) screens[i]->drawXBM(0, 0, 8, 8, bell_icon);
}

/**
 * @brief 한 화면의 U8g2 버퍼를 셰도 버퍼와 비교해, 바뀐 페이지 구간만 콜백에 넘긴다
 * @details [ENG 리뷰 §2.1 승계] 이 함수는 HW 버스(0·1)와 SW 버스(2·3)가 **똑같이** 도는 부분이다.
 *          diff는 순수 계산이고 전송은 버스마다 다르므로, 여기서는 diff만 하고
 *          실제 전송은 onPageDirty에 위임한다 (side effect를 경계에 격리).
 * @param onPageDirty (screen, screenIdx, page, firstTile, tileCount) — 해당 페이지가 더러울 때 한 번 호출
 * @note 셰도 버퍼는 **비교 직후** 갱신한다. 전송은 HW 태스크가 나중에 하므로,
 *      여기서 갱신하지 않으면 같은 프레임이 두 번 전송된다.
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
    // 숫자가 쓰는 기하를 캐시에서 찾는다. 없으면 기본 기하(drawSingleChar의 폴백과 같은 선택).
    const CellGeometry* dg = nullptr;
    for (int i = 0; i < 4 && !dg; i++) {
        String seg = String(ip[configManager.get().is_flipped ? (3 - i) : i]);
        for (int j = 0; j < seg.length() && !dg; j++) dg = renderer.geometryOf(seg.substring(j, j + 1));
    }
    const CellGeometry& g = dg ? *dg : defaultGeometry();
    const int pitch = g.glyphW;

    // 도트는 마지막 숫자 옆("192."처럼)에, **래스터 하단**에 맞춰 찍는다.
    // 묶음 폭 = 숫자들 + 간격 + 도트 → 이 묶음을 가로 중앙에 놓는다.
    // (ENG의 dotY = (SCREEN_HEIGHT + glyphH)/2 는 1줄에서 LINE_TOP_Y + glyphH와 같다 —
    //  도트를 래스터 아래쪽에, 숫자 위가 아니라 아래에 맞춘다는 의도가 값에 그대로 남아 있다.)
    const int dotY = LINE_TOP_Y + g.glyphH - IP_DOT_SIZE;

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
            renderer.drawSingleChar(i, segment.substring(j, j + 1), startX + (j * pitch), LINE_TOP_Y);
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
    // [표시 모드 라벨 — PLAN §6.9는 "S:MODE(한자)"를 지시하나 그대로 따르지 않았다]
    //   이 화면은 u8g2_font_7x14_tf(라틴 전용)로 그린다. 한자를 넣으면 UTF-8 바이트 3개가
    //   라틴 글리프로 해석되어 **깨진 글자**가 출력된다.
    //   CJK 폴백 폰트를 도입하지 않는 것(PLAN §6.7 대안 A)이 Flash 용량 결정이기 때문에,
    //   여기서 한자를 쓰려면 커스텀 폰트 슬롯에 그 글자를 올려야 하는데 — "漢"도 "字"도
    //   §6.11의 필수 문자집합(34자)에 없다. 즉 웹 폰트로도 이 라벨은 그릴 수 없다.
    //   → 라틴 약어(SIM/TRAD/NUM)으로 두고, 한자 표기는 Step 10 이후 재검토한다.
    //   BTN2 short가 3단계(简体/繁體/數字)를 순환하므로 약어도 3개여야 한다.
    char chimeStr[20], animStr[32], modeStr[16], fmtStr[16], fontStr[20];
    sprintf(chimeStr, "S:CHIME(%s)", configManager.get().chime_enabled ? "ON" : "OFF");
    sprintf(animStr, "S:ANIMATION MODE %d", configManager.get().anim_mode);
    const char* presTag = (presentation() == PRESENTATION_TRADITIONAL) ? "TRAD"
                          : (presentation() == PRESENTATION_NUMERIC)  ? "NUM"
                                                                      : "SIM";
    sprintf(modeStr, "S:CHAR(%s)", presTag);
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