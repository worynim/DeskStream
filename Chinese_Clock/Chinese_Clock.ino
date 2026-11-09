// worynim@gmail.com
/**
 * @file Chinese_Clock.ino
 * @brief 중국어 단어 시계 메인 엔트리 포인트
 * @details 시스템 초기화(Setup), 메인 서비스 루프 제어 및 전역 서비스 통합 관리
 * @note [SYNC] 원본: ENG_Clock/ENG_Clock.ino — 시간 변환기와 텍스트 생성부만 교체.
 *       4화면 구조는 유지한다 — [요일|오전오후] / 시 / 분 / 초.
 */
#include <Arduino.h>
#include <WiFi.h>      // WiFi.localIP() — IP 화면
#include <time.h>
#include "LittleFS.h"
#include <DeskStreamCore.h>
#include "config.h"
#include "display_manager.h"
#include "chinese_time.h"
#include <WebServer.h>
#include "web_manager.h"

// === 앱 전역 서비스 (display_manager.h가 logger·timeSync를 extern 선언한다) ===
ds::Logger logger;
ds::TimeSync timeSync;
static ds::Buttons buttons;
static ds::WifiService wifi;

/**
 * @brief WiFi 설정 모드(AP) 진입 시 호출 — 화면 0에 AP 이름과 접속 주소를 안내한다
 * @details 셰도 버퍼를 우회하는 sendBuffer() 대신 push()로 보낸다 — 셰도와 화면이 어긋나지 않게.
 */
static void onWifiPortal(const char* apName) {
    logger.addLog("AP Mode");
    U8G2& u = display.core.screen(0);
    u.clearBuffer();
    u.setFont(STATUS_FONT);
    u.drawStr(0, 10, "WiFi Config Mode");
    u.drawStr(0, 25, (String("SSID: ") + apName).c_str());
    u.drawStr(0, 40, "IP: 192.168.4.1");
    display.pushParallel();
}

/** @brief 저장된 AP 접속 대기 중 500ms마다 — 진행 점 애니메이션 */
static void onWifiConnecting(uint32_t tick) {
    String dots = "WiFi Connecting";
    for (uint32_t j = 0; j <= tick % 5; j++) dots += ".";
    logger.updateLastLog(dots);
}

/** @brief NTP 감독이 적용할 TZ — config_manager가 로드·웹 입력 시 이미 검증한 값 */
static const char* currentTimezone() {
    return configManager.get().timezone;
}

int uiStage = 0; // 0: Clock, 1: IP, 2: Button Help

// --- 버튼 콜백 정의 ---
void btn1_short() {
    display.beep(50, 3000);
    display.setChime(!configManager.get().chime_enabled);
    if (uiStage == 2) display.showButtonHelp();
}

void btn1_long() {
    display.beep(150, 2000);
    display.setFlipDisplay(!configManager.get().is_flipped);
    display.clearAll();
    if (uiStage == 1) display.showLargeIP(WiFi.localIP());
    else if (uiStage == 2) display.showButtonHelp();
}

void btn2_short() {
    display.beep(50, 3000);
    // [2026-10-05] 简体 → 繁體 → 數字 순환.
    //   예전엔 "한자 ↔ 숫자" 두 칸이었다. 이제 세 칸이 되고, 한자 칸 두 개는
    //   **그릴 글자가 있을 때만** 들어간다(폰트 업로드 전에는 한자 칸이 막힌다).
    //   숫자 칸은 라틴 폰트만으로 되므로 폰트가 없어도 반드시 열린다 —
    //   그렇지 않으면 폰트를 한 번도 올리지 않은 기기에서 이 버튼이 dead 한다.
    //   검증과 저장은 display.setPresentation() 한 곳에 있다.
    uint8_t next = (display.presentation() + 1) % PRESENTATION_COUNT;
    display.setPresentation(next);
    display.clearAll();
    if (uiStage == 1) display.showLargeIP(WiFi.localIP());
    else if (uiStage == 2) display.showButtonHelp();
}

void btn2_long() {
    display.beep(150, 2000);
    uint8_t nextFormat = (configManager.get().hour_format == HOUR_FORMAT_12H) ? HOUR_FORMAT_24H : HOUR_FORMAT_12H;
    display.setHourFormat(nextFormat);
    display.clearAll();
    if (uiStage == 1) display.showLargeIP(WiFi.localIP());
    else if (uiStage == 2) display.showButtonHelp();
}

