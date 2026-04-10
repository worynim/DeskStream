#include <Arduino.h>
#include <WiFiManager.h>
#include <time.h>
#include "LittleFS.h"
#include "config.h"
#include "display_manager.h"
#include "korean_time.h"
#include <WebServer.h>
#include "web_manager.h"

DisplayManager display;
WebServer server(80);

/**
 * @brief WiFi 설정 모드(AP) 진입 시 호출되는 콜백
 */
void configModeCallback(WiFiManager *myWiFiManager) {
    Serial.println("[WIFI] Config Mode Entered");
    display.u8g2_1.clearBuffer();
    display.u8g2_1.setFont(u8g2_font_6x10_tf);
    display.u8g2_1.drawStr(0, 10, "WiFi Config Mode");
    display.u8g2_1.drawStr(0, 25, "SSID: Korean_Clock_Setup");
    display.u8g2_1.drawStr(0, 40, "IP: 192.168.4.1");
    display.u8g2_1.sendBuffer();
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n[SYSTEM] Starting Korean Clock High Performance...");

    // 파일 시스템 초기화
    if (!LittleFS.begin(true)) {
        Serial.println("[SYSTEM] LittleFS Mount Failed");
    }

    // 버튼 설정
    pinMode(BTN1_PIN, INPUT_PULLUP);
    pinMode(BTN2_PIN, INPUT_PULLUP);
    pinMode(BTN3_PIN, INPUT_PULLUP);
    pinMode(BTN4_PIN, INPUT_PULLUP);

    // 1. 디스플레이 초기화
    display.begin();
    display.showStatus("Initializing...");

    // 2. WiFi 연결 (WiFiManager)
    WiFiManager wm;
    wm.setAPCallback(configModeCallback);
    wm.setConfigPortalTimeout(120); // 2분 후 타임아웃
    
    // 부팅 시 BTN1 눌림 감지하여 WiFi 설정 강제 초기화 기능
    pinMode(BTN1_PIN, INPUT_PULLUP);
    if (digitalRead(BTN1_PIN) == LOW) {
        Serial.println("[WIFI] Resetting Settings...");
        display.showStatus("WiFi Resetting...");
        wm.resetSettings();
        delay(1000);
    }

    display.showStatus("WiFi Connecting...");
    if (!wm.autoConnect(WIFI_SSID_AP)) {
        Serial.println("[WIFI] Failed to connect and hit timeout");
        display.showStatus("WiFi Failed! Restarting...");
        delay(3000);
        ESP.restart();
    }

    Serial.println("[WIFI] Connected!");
    display.showStatus("WiFi Connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // 웹 서버 시작
    startWebServer();

    // 3. NTP 설정
    configTime(TIMEZONE_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER1, NTP_SERVER2);
    display.showStatus("Syncing Time...");
    
    // 시간 동기화 완료 대기 (최대 10초)
    struct tm timeinfo;
    int retry = 0;
    while (!getLocalTime(&timeinfo) && retry < 10) {
        Serial.println("[NTP] Waiting for time sync...");
        delay(1000);
        retry++;
    }

    if (retry < 10) {
        Serial.println("[NTP] Time Synchronized");
        display.showStatus("Time Sync OK!");
    } else {
        Serial.println("[NTP] Time Sync Failed");
        display.showStatus("Time Sync Fail!");
    }
    
    delay(1000);
}

void loop() {
    server.handleClient();

    // BTN4 눌림 감지: IP 주소 표시
    if (digitalRead(BTN4_PIN) == LOW) {
        display.showStatus("IP: " + WiFi.localIP().toString());
        delay(3000); // 3초간 표시 (단발성 확인용)
    }
    
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();

    if (now - lastUpdate >= UPDATE_INTERVAL_MS) {
        lastUpdate = now;

        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            int h = timeinfo.tm_hour;
            int m = timeinfo.tm_min;
            int s = timeinfo.tm_sec;

            // 한글 변환
            String ampmStr = KoreanTimeConverter::getAmPm(h);
            String hourStr = KoreanTimeConverter::getHour(h);
            String minStr = KoreanTimeConverter::getMinute(m);
            String secStr = KoreanTimeConverter::getSecond(s);

            // 시리얼 출력 (디버깅용)
            Serial.printf("[TIME] %02d:%02d:%02d -> %s %s %s %s\n", 
                          h, m, s, ampmStr.c_str(), hourStr.c_str(), minStr.c_str(), secStr.c_str());

            // 4개 OLED에 그리기 (버퍼링)
            display.drawCenterText(0, ampmStr);
            display.drawCenterText(1, hourStr);
            display.drawCenterText(2, minStr);
            display.drawCenterText(3, secStr);

            // 실제 전송 (병렬 처리)
            display.pushParallel();
        } else {
            Serial.println("[SYSTEM] Failed to obtain time");
            display.showStatus("Time Sync Error!");
        }
    }
    
    // 버튼 처리 등 추가 로직이 필요한 경우 여기에 작성
}
