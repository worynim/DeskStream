// worynim@gmail.com
/**
 * @file Hangeul_Clock.ino
 * @brief 한글 시계 메인 엔트리 포인트
 * @details 시스템 초기화(Setup), 메인 서비스 루프 제어 및 전역 서비스 통합 관리.
 *          버튼·WiFi·NTP 감독·로그는 DeskStreamCore(ds::) 서비스를 쓴다.
 */
#include <Arduino.h>
#include <WiFi.h>      // WiFi.localIP() — IP 화면
#include <time.h>
#include "LittleFS.h"
#include <DeskStreamCore.h>
#include "config.h"
#include "display_manager.h"
#include "hangeul_time.h"
#include <WebServer.h>
#include "web_manager.h"

// === 앱 전역 서비스 (display_manager.h가 logger·timeSync를 extern 선언한다) ===
ds::Logger logger;
ds::TimeSync timeSync;
static ds::Buttons buttons;
static ds::WifiService wifi;

/**
 * @brief WiFi 설정 모드(AP) 진입 시 호출 — 화면 0에 AP 이름과 접속 주소를 안내한다
 * @details SSID를 라벨과 값으로 나눠 4줄로 그린다. 한 줄에 "SSID: Hangeul_Clock_Setup"(25자)을
 *          넣으면 6x10 폰트에서 150px라 128px 화면을 넘긴다(2026-11-04 사용자 보고).
 *          셰도 버퍼를 우회하는 sendBuffer() 대신 push()로 보낸다 — 셰도와 화면이 어긋나지 않게.
 */
static void onWifiPortal(const char* apName) {
    logger.addLog("AP Mode");
    U8G2& u = display.core.screen(0);
    u.clearBuffer();
    u.setFont(STATUS_FONT);
    u.drawStr(0, 10, "WiFi Config Mode");
    u.drawStr(0, 25, "SSID:");
    u.drawStr(0, 40, apName);
    u.drawStr(0, 55, "IP: 192.168.4.1");
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
    if (!renderer.isCacheLoaded()) {
        display.showStatus("Font Required!");
        return;
    }
    uint8_t nextMode = (configManager.get().display_mode == CLOCK_MODE_HANGUL) ? CLOCK_MODE_NUMERIC : CLOCK_MODE_HANGUL;
    display.setDisplayMode(nextMode);
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

    // ESP32-C3 USB CDC(HWCDC): 호스트가 포트를 읽지 않아 TX 링(256B)이 차면
    // HWCDC::write()가 호출당 최대 2초(100ms × 20회)를 블로킹해 loop()가 멈춘다.
    // 버튼·부저 등 실시간 처리가 지연되지 않도록 TX 타임아웃을 0(비차단)으로.
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
    
    logger.addLog("Hangeul Clock");
    logger.addLog("Service Unitized");
    
    // 비트맵 캐시 로딩 (이 작업이 무거워서 이전에는 버튼 감지를 방해함)
    display.loadBitmapCache();
    display.setYieldCallback(on_yield);

    // WiFi 연결 시퀀스 — 저장된 AP에 비차단으로 10초 시도 → 실패 시 곧바로 설정 포털(120초)
    //   → 그래도 실패하면 재부팅. autoConnect()의 60초 블로킹을 피하는 이유는 ds_wifi.h 참조.
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

    // 5. 시간 동기화 (NTP + POSIX TZ)
    // configManager.begin()이 display.begin() 내부에서 이미 끝났으므로
    // 이 시점의 configManager.get().timezone은 NVS에서 로드·검증된 값이다.
    //   [2026-11-07] timeSync.begin()이 SNTP 내부 재폴링 주기를 1시간으로 낮추고
    //   재동기화 감독을 시작한다(서버·주기는 ds::TimeSyncConfig 기본값). TZ 적용·SNTP 시작은
    //   display.applyTimezone() → timeSync.applyTimezone() 한 경로로만 일어난다.
    ds::TimeSyncConfig timeCfg;
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
    display.clearAll(); // [Step 5.1] 부팅 로그 종료 후 시계 시작 전 잔상 완벽 소거
}


void handleClockUpdate(const struct tm& timeinfo, bool force = false) {
    // [시보] 정시 알림 로직
    if (configManager.get().chime_enabled && timeinfo.tm_min == 0 && timeinfo.tm_sec == 0) {
        static int lastChimeHour = -1;
        if (lastChimeHour != timeinfo.tm_hour) {
            display.playChimeMelody();
            lastChimeHour = timeinfo.tm_hour;
        }
    }

    int h = timeinfo.tm_hour, m = timeinfo.tm_min, s = timeinfo.tm_sec, d = timeinfo.tm_mday;
    String texts[4];
    // 커스텀 폰트 캐시가 로드되지 않은 경우 강제로 숫자 모드 사용 (기본 폰트 글자 부족 대응)
    bool isHangul = (configManager.get().display_mode == CLOCK_MODE_HANGUL) && renderer.isCacheLoaded();
    bool is24H = (configManager.get().hour_format == HOUR_FORMAT_24H);

    // 텍스트 생성 (v1.4.0 최적화 구조)
    if (is24H) texts[0] = isHangul ? HangeulTimeConverter::getDay(d) : HangeulTimeConverter::getNumericDay(d);
    else texts[0] = HangeulTimeConverter::getAmPm(h);

    texts[1] = isHangul ? HangeulTimeConverter::getHour(h, is24H) : HangeulTimeConverter::getNumericHour(h, is24H);
    texts[2] = isHangul ? HangeulTimeConverter::getMinute(m) : HangeulTimeConverter::getNumericMinute(m);
    texts[3] = isHangul ? HangeulTimeConverter::getSecond(s) : HangeulTimeConverter::getNumericSecond(s);

    // [Step 5.3] '정각' 중복 표시 방지
    //   getMinute(0)과 getSecond(0)이 **둘 다** "정각"을 반환한다. 0분0초에 그대로 두면
    //   화면2와 화면3에 "정각"이 동시에 뜨므로, 화면3을 비워 화면2가 담당하게 한다.
    //   → getMinute()의 "정각" 분기가 바뀌면 이 조건도 같이 바꿔야 한다. (두 곳이 짝)
    //   참고: 화면3은 0분0초 여부와 무관하게 매초 애니메이션을 돌린다.
    if (isHangul && m == 0 && s == 0) {
        texts[3] = "";
    }

    display.updateAll(texts, force);
}

// 루프 시작
void loop() {
    on_yield(); 
    display.updateTick(); 
    wifi.loop();       // WiFi 끊김 시 60초마다 재접속 시도
    timeSync.loop();   // NTP 재동기화 감독: 주기·WiFi 복구·실패 재시도

    // [2026-11-07] 1초 갱신 트리거를 millis() 1000ms 주기에서 **초 경계(epoch 초) 감지**로
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