void btn3_short() {
    display.beep(50, 3000);
    uint8_t nextAnim = (configManager.get().anim_mode + 1) % ANIMATION_TYPE_COUNT;
    display.setAnimMode(nextAnim);
    if (uiStage == 2) display.showButtonHelp();
}

void btn3_long() {
    // [PLAN §6.10] 문자판 전환 버튼을 새로 늘리지 않는다.
    //   간체 폰트를 /f0, 번체를 /f1에 올려두면 이 순환이 곧 문자판 전환이 된다.
    //   웹 UI에서 문자판↔슬롯 연동을 제공하므로 표면 버튼은 4개 그대로다.
    display.beep(150, 2000);
    uint8_t nextSlot = (configManager.get().font_slot + 1) % FONT_SLOT_COUNT;
    display.setFontSlot(nextSlot);

    // 도움말 페이지에서는 페이지 갱신으로 현재 슬롯 표시, 그 외에는 상태 메시지 출력
    if (uiStage == 2) {
        display.showButtonHelp();
    } else {
        char msg[16];
        sprintf(msg, "Font Slot %d", nextSlot);
        display.showStatus(msg);
    }
}

void btn4_short() {
    display.beep(50, 3000);
    uiStage = (uiStage + 1) % UI_STAGE_COUNT;
    display.clearAll();
    if (uiStage == 1) display.showLargeIP(WiFi.localIP());
    else if (uiStage == 2) display.showButtonHelp();
}

void btn4_long() {
    display.beep(150, 2000);
    bool nextInv = !configManager.get().is_inverted;
    display.setInversion(nextInv);
    display.showStatus(nextInv ? "Invert: ON" : "Invert: OFF");
}

/**
 * @brief 시스템 양보 및 입력 동기화 (애니메이션 루프 등에서 빈번히 호출됨)
 */
void on_yield() {
    webManager.handleClient();
    buttons.update();
}

