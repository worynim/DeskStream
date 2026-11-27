// worynim@gmail.com
/**
 * @file web_log.h
 * @brief WiFi AP 웹 디버그 로그 뷰어
 * @details WiFi AP를 열고 브라우저에서 실시간 디버그 로그를 보여 준다 (5분 뒤 자동 종료).
 *          펌웨어 업데이트는 다른 프로젝트와 같이 웹 플래셔·런처로 한다 (웹 OTA 없음).
 */

#ifndef WEB_LOG_H
#define WEB_LOG_H

#include <WiFi.h>
#include <WebServer.h>

// --- WiFi AP 설정 ---
const char* ap_ssid = "ESP32_GPS_DASH";
const char* ap_password = "gps12345";  // AP 비밀번호 (최소 8자)
WebServer server(80);

// --- WiFi AP 자동 종료 시스템 (전력 절감) ---
const unsigned long AP_TIMEOUT_MS = 5UL * 60 * 1000;  // 5분
unsigned long _apStartTime = 0;
bool _apActive = true;

// 웹 디버그 로그용 링 버퍼 (힙 단편화 완전 방지)
const int MAX_LOG_ENTRIES = 20;           // 최대 보관 로그 수
const int MAX_LOG_LINE_LEN = 160;         // 한 줄 최대 길이
char logBuffer[MAX_LOG_ENTRIES][MAX_LOG_LINE_LEN];
int logWriteIdx = 0;                      // 다음 쓰기 위치
int logCount = 0;                         // 현재 저장된 로그 수

/**
 * @brief 링 버퍼에 디버그 로그 추가 (O(1), 힙 할당 없음)
 */
void addWebLog(const char* msg) {
    snprintf(logBuffer[logWriteIdx], MAX_LOG_LINE_LEN, "[%lus] %s", millis()/1000, msg);
    logWriteIdx = (logWriteIdx + 1) % MAX_LOG_ENTRIES;
    if (logCount < MAX_LOG_ENTRIES) logCount++;
}

/**
 * @brief 링 버퍼의 로그를 시간순으로 조립하여 반환 (/log 요청 시에만 호출)
 */
String getWebLog() {
    String result;
    result.reserve(logCount * 80);  // 대략적 크기 예약으로 재할당 최소화
    // 가장 오래된 로그부터 순서대로 출력
    int startIdx = (logCount < MAX_LOG_ENTRIES) ? 0 : logWriteIdx;
    for (int i = 0; i < logCount; i++) {
        int idx = (startIdx + i) % MAX_LOG_ENTRIES;
        result += logBuffer[idx];
        result += "\n";
    }
    return result;
}

/**
 * @brief 디버그 로그 뷰어 페이지 (Modern Dark UI)
 */
const char* log_html = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>GPS Dash Debug</title>"
"<style>body{font-family:'Segoe UI',sans-serif;background:#0f0f0f;color:#e0e0e0;margin:0;padding:20px;display:flex;flex-direction:column;align-items:center;min-height:100vh;}"
".container{width:100%;max-width:600px;display:flex;flex-direction:column;gap:20px;}"
".card{background:#1a1a1a;padding:25px;border-radius:15px;box-shadow:0 10px 30px rgba(0,0,0,0.5);border:1px solid #333;}"
"h2{color:#00e676;margin-top:0;font-size:20px;display:flex;align-items:center;gap:10px;}"
"p{color:#888;font-size:14px;margin-bottom:20px;}"
".log-area{background:#000;color:#00ff41;padding:15px;border-radius:10px;font-family:'Consolas',monospace;font-size:12px;height:250px;overflow-y:auto;white-space:pre-wrap;border:1px solid #222;}"
"</style></head>"
"<body><div class='container'>"
"<div class='card'><h2>📟 Debug Console</h2><div class='log-area' id='l'>Waiting for logs...</div></div>"
"</div>"
"<script>"
"function fetchLogs(){ fetch('/log').then(r=>r.text()).then(t=>{ const l=document.getElementById('l'); l.innerText=t; l.scrollTop=l.scrollHeight; }); }"
"setInterval(fetchLogs, 1000);"
"</script></body></html>";

/**
 * @brief 웹 로그 뷰어 초기화 (AP 생성 및 서버 설정)
 */
void initWebLog() {
    WiFi.softAP(ap_ssid, ap_password);
    _apStartTime = millis();  // AP 시작 시각 기록
    
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", log_html);
    });

    server.on("/log", HTTP_GET, []() {
        server.send(200, "text/plain", getWebLog());
    });

    server.begin();
    addWebLog("System Initialized. AP active for 5 min.");
    Serial.printf("[WiFi] AP '%s' started (auto-off in 5min)\n", ap_ssid);
}

/**
 * @brief WiFi AP 5분 자동 종료 확인 (접속자 있으면 연장)
 */
void checkAPTimeout() {
    if (!_apActive) return;  // 이미 꺼졌으면 무시
    
    if (millis() - _apStartTime > AP_TIMEOUT_MS) {
        if (WiFi.softAPgetStationNum() == 0) {
            // 접속자 없음 — AP 종료하여 전력 절감
            server.stop();
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_OFF);
            _apActive = false;
            Serial.println("[WiFi] AP auto-shutdown (5min, no clients)");
        } else {
            // 접속자 있음 — 타이머 연장 (1분 추가)
            _apStartTime = millis() - AP_TIMEOUT_MS + 60000;
            Serial.printf("[WiFi] Client connected, extending AP (+1min)\n");
        }
    }
}

void handleWebLog() {
    if (_apActive) {
        server.handleClient();
        checkAPTimeout();
    }
}

#endif // WEB_LOG_H
