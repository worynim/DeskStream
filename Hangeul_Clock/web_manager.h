#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "display_manager.h"
#include "config.h"

class WebManager {
public:
    WebManager();
    void begin();
    void handleClient();

private:
    // 루트 페이지 핸들러
    void handleRoot();
    
    // 파일 업로드 핸들러
    void handleFileUpload();
    void handleUploadData();
    
    // API 핸들러
    void handleRefreshCache();
    void handleGetConfig();
    void handleSetConfig();

    // 헬퍼 메서드
    int parseVal(const String& body, const String& key);
    bool parseBool(const String& body, const String& key);
};

extern WebManager webManager;

#endif