void setup() {
    Serial.begin(115200);

    // 시리얼 모니터를 열지 않아도 부팅이 멈추지 않게 한다 (ESP32-C3 native USB).
    // [ENG §setup 승계] HWCDC::write()는 호스트가 포트를 읽어주지 않으면
    //   xRingbufferSend(..., tx_timeout_ms) 를 max_consec_timeouts(20)회까지 반복하며
    //   호출당 최대 20 × 100ms = 2초를 블로킹한다. 부팅 중 logger.addLog()가 여러 번
    //   호출되므로 그만큼 멈춘다. 진단 로그가 부팅 경로를 막으면 안 되므로 TX 타임아웃을 0으로.
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(0);
#endif

    // 1. 시스템 공장 초기화 감지 (최우선 순위)
    pinMode(ds::kButtonPins[0], INPUT_PULLUP);
    delay(300); // 핀 및 하드웨어 안정화 대기

    if (digitalRead(ds::kButtonPins[0]) == LOW) {
        display.begin(); // OLED 피드백을 위해 먼저 초기화
        logger.attachDisplay(&display.core, 0, STATUS_FONT);
        display.showStatus("System Resetting...");
        logger.addLog("!!! FACTORY RESET !!!");

        // 파일 시스템 드라이버 활성화
        LittleFS.begin(true);

        logger.addLog("LittleFS Formatting...");
        if (LittleFS.format()) {
            logger.addLog("Format Success!");
        } else {
            logger.addLog("Format Failed!");
        }
        LittleFS.end();

        logger.addLog("WiFi Resetting...");
        ds::WifiService::resetSettings();

        logger.addLog("Reset Complete!");
        delay(2000);
        ESP.restart();
    }

    // 2. 일반 부팅 시퀀스
    if (!LittleFS.begin(true)) Serial.println("[SYSTEM] LittleFS Mount Failed");

    // 입력 시스템 초기화 (짧게 > 50ms, 길게 > 1000ms — ds::ButtonTiming 기본값)
    buttons.begin();
    buttons.setCallbacks(0, btn1_short, btn1_long);
    buttons.setCallbacks(1, btn2_short, btn2_long);
    buttons.setCallbacks(2, btn3_short, btn3_long);
    buttons.setCallbacks(3, btn4_short, btn4_long);

    // 디스플레이 초기화 및 시작 피드백
    display.begin();
    logger.attachDisplay(&display.core, 0, STATUS_FONT);   // 부팅 로그를 화면 0에 4줄로
#if FPS_BENCH_ON_BOOT
    display.core.runFpsBenchmark(FPS_BENCH_FRAMES, FPS_BENCH_SHOW_MS);   // 풀 프레임 전송 FPS 측정
#endif
    display.playStartupMelody();

    logger.addLog("Chinese Clock");
    logger.addLog("Service Unitized");

    // 비트맵 캐시 로딩 (이 작업이 무거워서 이전에는 버튼 감지를 방해함)
    display.loadBitmapCache();
    display.setYieldCallback(on_yield);

    // WiFi 연결 시퀀스 — 저장된 AP에 비차단으로 10초 시도 → 실패 시 곧바로 설정 포털(120초)
    //   → 그래도 실패하면 재부팅. autoConnect()는 내부에서 최대 60초를 블로킹해 그동안 화면 진행
    //   표시가 멈췄다 (자세한 이유는 libraries/DeskStreamCore/src/net/ds_wifi.h).
    logger.addLog("WiFi Connecting");
    ds::WifiConfig wifiCfg;
    wifiCfg.apName = WIFI_SSID_AP;
    wifiCfg.connectTimeoutMs = 10000;
    wifiCfg.portalTimeoutS = WIFI_CONFIG_TIMEOUT;
    wifiCfg.restartOnFail = true;
    wifiCfg.onConnecting = onWifiConnecting;
    wifiCfg.onPortal = onWifiPortal;
    wifi.begin(wifiCfg);

    logger.addLog("WiFi Connected!");
    webManager.begin();

    // 3. 시간 동기화 (NTP + POSIX TZ)
    // configManager.begin()이 display.begin() 내부에서 이미 끝났으므로
    // 이 시점의 configManager.get().timezone은 NVS에서 로드·검증된 값이다.
    //   [2026-11-08] timeSync.begin()이 SNTP 내부 재폴링 주기를 1시간으로 낮추고
    //   재동기화 감독을 시작한다. TZ 적용·SNTP 시작은 display.applyTimezone()
    //   → timeSync.applyTimezone() 한 경로로만 일어난다.
    ds::TimeSyncConfig timeCfg;   // 서버·감독 주기 = 기본값 (ds_time_sync.h)
    timeCfg.tzProvider = currentTimezone;
    timeSync.begin(timeCfg);
    display.applyTimezone();
    logger.addLog("Syncing Time");

    struct tm timeinfo;
    int retry = 0;
    while (!getLocalTime(&timeinfo) && retry < 15) {
        String dots = "Syncing Time";
        for (int j = 0; j <= retry % 5; j++) dots += ".";
        logger.updateLastLog(dots);
        delay(1000);
        retry++;
    }

    if (retry < 15) logger.addLog("Time Sync OK!");
    else logger.addLog("Time Sync Fail!");

    delay(1000);
    display.clearAll(); // 부팅 로그 종료 후 시계 시작 전 잔상 완벽 소거
}

