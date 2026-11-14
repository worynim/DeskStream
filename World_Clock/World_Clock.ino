// worynim@gmail.com
/**
 * @file World_Clock.ino
 * @brief 세계 4곳 시계 — OLED 화면 하나에 도시 하나의 현지 시각
 * @details 이 보드는 OLED 화면을 누르면 그 화면의 버튼이 눌린다 (화면 i = 버튼 i).
 *          시계 화면: 화면 1 짧게 = 12/24시, 화면 2 = 밝기, 화면 3 = 정보 카드, 화면 4 = 한글/영문.
 *                    아무 화면이나 길게 = 그 화면의 장소 설정.
 *          설정 화면: 고치는 화면 짧게 = 저장. 나머지 세 화면은 왼쪽부터 지도(취소) · 서쪽 · 동쪽.
 *                    서쪽/동쪽은 짧게 = 도시 하나, 누르고 있기 = 시간대 단위로 건너뛰기.
 *          시각은 NTP 로 UTC 만 받고(시스템 TZ = UTC0), 도시별 현지 시각은 tz_calc 가 계산한다.
 *          WiFi 정보는 DeskStreamCore 내장 접속 포털로 넣는다. 웹 서버는 띄우지 않는다.
 */
#include <Arduino.h>
#include <DeskStreamCore.h>
#include <time.h>

#include "city_table.h"
#include "clock_view.h"
#include "config.h"
#include "misc_view.h"
#include "setup_view.h"

// ---------------------------------------------------------------- 설정 (NVS)
struct Settings {
    uint8_t cityId[4] = {kDefaultCityIds[0], kDefaultCityIds[1], kDefaultCityIds[2], kDefaultCityIds[3]};
    uint8_t brightness = 2;     // kBrightnessLevels 의 순서
    bool hour24 = false;
    uint8_t lang = LANG_KO;
};
static Settings settings;
static ds::ConfigStore store;

static const ds::ConfigField kFields[] = {
    DS_CFG_U8(Settings, cityId[0], "city1", "city1", 0, 255, nullptr),
    DS_CFG_U8(Settings, cityId[1], "city2", "city2", 0, 255, nullptr),
    DS_CFG_U8(Settings, cityId[2], "city3", "city3", 0, 255, nullptr),
    DS_CFG_U8(Settings, cityId[3], "city4", "city4", 0, 255, nullptr),
    DS_CFG_U8(Settings, brightness, "brightness", "bright", 0, kBrightnessCount - 1, nullptr),
    DS_CFG_BOOL(Settings, hour24, "hour24", "hour24", nullptr),
    DS_CFG_U8(Settings, lang, "lang", "lang", LANG_KO, LANG_EN, nullptr),
};

// ---------------------------------------------------------------- 장치
static ds::DisplayCore display;
static ds::Buttons buttons;
static ds::Buzzer buzzer;
static ds::WifiService wifi;
static ds::TimeSync timeSync;

static const ds::Note kStartup[] = {{2093, 100, 20}, {2637, 100, 20}, {3136, 100, 20}};

// ---------------------------------------------------------------- 상태
static int cityIndex[4];            // 화면별 도시 (목록 순서). settings.cityId 에서 만든다
static bool inSetup = false;
static int editScreen = 0;          // 설정 중인 화면
static int candidate = 0;           // 설정 중 후보 도시 (목록 순서)
static uint32_t lastInputMs = 0;    // 설정 화면의 마지막 입력 시각 (타임아웃용)
static int toastScreen = -1;        // 알림 딱지를 띄운 화면 (-1 = 없음)
static char toastText[24];
static uint32_t toastUntilMs = 0;
static uint32_t infoUntilMs = 0;    // 정보 카드를 띄워 두는 시각 (0 = 안 띄움)
static bool needRedraw = true;

static Lang lang() { return (Lang)settings.lang; }
static const char* cityName(int index) { return lang() == LANG_KO ? cityAt(index).nameKo : cityAt(index).nameEn; }
static u8g2_t* raw(int screen) { return display.screen(screen).getU8g2(); }

/** 시스템 시각은 UTC 로만 쓴다 — 도시별 시각은 tz_calc 가 계산 */
static const char* utcTimezone() { return "UTC0"; }

/** 저장된 도시 ID → 목록 순서. 표에 없는 ID 면 그 화면의 기본 도시 */
static void loadCityIndexes() {
    for (int s = 0; s < 4; s++) {
        int idx = cityIndexOfId(settings.cityId[s]);
        if (idx < 0) {
            settings.cityId[s] = kDefaultCityIds[s];
            idx = cityIndexOfId(kDefaultCityIds[s]);
        }
        cityIndex[s] = idx;
    }
}

