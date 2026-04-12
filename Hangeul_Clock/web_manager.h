/**
 * @file web_manager.h
 * @brief 웹 설정 대시보드 및 API 서버 클래스 정의
 * @details 비동기 웹 서버를 통한 기기 설정 제어, 폰트 업로드 API 및 실시간 상태 동기화 관리
 */
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
