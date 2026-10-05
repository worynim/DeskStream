// worynim@gmail.com
/**
 * @file Chinese_Clock.ino
 * @brief 중국어 단어 시계 메인 엔트리 포인트
 * @details 시스템 초기화(Setup), 메인 서비스 루프 제어 및 전역 서비스 통합 관리
 * @note [SYNC] 원본: ENG_Clock/ENG_Clock.ino — 시간 변환기와 텍스트 생성부만 교체.
 *       4화면 구조는 유지한다 — [요일|오전오후] / 시 / 분 / 초.
 */
#include <Arduino.h>
#include <WiFiManager.h>
#include <time.h>
#include "LittleFS.h"
#include "config.h"
#include "display_manager.h"
#include "chinese_time.h"
#include <WebServer.h>
#include "web_manager.h"
#include "input_manager.h"
#include "logger.h"

/**
 * @brief WiFi 설정 모드(AP) 진입 시 호출되는 콜백
 */
void configModeCallback(WiFiManager *myWiFiManager) {
    Serial.println("[WIFI] Config Mode Entered");
    display.u8g2_1.clearBuffer();
    display.u8g2_1.setFont(u8g2_font_6x10_tf);
    display.u8g2_1.drawStr(0, 10, "WiFi Config Mode");
    display.u8g2_1.drawStr(0, 25, "SSID: " WIFI_SSID_AP);
    display.u8g2_1.drawStr(0, 40, "IP: 192.168.4.1");
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
    inputManager.update();
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
    display.playStartupMelody();

    logger.addLog("Chinese Clock");
    logger.addLog("Service Unitized");

    // 비트맵 캐시 로딩 (이 작업이 무거워서 이전에는 버튼 감지를 방해함)
    display.loadBitmapCache();
    display.setYieldCallback(on_yield);

    // WiFi 연결 시퀀스
    WiFiManager wm;
    wm.setAPCallback(configModeCallback);
    wm.setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT);

    logger.addLog("WiFi Connecting");

    int dotCount = 0;
    while (WiFi.status() != WL_CONNECTED && dotCount < 10) {
        String dots = "WiFi Connecting";
        for (int j = 0; j <= dotCount % 5; j++) dots += ".";
        logger.updateLastLog(dots);
        if (wm.autoConnect(WIFI_SSID_AP)) break;
        delay(500);
        dotCount++;
    }

    if (WiFi.status() != WL_CONNECTED) ESP.restart();

    logger.addLog("WiFi Connected!");
    webManager.begin();

    // 3. 시간 동기화 (NTP + POSIX TZ)
    // configManager.begin()이 display.begin() 내부에서 이미 끝났으므로
    // 이 시점의 configManager.get().timezone은 NVS에서 로드·검증된 값이다.
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

void handleClockUpdate(bool force) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        display.showStatus("Time Sync Error!");
        return;
    }

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

    static unsigned long lastUpdate = 0;
    unsigned long now = millis();

    bool needsUpdate = display.checkForceUpdate();

    // IP/도움말 모드가 아니고, (1초가 지났거나 강제 트리거가 발생했을 때) 갱신
    if (uiStage == 0 && (needsUpdate || (now - lastUpdate >= UPDATE_INTERVAL_MS))) {
        handleClockUpdate(needsUpdate);
        lastUpdate = now;
    }
}