// ---------------------------------------------------------------- 그리기
static void showMessage(const char* const* lines, int count) {
    for (int s = 0; s < ds::kNumScreens; s++) drawMessage(raw(s), s == 0 ? lines : nullptr, s == 0 ? count : 0);
    display.push();
}

static void onWifiConnecting(uint32_t) {
    static const char* const kLines[2][3] = {{nullptr, nullptr, "WiFi 연결 중..."}, {nullptr, nullptr, "Connecting WiFi..."}};
    showMessage(kLines[lang()], 3);
}

static void onWifiPortal(const char* apName) {
    const char* const ko[5] = {"WiFi 설정이 필요합니다", nullptr, "휴대폰으로 아래 WiFi 에 접속", apName, "192.168.4.1"};
    const char* const en[5] = {"WiFi setup needed", nullptr, "Join this WiFi on your phone", apName, "192.168.4.1"};
    showMessage(lang() == LANG_KO ? ko : en, 5);
}

static void drawClockScreen(int s, int64_t utc, bool timeValid, bool blinkOn) {
    const LocalTime t = toLocalTime(utc, cityTz(cityAt(cityIndex[s])));
    const LocalTime home = toLocalTime(utc, cityTz(cityAt(cityIndex[0])));
    if (timeValid && infoUntilMs != 0) {
        const InfoCard card = {cityName(cityIndex[s]), lang(), t.year, t.month, t.day, t.weekday,
                               t.offsetMin, t.dst, s == 0, (int16_t)(t.offsetMin - home.offsetMin)};
        drawInfoCard(raw(s), card);
        return;
    }
    const ClockFace face = {cityName(cityIndex[s]), lang(), t.hour, t.minute, t.month, t.day, t.weekday,
                            (int8_t)(t.dayNumber - home.dayNumber), settings.hour24, blinkOn, timeValid};
    drawClock(raw(s), face);
    if (s == toastScreen) drawToast(raw(s), toastText);
}

/** 설정 화면: 고치는 화면은 미리보기, 나머지는 setupRole 이 정한 지도(취소) · 서쪽 · 동쪽 */
static void drawSetupScreen(int s, int64_t utc, bool timeValid, bool blinkOn) {
    const City& city = cityAt(candidate);
    const SetupRole role = setupRole(editScreen, s);
    if (role == SETUP_EDIT) {
        const LocalTime t = toLocalTime(utc, cityTz(city));
        const bool isHome = editScreen == 0;
        const int homeOffset = utcOffsetMinutes(utc, cityTz(cityAt(cityIndex[0])));
        const SetupPreview p = {cityName(candidate), lang(), t.hour, t.minute, t.offsetMin, t.dst,
                                isHome ? nullptr : cityName(cityIndex[0]), (int16_t)(t.offsetMin - homeOffset),
                                blinkOn, timeValid};
        drawSetupPreview(raw(s), p);
        return;
    }
    if (role == SETUP_MAP) {
        drawSetupMap(raw(s), lang(), cityMapX(city), cityMapY(city), blinkOn);
    } else {
        const int next = cityStep(candidate, role == SETUP_WEST ? -1 : +1);
        drawSetupMove(raw(s), lang(), role == SETUP_EAST, cityName(next), utcOffsetMinutes(utc, cityTz(cityAt(next))));
    }
}

static void render(bool blinkOn) {
    const time_t now = time(nullptr);
    const bool timeValid = ds::localTimeIsValid(now);
    for (int s = 0; s < ds::kNumScreens; s++) {
        if (inSetup) drawSetupScreen(s, now, timeValid, blinkOn);
        else drawClockScreen(s, now, timeValid, blinkOn);
    }
    display.push();                    // 바뀐 타일만 전송
}

// ---------------------------------------------------------------- 입력
static void showToast(int screen, const char* text) {
    toastScreen = screen;
    snprintf(toastText, sizeof(toastText), "%s", text);
    toastUntilMs = millis() + kToastMs;
}

/** 시계 화면에서 화면 s 를 짧게 눌렀을 때 */
static void onClockShort(int s) {
    char buf[24];
    switch (s) {
        case 0:
            settings.hour24 = !settings.hour24;
            showToast(s, settings.hour24 ? "24H" : "12H");
            break;
        case 1:
            settings.brightness = (settings.brightness + 1) % kBrightnessCount;
            display.setBrightness(kBrightnessLevels[settings.brightness]);
            snprintf(buf, sizeof(buf), lang() == LANG_KO ? "밝기 %d/%d" : "BRIGHT %d/%d", settings.brightness + 1, kBrightnessCount);
            showToast(s, buf);
            break;
        case 2:
            infoUntilMs = millis() + kInfoCardMs;
            return;                     // 저장할 것이 없다
        default:
            settings.lang = lang() == LANG_KO ? LANG_EN : LANG_KO;
            showToast(s, lang() == LANG_KO ? "한글" : "ENGLISH");
            break;
    }
    store.setDirty();
}

