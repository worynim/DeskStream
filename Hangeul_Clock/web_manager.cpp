// worynim@gmail.com
/**
 * @file web_manager.cpp
 * @brief 웹 설정 대시보드 및 API 서버 클래스 구현
 * @details 비동기 HTTP 핸들러 및 JSON 설정 데이터 입출력 로직 구현
 */
#include "web_manager.h"
#include "web_pages.h"

// 전역 객체 정의
WebServer server(WEB_PORT);
WebManager webManager;

WebManager::WebManager() {}

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
    static File fsUploadFile;
    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        if (!filename.startsWith("/")) filename = "/" + filename;
        
        // 슬롯 파라미터 확인 (예: /upload?slot=1)
        int slot = 0;
        if (server.hasArg("slot")) {
            slot = server.arg("slot").toInt();
            if (slot < 0 || slot >= 5) slot = 0;
        }
        
        String path = "/f" + String(slot);
        if (!LittleFS.exists(path)) LittleFS.mkdir(path);
        
        String fullPath = path + filename;
        if (fullPath.indexOf("..") != -1) {
            Serial.println("[WEB] Invalid path detected: " + fullPath);
            return; 
        }
        fsUploadFile = LittleFS.open(fullPath, "w");
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (fsUploadFile) fsUploadFile.close();
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
    for(int i=0; i<5; i++) {
        slotNames += "\"" + display.getSlotName(i) + "\"" + (i < 4 ? "," : "");
    }
    slotNames += "]";

    String json = "{\"anim_mode\":" + String((int)s.anim_mode) + 
                 ",\"display_mode\":" + String((int)s.display_mode) + 
                 ",\"hour_format\":" + String((int)s.hour_format) + 
                 ",\"chime_enabled\":" + String(s.chime_enabled ? "true":"false") + 
                 ",\"font_name\":\"" + s.font_name + 
                 "\",\"font_slot\":" + String((int)s.font_slot) +
                 ",\"slot_names\":" + slotNames +
                 ",\"is_inverted\":" + String(s.is_inverted ? "true":"false") +
                 ",\"is_flipped\":" + String(s.is_flipped ? "true":"false") + "}";
    server.send(200, "application/json", json);
}

void WebManager::handleSetConfig() {
    if (server.hasArg("plain")) {
        SystemSettings& s = configManager.get();
        String body = server.arg("plain");
        
        int am = parseVal(body, "anim_mode"); 
        if(am >= 0 && am <= 5 && am != s.anim_mode) display.setAnimMode(am);
        
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

        int fnS = body.indexOf("\"font_name\":\"");
        if (fnS != -1) {
            fnS += 13;
            int fnE = body.indexOf("\"", fnS);
            if (fnE != -1) {
                String fontName = body.substring(fnS, fnE);
                // 파일명 검증: 간단한 길이 및 문자 제한
                if (fontName.length() > 0 && fontName.length() < 32) {
                    display.setFontName(fontName);
                }
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
