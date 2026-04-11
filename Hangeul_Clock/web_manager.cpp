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
        fsUploadFile = LittleFS.open("/" + upload.filename, "w");
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
    String json = "{\"anim_mode\":" + String((int)display.anim_mode) + 
                 ",\"display_mode\":" + String((int)display.display_mode) + 
                 ",\"hour_format\":" + String((int)display.hour_format) + 
                 ",\"chime_enabled\":" + String(display.chime_enabled ? "true":"false") + 
                 ",\"font_name\":\"" + display.font_name + 
                 "\",\"is_flipped\":" + String(display.is_flipped ? "true":"false") + "}";
    server.send(200, "application/json", json);
}

void WebManager::handleSetConfig() {
    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        
        int am = parseVal(body, "anim_mode"); if(am != -1) display.setAnimMode(am);
        int dm = parseVal(body, "display_mode"); if(dm != -1) display.setDisplayMode(dm);
        int hf = parseVal(body, "hour_format"); if(hf != -1) display.setHourFormat(hf);
        
        if (body.indexOf("\"chime_enabled\"") != -1) display.setChime(parseBool(body, "chime_enabled"));
        if (body.indexOf("\"is_flipped\"") != -1) display.setFlipDisplay(parseBool(body, "is_flipped"));
        
        int fnS = body.indexOf("\"font_name\":\"");
        if (fnS != -1) {
            fnS += 13;
            int fnE = body.indexOf("\"", fnS);
            if (fnE != -1) display.setFontName(body.substring(fnS, fnE));
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