/** 설정 화면에서 화면 s 에 난 이벤트 처리 */
static void onSetupEvent(int s, uint8_t ev) {
    const SetupRole role = setupRole(editScreen, s);
    if (role == SETUP_EDIT) {
        if (!(ev & ds::BUTTON_SHORT)) return;
        cityIndex[editScreen] = candidate;                 // 저장하고 나가기
        settings.cityId[editScreen] = cityAt(candidate).id;
        store.setDirty();
        inSetup = false;
        buzzer.beep(80, 3136);
        return;
    }
    if (role == SETUP_MAP) {
        if (!(ev & ds::BUTTON_SHORT)) return;
        inSetup = false;                                    // 취소
        buzzer.beep(80, 1500);
        return;
    }
    const int dir = role == SETUP_WEST ? -1 : +1;
    if (ev & ds::BUTTON_REPEAT) candidate = cityJumpZone(candidate, dir, time(nullptr));
    else if (ev & ds::BUTTON_SHORT) candidate = cityStep(candidate, dir);
    else return;
    buzzer.beep(15, 3000);
}

static void handleButtons() {
    buttons.update();
    for (int s = 0; s < ds::kNumScreens; s++) {
        const uint8_t ev = buttons.events(s);
        if (ev == 0) continue;
        needRedraw = true;
        if (inSetup) {
            lastInputMs = millis();
            onSetupEvent(s, ev);
        } else if (ev & ds::BUTTON_LONG) {
            inSetup = true;                                 // 이 화면의 장소 설정 시작
            editScreen = s;
            candidate = cityIndex[s];
            lastInputMs = millis();
            toastScreen = -1;
            infoUntilMs = 0;
            buzzer.beep(80, 2637);
        } else if (ev & ds::BUTTON_SHORT) {
            buzzer.beep(30, 3000);
            onClockShort(s);
        }
        if (!inSetup) break;    // 설정을 막 나왔거나 시계 화면이면 같은 순간의 다른 버튼은 처리하지 않는다
    }
}

/** 시간이 다 된 알림 딱지 · 정보 카드 · 설정 화면을 걷는다 */
static void handleTimeouts() {
    const uint32_t now = millis();
    if (toastScreen >= 0 && (int32_t)(now - toastUntilMs) >= 0) {
        toastScreen = -1;
        needRedraw = true;
    }
    if (infoUntilMs != 0 && (int32_t)(now - infoUntilMs) >= 0) {
        infoUntilMs = 0;
        needRedraw = true;
    }
    if (inSetup && now - lastInputMs >= kSetupTimeoutMs) {
        inSetup = false;        // 저장하지 않고 돌아간다
        needRedraw = true;
    }
}

// ---------------------------------------------------------------- setup / loop
void setup() {
    Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(0);          // 시리얼 모니터를 안 열어도 loop()가 멈추지 않게
#endif
    Serial.printf("World_Clock %s\n", WORLD_CLOCK_VERSION);

    store.begin("worldclock", &settings, kFields, sizeof(kFields) / sizeof(kFields[0]));
    loadCityIndexes();

    display.begin();
    display.setBrightness(kBrightnessLevels[settings.brightness]);
    ds::ButtonTiming timing;
    timing.repeatDelayMs = kRepeatDelayMs;
    timing.repeatIntervalMs = kRepeatIntervalMs;
    buttons.begin(timing);
    buzzer.begin();
    buzzer.playMelody(kStartup, sizeof(kStartup) / sizeof(kStartup[0]));

    ds::WifiConfig wifiCfg;
    wifiCfg.apName = kPortalApName;
    wifiCfg.onConnecting = onWifiConnecting;
    wifiCfg.onPortal = onWifiPortal;
    wifi.begin(wifiCfg);               // 접속(또는 포털)이 끝날 때까지 반환하지 않는다. 실패하면 재부팅

    ds::TimeSyncConfig timeCfg;
    timeCfg.tzProvider = utcTimezone;
    timeSync.begin(timeCfg);
    timeSync.applyTimezone();
}

void loop() {
    wifi.loop();
    timeSync.loop();
    handleButtons();
    handleTimeouts();

    static int shownBlink = -1;
    const int blink = (millis() / kBlinkIntervalMs) % 2 == 0;
    if (blink != shownBlink) needRedraw = true;
    if (!needRedraw) return;
    shownBlink = blink;
    needRedraw = false;
    render(blink);
}
