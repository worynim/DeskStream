/**
 * @file Hangeul_Clock.ino
 * @brief 한글 시계 메인 엔트리 포인트
 * @details 시스템 초기화(Setup), 메인 서비스 루프 제어 및 전역 서비스 통합 관리
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

/**
 * @brief WiFi 설정 모드(AP) 진입 시 호출되는 콜백
 */
void configModeCallback(WiFiManager *myWiFiManager) {
    Serial.println("[WIFI] Config Mode Entered");
    display.u8g2_1.clearBuffer();
    display.u8g2_1.setFont(u8g2_font_6x10_tf);
    display.u8g2_1.drawStr(0, 10, "WiFi Config Mode");
    display.u8g2_1.drawStr(0, 25, "SSID: Hangeul_Clock_Setup");
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
    uint8_t nextMode = (configManager.get().display_mode == CLOCK_MODE_HANGUL) ? CLOCK_MODE_NUMERIC : CLOCK_MODE_HANGUL;
    display.setDisplayMode(nextMode);
    display.clearAll();
}

void btn2_long() {
    display.beep(150, 2000);
    uint8_t nextFormat = (configManager.get().hour_format == HOUR_FORMAT_12H) ? HOUR_FORMAT_24H : HOUR_FORMAT_12H;
    display.setHourFormat(nextFormat);
    display.clearAll();
}

void btn3_short() { 
    display.beep(50, 3000); 
    uint8_t nextAnim = (configManager.get().anim_mode + 1) % 6;
    display.setAnimMode(nextAnim);
}

void btn3_long()  { }

void btn4_short() {
    display.beep(50, 3000);
    uiStage = (uiStage + 1) % UI_STAGE_COUNT;
    display.clearAll();
    if (uiStage == 1) display.showLargeIP(WiFi.localIP());
    else if (uiStage == 2) display.showButtonHelp();
}

void btn4_long() { }

/**
 * @brief 시스템 양보 및 입력 동기화 (애니메이션 루프 등에서 빈번히 호출됨)
 */
void on_yield() {
    webManager.handleClient();
    inputManager.update();
}

void setup() {
    Serial.begin(115200);

    if (!LittleFS.begin(true)) Serial.println("[SYSTEM] LittleFS Mount Failed");

    // 1. 입력 및 로깅 시스템 초기화
    inputManager.begin();
    inputManager.setCallbacks(0, btn1_short, btn1_long);
    inputManager.setCallbacks(1, btn2_short, btn2_long);
    inputManager.setCallbacks(2, btn3_short, btn3_long);
    inputManager.setCallbacks(3, btn4_short, btn4_long);

    // 2. 디스플레이 초기화 및 시작 피드백
    display.begin();
    display.playStartupMelody(); 
    
    logger.addLog("Hangeul Clock v1.9.0");
    logger.addLog("Service Unitized");
    
    // 3. 비트맵 캐시 로딩
    display.loadBitmapCache();
    display.setYieldCallback(on_yield);

    // 4. WiFi 연결 시퀀스
    WiFiManager wm;
    wm.setAPCallback(configModeCallback);
    wm.setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT);
    
    if (digitalRead(BTN1_PIN) == LOW) {
        display.showStatus("WiFi Resetting...");
        wm.resetSettings();
        delay(1000);
    }

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

    // 5. 시간 동기화 (NTP)
    configTime(TIMEZONE_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER1, NTP_SERVER2);
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


void handleClockUpdate(bool force = false) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        display.showStatus("Time Sync Error!");
        return;
    }

    // [시보] 정시 알림 로직
    if (configManager.get().chime_enabled && timeinfo.tm_min == 0 && timeinfo.tm_sec == 0) {
        static int lastChimeHour = -1;
        if (lastChimeHour != timeinfo.tm_hour) {
            display.beep(500, 1000);
            lastChimeHour = timeinfo.tm_hour;
        }
    }

    int h = timeinfo.tm_hour, m = timeinfo.tm_min, s = timeinfo.tm_sec, d = timeinfo.tm_mday;
    String texts[4];
    bool isHangul = (configManager.get().display_mode == CLOCK_MODE_HANGUL);
    bool is24H = (configManager.get().hour_format == HOUR_FORMAT_24H);

    // 텍스트 생성 (v1.4.0 최적화 구조)
    if (is24H) texts[0] = isHangul ? HangeulTimeConverter::getDay(d) : HangeulTimeConverter::getNumericDay(d);
    else texts[0] = HangeulTimeConverter::getAmPm(h);

    texts[1] = isHangul ? HangeulTimeConverter::getHour(h, is24H) : HangeulTimeConverter::getNumericHour(h, is24H);
    texts[2] = isHangul ? HangeulTimeConverter::getMinute(m) : HangeulTimeConverter::getNumericMinute(m);
    texts[3] = isHangul ? HangeulTimeConverter::getSecond(s) : HangeulTimeConverter::getNumericSecond(s);

    // [Step 5.3] '정각' 표현 최적화: 
    // 정시(0분 0초)일 때는 '오전 한시 정각' (Screen 3에 정각)
    // 그 외 0초일 때는 '오전 한시 오분 정각' (Screen 4에 정각) 이 나오도록 명시적으로 보장
    if (isHangul && m == 0 && s == 0) {
        texts[3] = ""; // Screen 4 비우기 (Screen 3가 정각을 담당)
    }

    display.updateAll(texts, force);
}

// 루프 시작
void loop() {
    on_yield(); 

    static unsigned long lastUpdate = 0;
    unsigned long now = millis();
    
    bool needsUpdate = display.checkForceUpdate();

    // IP/도움말 모드가 아니고, (1초가 지났거나 강제 트리거가 발생했을 때) 갱신
    if (uiStage == 0 && (needsUpdate || (now - lastUpdate >= UPDATE_INTERVAL_MS))) {
        handleClockUpdate(needsUpdate);
        lastUpdate = now;
    }
}
