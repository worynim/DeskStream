/**
 * @file ota_handler.h
 * @author Antigravity
 * @brief WiFi AP 모드 및 웹 기반 OTA 업로드 기능을 위한 전용 헤더 파일 (디버그 로그 뷰어 포함)
 */

#ifndef OTA_HANDLER_H
#define OTA_HANDLER_H

#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

// --- OTA 및 WiFi 설정 ---
const char* ap_ssid = "ESP32_GPS_DASH";
const char* ap_password = "gps12345";  // AP 비밀번호 (최소 8자)
WebServer server(80);

// OTA 상태 관리 변수
volatile bool is_ota_mode = false;
volatile int ota_progress = 0;

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
 * @brief OTA 업로드 및 로그 뷰어 통합 페이지 (Modern Dark UI)
 */
const char* ota_html = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>GPS Dash OTA & Debug</title>"
"<style>body{font-family:'Segoe UI',sans-serif;background:#0f0f0f;color:#e0e0e0;margin:0;padding:20px;display:flex;flex-direction:column;align-items:center;min-height:100vh;}"
".container{width:100%;max-width:600px;display:flex;flex-direction:column;gap:20px;}"
".card{background:#1a1a1a;padding:25px;border-radius:15px;box-shadow:0 10px 30px rgba(0,0,0,0.5);border:1px solid #333;}"
"h2{color:#00e676;margin-top:0;font-size:20px;display:flex;align-items:center;gap:10px;}"
"p{color:#888;font-size:14px;margin-bottom:20px;}"
"input[type='file']{background:#262626;padding:10px;border-radius:8px;width:100%;box-sizing:border-box;border:1px dashed #444;color:#aaa;margin-bottom:15px;}"
"button{background:linear-gradient(45deg, #00e676, #00c853);color:#0a0a0a;border:none;padding:12px;border-radius:8px;font-weight:bold;cursor:pointer;width:100%;font-size:14px;transition:0.3s;}"
"button:disabled{background:#444;color:#888;cursor:not-allowed;}"
".log-area{background:#000;color:#00ff41;padding:15px;border-radius:10px;font-family:'Consolas',monospace;font-size:12px;height:250px;overflow-y:auto;white-space:pre-wrap;border:1px solid #222;}"
".progress-container{margin-top:15px;background:#262626;border-radius:5px;overflow:hidden;height:8px;display:none;}"
".progress-bar{background:#00e676;width:0%;height:100%;transition:0.2s;}</style></head>"
"<body><div class='container'>"
"<div class='card'><h2>🚀 FW Update</h2><p>무선 펌웨어 업데이트</p>"
"<form id='f' method='POST' action='/update' enctype='multipart/form-data'>"
"<input type='file' name='update' onchange='document.getElementById(\"b\").disabled=false'>"
"<button id='b' type='submit' disabled>업데이트 시작</button>"
"<div class='progress-container' id='pc'><div class='progress-bar' id='p'></div></div></form></div>"
"<div class='card'><h2>📟 Debug Console</h2><div class='log-area' id='l'>Waiting for logs...</div></div>"
"</div>"
"<script>"
"function fetchLogs(){ fetch('/log').then(r=>r.text()).then(t=>{ const l=document.getElementById('l'); l.innerText=t; l.scrollTop=l.scrollHeight; }); }"
"setInterval(fetchLogs, 1000);"
"document.getElementById('f').onsubmit = function(){ "
"document.getElementById('b').disabled=true; document.getElementById('pc').style.display='block';"
"let d = new FormData(this); let x = new XMLHttpRequest();"
"x.upload.onprogress = function(e){ if(e.lengthComputable){ let p = Math.round((e.loaded/e.total)*100); document.getElementById('p').style.width=p+'%'; } };"
"x.open('POST', '/update'); x.send(d); return false; };"
"</script></body></html>";

/**
 * @brief OTA 매니저 초기화 (AP 생성 및 서버 설정)
 */
void initOTA() {
    WiFi.softAP(ap_ssid, ap_password);
    
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", ota_html);
    });

    server.on("/log", HTTP_GET, []() {
        server.send(200, "text/plain", getWebLog());
    });

    server.on("/update", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "SUCCESS");
        delay(500);  // 응답 전송 완료 대기 후 리스타트
        ESP.restart();
    }, []() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
            is_ota_mode = true;
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
            ota_progress = (Update.progress() * 100) / Update.size();
        } else if (upload.status == UPLOAD_FILE_END) {
            Update.end(true);
            is_ota_mode = false;
        }
    });

    server.begin();
    addWebLog("System Initialized. AP Mode Active.");
}

void handleOTA() {
    server.handleClient();
}

#endif // OTA_HANDLER_H
