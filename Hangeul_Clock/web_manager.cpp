// worynim@gmail.com
/**
 * @file web_manager.cpp
 * @brief 웹 설정 대시보드 및 API 서버 클래스 구현
 * @details 비동기 HTTP 핸들러 및 JSON 설정 데이터 입출력 로직 구현
 * @note [SYNC] ENG_Clock/web_manager.cpp — timezone JSON 입출력 및 검증 추가
 */
#include "web_manager.h"
#include "config.h"          // FONT_SLOT_COUNT
#include "web_pages.h"
#include "tz_util.h"
#include <string.h>   // strlen — JSON 키 파싱 오프셋 계산용

// 전역 객체 정의
WebServer server(WEB_PORT);
WebManager webManager;

WebManager::WebManager() {}

/**
 * @brief JSON 문자열 값 안에 들어갈 수 있도록 이스케이프한다
 * @details [리뷰 §3.3] font_name과 슬롯명은 (1) 이스케이프 없이 JSON 응답에 붙고
 *          (2) "/fN/name.txt" 파일명으로도 쓰인다. 이스케이프는 출력 구멍만 막으므로
 *          setFontName() 쪽에서 **입력**을 먼저 검증한다 (둘 다 있어야 한다).
 * @note UTF-8 연속 바이트(0x80~0xFF)는 그대로 통과시킨다 — 한글 폰트명이 깨지지 않게.
 */
static String jsonEscape(const String& s) {
    String out;
    out.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
        const char c = s[i];
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c < 0x20) { char u[7]; snprintf(u, sizeof(u), "\\u%04x", (unsigned char)c); out += u; }
        else out += c;
    }
    return out;
}

void WebManager::begin() {
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/upload", HTTP_POST, [this]() { server.send(200, "text/plain", "OK"); }, [this]() { handleUploadData(); });
    server.on("/api/refresh_cache", HTTP_POST, [this]() { handleRefreshCache(); });
    server.on("/api/config", HTTP_GET, [this]() { handleGetConfig(); });
    server.on("/api/config", HTTP_POST, [this]() { handleSetConfig(); });
    
    server.begin();
    Serial.println("[WEB] WebManager started");
}

void WebManager::handleClient() {
    server.handleClient();
}

void WebManager::handleRoot() {
    server.send(200, "text/html", font_studio_html);
}

void WebManager::handleUploadData() {
    HTTPUpload& upload = server.upload();
    // ESP32 WebServer는 업로드 콜백에 상태를 넘겨주지 않으므로 파일 핸들을 static으로 유지한다.
    //   단 그 상태를 그대로 신뢰하면 안 된다 — 다음 세 경우를 모두 막아야 한다:
    //     1) 경로 검증 실패로 START가 return하거나, START 없이 WRITE가 먼저 오는 경우
    //        → 이전 업로드의 열린 파일에 이번 내용이 계속 쓰인다
    //     2) 클라이언트 연결 중단(UPLOAD_FILE_ABORTED) → 파일이 열린 채 남는다
    //     3) LittleFS.open 실패 → falsy 핸들이 WRITE마다 검사돼야 한다
    //   uploadActive는 "열린 핸들이 이번 업로드 소유"라는 불변식을 나타낸다.
    static File fsUploadFile;
    static bool uploadActive = false;

    switch (upload.status) {
        case UPLOAD_FILE_START: {
            // 이전 업로드가 미완료로 방치된 경우(경로 거부·연결 중단) 핸들을 정리한다.
            if (uploadActive && fsUploadFile) fsUploadFile.close();
            uploadActive = false;

            String filename = upload.filename;
            if (!filename.startsWith("/")) filename = "/" + filename;

            // 슬롯 파라미터 확인 (예: /upload?slot=1)
            int slot = 0;
            if (server.hasArg("slot")) {
                slot = server.arg("slot").toInt();
                if (slot < 0 || slot >= FONT_SLOT_COUNT) slot = 0;
            }

            String path = "/f" + String(slot);
            if (!LittleFS.exists(path)) LittleFS.mkdir(path);

            String fullPath = path + filename;
            if (fullPath.indexOf("..") != -1) {
                Serial.println("[WEB] Invalid path detected: " + fullPath);
                return;   // uploadActive는 이미 false — 이후 WRITE는 아무 데도 쓰지 않는다
            }
            fsUploadFile = LittleFS.open(fullPath, "w");
            if (!fsUploadFile) {
                Serial.println("[WEB] Failed to open for write: " + fullPath);
                return;   // 열지 못했으므로 uploadActive를 세우지 않는다
            }
            uploadActive = true;
            break;
        }

        case UPLOAD_FILE_WRITE:
            // uploadActive가 false면 이번 업로드에 유효한 파일이 없다 (경로 거부·open 실패).
            //   이전 업로드의 핸들이 남아 있어도 **쓰지 않는다.**
            if (uploadActive && fsUploadFile) {
                fsUploadFile.write(upload.buf, upload.currentSize);
            }
            break;

        case UPLOAD_FILE_END:
            if (uploadActive && fsUploadFile) fsUploadFile.close();
            uploadActive = false;
            break;

        case UPLOAD_FILE_ABORTED:
            // 클라이언트가 전송 도중 끊음 — END 없이 여기서 끝나면 핸들이 새어 나간다.
            if (uploadActive && fsUploadFile) {
                fsUploadFile.close();
                Serial.println("[WEB] Upload aborted by client, handle closed");
            }
            uploadActive = false;
            break;
    }
}