void handleClockUpdate(const struct tm& timeinfo, bool force) {
    // [시보] 정시 알림 로직
    if (configManager.get().chime_enabled && timeinfo.tm_min == 0 && timeinfo.tm_sec == 0) {
        static int lastChimeHour = -1;
        if (lastChimeHour != timeinfo.tm_hour) {
            display.playChimeMelody();
            lastChimeHour = timeinfo.tm_hour;
        }
    }

    // d는 **요일(tm_wday, 0=일요일)** 이다. chinese_time_core::weekdayToChars()가
    // 0~6을 요일로 매핑하고 그 외에는 빈 문자열을 돌려준다.
    // (한글판·ENG판처럼 tm_mday를 물리면 7일 이후 제목 화면이 통째로 비게 된다)
    const int h = timeinfo.tm_hour, m = timeinfo.tm_min, s = timeinfo.tm_sec;
    const int d = timeinfo.tm_wday;
    const uint8_t sc = configManager.get().script_type;
    String texts[4];

    // 커스텀 폰트 캐시가 로드되지 않은 경우 강제로 숫자 모드 사용 (기본 폰트 글자 부족 대응)
    const bool isWord = (configManager.get().display_mode == CLOCK_MODE_WORD) && renderer.isCacheLoaded();
    const bool is24H = (configManager.get().hour_format == HOUR_FORMAT_24H);

    // 화면0: 24H는 요일("星期三"), 12H는 오전/오후("上午"/"下午")
    texts[0] = is24H ? ChineseTimeConverter::getWeekday(d, sc)
                     : ChineseTimeConverter::getDayPart(h, sc);

    // [PLAN §6.10 정정] 샘플 코드는 숫자 모드에 `+ " 时"`를 덧붙였으나,
    //   getNumericHour()는 chinese_time.cpp의 withUnit()에서 **이미** 단위와 공백을 붙인다
    //   ("13 时"). 그대로 쓰면 "13 时 时"이 되어 6셀 → 4셀 한계를 넘어 화면이 통째로 빈칸이 된다.
    //   ENG판의 getNumericHour()가 숫자만 돌려줘서 .ino가 단위를 붙이던 구조와 다르다.
    texts[1] = isWord ? ChineseTimeConverter::getHour(h, is24H, sc)
                      : ChineseTimeConverter::getNumericHour(h, is24H, sc);
    texts[2] = isWord ? ChineseTimeConverter::getMinute(m, sc)
                      : ChineseTimeConverter::getNumericMinute(m, sc);
    texts[3] = isWord ? ChineseTimeConverter::getSecond(s, sc)
                      : ChineseTimeConverter::getNumericSecond(s, sc);

    // 정각 규칙 (한글판·ENG판과 동일 위치): 0분 0초이면 분 화면이 整이므로 초 화면을 비운다.
    if (isWord && m == 0 && s == 0) texts[3] = "";

    display.updateAll(texts, force);
}

// 루프 시작
void loop() {
    on_yield(); 
    display.updateTick(); 
    wifi.loop();       // WiFi 끊김 시 60초마다 재접속 시도
    timeSync.loop();   // NTP 재동기화 감독: 주기·WiFi 복구·실패 재시도

    // [2026-11-08] 1초 갱신 트리거를 millis() 1000ms 주기에서 **초 경계(epoch 초) 감지**로
    //   교체했다. 이전 방식은 실제 초 경계와 위상이 어긋나 (1) 화면이 최대 1초 늦게
    //   바뀌고 (2) 렌더 시점이 매초 흔들려 초 표시가 건너뛰거나 겹쳐 보였다.
    //   초 경계는 시스템 시각이 유일한 기준이다. 판정 규칙은 ds_time_sync_policy.h의
    //   clockRenderDue()/secondTickDue() (호스트 단위 테스트 대상).
    //
    //   매 loop마다 읽는 것은 time()(epoch 초)뿐이다 — 비차단이고 저비용이다.
    //   localtime_r()은 실제 렌더가 일어나는 초에만 호출한다.
    //   getLocalTime()은 인자를 생략하면 최대 5초(기본 5000ms) 블로킹이라
    //   메인 루프에서 쓰면 버튼·애니메이션이 그만큼 멈추므로 쓰지 않는다.
    static int64_t lastEpochSec = -1;
    static unsigned long lastFallbackMs = 0;

    const time_t epochNow = time(nullptr);
    const bool timeValid = ds::localTimeIsValid((int64_t)epochNow);
    const bool secondChanged = timeValid && ds::secondTickDue(lastEpochSec, (int64_t)epochNow);
    if (timeValid) lastEpochSec = (int64_t)epochNow;

    const unsigned long now = millis();
    const bool fallbackDue = !timeValid && (now - lastFallbackMs >= UPDATE_INTERVAL_MS);
    // force 플래그는 uiStage가 0일 때만 소비한다 — IP/도움말 화면에서 버튼·웹이 세운
    //   갱신 요청을 잃지 않기 위해서다 (이전에는 매 루프에서 무조건 소비됐다).
    const bool forceUpdate = (uiStage == 0) && display.checkForceUpdate();

    if (uiStage == 0 && ds::clockRenderDue(forceUpdate, timeValid, secondChanged, fallbackDue)) {
        lastFallbackMs = now;
        if (timeValid) {
            struct tm timeinfo;
            localtime_r(&epochNow, &timeinfo);   // 초당 1회만 변환 (매 루프 아님)
            handleClockUpdate(timeinfo, forceUpdate);
        } else {
            lastEpochSec = -1;   // 시각이 돌아오면 반드시 다시 그린다
            display.showStatus("Time Sync Error!");
        }
    }
}
