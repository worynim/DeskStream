// worynim@gmail.com
/**
 * @file Hangeul_Clock.ino
 * @brief 한글 시계 메인 엔트리 포인트
 * @details 시스템 초기화(Setup), 메인 서비스 루프 제어 및 전역 서비스 통합 관리
 * @note [SYNC] ENG_Clock/ENG_Clock.ino — 고정 오프셋 NTP를 POSIX TZ 기반
 *       DisplayManager::applyTimezone()로 교체
 */
#include <Arduino.h>
#include <WiFiManager.h>
#include <time.h>
#include "LittleFS.h"
#include "config.h"
#include "display_manager.h"
#include "hangeul_time.h"
#include <WebServer.h>
#include "web_manager.h"
#include "input_manager.h"
#include "logger.h"
#include "time_sync.h"

/**
 * @brief WiFi 설정 모드(AP) 진입 시 호출되는 콜백
 */
void configModeCallback(WiFiManager *myWiFiManager) {
    Serial.println("[WIFI] Config Mode Entered");
    // [2026-11-04] SSID를 라벨과 값으로 나눠 4줄로 그린다.
    //   한 줄에 "SSID: Hangeul_Clock_Setup"(25자)을 넣으면 6x10 폰트에서 150px라
    //   128px 화면을 넘겨 "S"까지만 보인다(사용자 보고). 이름만 따로 그리면 114px로 들어간다.
    //   문자열은 config.h의 WIFI_SSID_AP를 쓴다 — 리터럴을 중복 기재하면 AP 이름을 바꿀 때
    //   실제 AP와 화면 표시가 어긋난다.
    display.u8g2_1.clearBuffer();
    display.u8g2_1.setFont(u8g2_font_6x10_tf);
    display.u8g2_1.drawStr(0, 10, "WiFi Config Mode");
    display.u8g2_1.drawStr(0, 25, "SSID:");
    display.u8g2_1.drawStr(0, 40, WIFI_SSID_AP);
    display.u8g2_1.drawStr(0, 55, "IP: 192.168.4.1");
    display.u8g2_1.sendBuffer();
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
    inputManager.update();
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
    pinMode(BTN1_PIN, INPUT_PULLUP);
    delay(300); // 핀 및 하드웨어 안정화 대기
    
    if (digitalRead(BTN1_PIN) == LOW) {
        display.begin(); // OLED 피드백을 위해 먼저 초기화
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
        WiFiManager wm; // 로컬 객체로 생성하여 초기화
        wm.resetSettings();
        
        logger.addLog("Reset Complete!");
        delay(2000);
        ESP.restart();
    }

    // 2. 일반 부팅 시퀀스
    if (!LittleFS.begin(true)) Serial.println("[SYSTEM] LittleFS Mount Failed");

    // 입력 및 로깅 시스템 초기화
    inputManager.begin();
    inputManager.setCallbacks(0, btn1_short, btn1_long);
    inputManager.setCallbacks(1, btn2_short, btn2_long);
    inputManager.setCallbacks(2, btn3_short, btn3_long);
    inputManager.setCallbacks(3, btn4_short, btn4_long);

    // 디스플레이 초기화 및 시작 피드백
    display.begin();
#if FPS_BENCH_ON_BOOT
    display.runFpsBenchmark();   // 풀 프레임 전송 FPS 측정 (config.h FPS_BENCH_ON_BOOT)
#endif
    display.playStartupMelody();
    
    logger.addLog("Hangeul Clock");
    logger.addLog("Service Unitized");
    
    // 비트맵 캐시 로딩 (이 작업이 무거워서 이전에는 버튼 감지를 방해함)
    display.loadBitmapCache();
    display.setYieldCallback(on_yield);

    // WiFi 연결 시퀀스
    // [2026-11-04] autoConnect()를 쓰지 않는다 — STA 연결을 **비차단**으로 직접 시도한다.
    //   왜: autoConnect()는 내부에서 waitForConnectResult()를 부르는데, _connectTimeout이
    //   0이면 ESP32 코어 기본값 60000ms를 그대로 기다린다(블로킹). 그 동안 loop()가 돌지
    //   않으므로 OLED 도트도 멈춰 기기가 죽은 것처럼 보였고, 사용자가 관찰한 "1분"이 바로
    //   이 60초다. 예전 바깥 while 루프(10회 × 500ms = 5초)는 autoConnect가 반환한 뒤에야
    //   delay를 실행하는 구조라 아무 의미가 없었다.
    //   대신 WiFi.begin()(무인자)은 드라이버/NVS에 저장된 AP로 연결만 걸고 즉시 반환한다
    //   (STA.cpp의 connect() → esp_wifi_get_config()). WiFiManager가 저장 시 자격증명을
    //   NVS에 기록하므로(WiFiManager.cpp의 wifiConnectNew) 찾을 수 있다.
    WiFiManager wm;
    wm.setAPCallback(configModeCallback);
    wm.setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT);

    logger.addLog("WiFi Connecting");

    const unsigned long WIFI_CONNECT_TIMEOUT_MS = 10000;   // 10초 안에 못 붙으면 AP 모드로
    WiFi.begin();
    unsigned long wifiStart = millis();
    int dotCount = 0;
    while (WiFi.status() != WL_CONNECTED && (millis() - wifiStart) < WIFI_CONNECT_TIMEOUT_MS) {
        String dots = "WiFi Connecting";
        for (int j = 0; j <= dotCount % 5; j++) dots += ".";
        logger.updateLastLog(dots);
        delay(500);
        dotCount++;
    }

    // 10초 내 미연결 → 저장된 SSID를 건너뛰고 **곧바로** AP(설정) 모드로.
    //   startConfigPortal()은 autoConnect()와 달리 STA 재시도를 하지 않는다.
    //   setAPCallback이 살아 있어 configModeCallback이 호출되고, 블로킹 모드이므로
    //   setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT)도 그대로 적용된다.
    if (WiFi.status() != WL_CONNECTED) {
        logger.addLog("AP Mode");
        wm.startConfigPortal(WIFI_SSID_AP);
    }

    if (WiFi.status() != WL_CONNECTED) ESP.restart();

    logger.addLog("WiFi Connected!");
    webManager.begin();

    // 5. 시간 동기화 (NTP + POSIX TZ)
    // configManager.begin()이 display.begin() 내부에서 이미 끝났으므로
    // 이 시점의 configManager.get().timezone은 NVS에서 로드·검증된 값이다.
    //   [2026-11-07] timeSync.begin()이 SNTP 내부 재폴링 주기를 1시간으로 낮추고
    //   재동기화 감독을 시작한다. TZ 적용·SNTP 시작은 display.applyTimezone()
    //   → timeSync.applyTimezone() 한 경로로만 일어난다.
    timeSync.begin();
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
    timeSync.loop();   // NTP 재동기화 감독: 주기·WiFi 복구·실패 재시도

    // [2026-11-07] 1초 갱신 트리거를 millis() 1000ms 주기에서 **초 경계(epoch 초) 감지**로
    //   교체했다. 이전 방식은 실제 초 경계와 위상이 어긋나 (1) 화면이 최대 1초 늦게
    //   바뀌고 (2) 렌더 시점이 매초 흔들려 초 표시가 건너뛰거나 겹쳐 보였다.
    //   초 경계는 시스템 시각이 유일한 기준이다. 판정 규칙은 time_sync_policy.h의
    //   clockRenderDue()/secondTickDue() (호스트 단위 테스트 대상).
    //
    //   매 loop마다 읽는 것은 time()(epoch 초)뿐이다 — 비차단이고 저비용이다.
    //   localtime_r()은 실제 렌더가 일어나는 초에만 호출한다.
    //   getLocalTime()은 인자를 생략하면 최대 5초(기본 5000ms) 블로킹이라
    //   메인 루프에서 쓰면 버튼·애니메이션이 그만큼 멈추므로 쓰지 않는다.
    static int64_t lastEpochSec = -1;
    static unsigned long lastFallbackMs = 0;

    const time_t epochNow = time(nullptr);
    const bool timeValid = localTimeIsValid((int64_t)epochNow);
    const bool secondChanged = timeValid && secondTickDue(lastEpochSec, (int64_t)epochNow);
    if (timeValid) lastEpochSec = (int64_t)epochNow;

    const unsigned long now = millis();
    const bool fallbackDue = !timeValid && (now - lastFallbackMs >= UPDATE_INTERVAL_MS);
    // force 플래그는 uiStage가 0일 때만 소비한다 — IP/도움말 화면에서 버튼·웹이 세운
    //   갱신 요청을 잃지 않기 위해서다 (이전에는 매 루프에서 무조건 소비됐다).
    const bool forceUpdate = (uiStage == 0) && display.checkForceUpdate();

    if (uiStage == 0 && clockRenderDue(forceUpdate, timeValid, secondChanged, fallbackDue)) {
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