void WebManager::handleRefreshCache() {
    display.loadBitmapCache();
    display.setForceUpdate(true);
    server.send(200, "text/plain", "OK");
}

void WebManager::handleGetConfig() {
    SystemSettings& s = configManager.get();
    String slotNames = "[";
    for (int i = 0; i < FONT_SLOT_COUNT; i++) {
        slotNames += "\"" + jsonEscape(display.getSlotName(i)) + "\"" + (i < FONT_SLOT_COUNT - 1 ? "," : "");
    }
    slotNames += "]";

    String json = "{\"anim_mode\":" + String((int)s.anim_mode) + 
                 ",\"display_mode\":" + String((int)s.display_mode) + 
                 ",\"hour_format\":" + String((int)s.hour_format) + 
                 ",\"chime_enabled\":" + String(s.chime_enabled ? "true":"false") + 
                 ",\"font_name\":\"" + jsonEscape(s.font_name) + 
                 "\",\"font_slot\":" + String((int)s.font_slot) +
                 ",\"slot_names\":" + slotNames +
                 ",\"is_inverted\":" + String(s.is_inverted ? "true":"false") +
                 ",\"brightness\":" + String((int)s.brightness) +
                 ",\"is_flipped\":" + String(s.is_flipped ? "true":"false") +
                 ",\"timezone\":\"" + s.timezone + "\"}";
    server.send(200, "application/json", json);
}

void WebManager::handleSetConfig() {
    if (server.hasArg("plain")) {
        SystemSettings& s = configManager.get();
        String body = server.arg("plain");
        
        int am = parseVal(body, "anim_mode"); 
        if(am >= 0 && am < ANIMATION_TYPE_COUNT && am != s.anim_mode) display.setAnimMode(am);
        
        int dm = parseVal(body, "display_mode"); 
        if(dm >= 0 && dm <= 1 && dm != s.display_mode) display.setDisplayMode(dm);
        
        int hf = parseVal(body, "hour_format"); 
        if(hf >= 0 && hf <= 1 && hf != s.hour_format) display.setHourFormat(hf);
        
        if (body.indexOf("\"chime_enabled\"") != -1) {
            bool val = parseBool(body, "chime_enabled");
            if(val != s.chime_enabled) display.setChime(val);
        }
        if (body.indexOf("\"is_flipped\"") != -1) {
            bool val = parseBool(body, "is_flipped");
            if(val != s.is_flipped) display.setFlipDisplay(val);
        }
        if (body.indexOf("\"is_inverted\"") != -1) {
            bool val = parseBool(body, "is_inverted");
            if(val != s.is_inverted) display.setInversion(val);
        }
        
        int fs = parseVal(body, "font_slot");
        if (fs >= 0 && fs <= 4 && fs != s.font_slot) display.setFontSlot(fs);

        int br = parseVal(body, "brightness");
        if (br >= 1 && br <= 255 && br != s.brightness) display.setBrightness(br);

        // POSIX TZ 문자열. setenv("TZ", ...)로 직접 들어가므로 화이트리스트 검증을 통과해야 한다.
        // font_name과 동일한 문자열 파싱 패턴을 쓰되, 검증은 tz_util::isValidTimezone()에 위임한다
        // — NVS 로드와 웹 입력이 같은 검증기를 써야 한다.
        //
        // 파싱 오프셋은 리터럴 실제 길이(strlen)로 계산한다. 이전처럼 다른 키의 오프셋을
        // 복사하면 "\"timezone\":\""(12자) 어긋나 첫 글자가 유실된다 ("KST-9" → "ST-9").
        static const char TZ_JSON_KEY[] = "\"timezone\":\"";
        int tzS = body.indexOf(TZ_JSON_KEY);
        if (tzS != -1) {
            tzS += strlen(TZ_JSON_KEY);
            int tzE = body.indexOf('"', tzS);
            if (tzE > tzS) {
                String newTz = body.substring(tzS, tzE);
                if (!isValidTimezone(newTz.c_str())) {
                    Serial.println("[WEB] Rejected invalid timezone: " + newTz);
                } else if (newTz != s.timezone) {
                    strncpy(s.timezone, newTz.c_str(), sizeof(s.timezone) - 1);
                    s.timezone[sizeof(s.timezone) - 1] = '\0';   // NUL 종료 보장
                    configManager.setDirty();
                    display.applyTimezone();
                }
            }
        }

        // timezone과 같은 오프셋 버그가 재발하지 않도록 리터럴 길이를 직접 계산한다.
        static const char FONT_JSON_KEY[] = "\"font_name\":\"";
        int fnS = body.indexOf(FONT_JSON_KEY);
        if (fnS != -1) {
            fnS += strlen(FONT_JSON_KEY);
            int fnE = body.indexOf("\"", fnS);
            if (fnE != -1) {
                String fontName = body.substring(fnS, fnE);
                // 길이·문자 검증은 setFontName()이 한다 (검증 규칙을 한 곳에 둔다).
                display.setFontName(fontName);
            }
        }
        
        display.setForceUpdate(true);
        server.send(200, "text/plain", "OK");
    }
}

int WebManager::parseVal(const String& body, const String& key) {
    int pos = body.indexOf("\"" + key + "\":");
    if (pos == -1) return -1;
    int start = pos + key.length() + 2;
    while (start < body.length() && !isDigit(body[start])) start++;
    String v = "";
    while (start < body.length() && isDigit(body[start])) v += body[start++];
    return v.length() > 0 ? v.toInt() : -1;
}

bool WebManager::parseBool(const String& body, const String& key) {
    return body.indexOf("\"" + key + "\":true") != -1;
}
