// worynim@gmail.com
/**
 * @file web_manager.h
 * @brief 웹 설정 대시보드 및 API 서버 클래스 정의
 * @details 설정 GET/POST(/api/config)는 ds::WebConfigApi가 config_manager의 필드 테이블로 처리하고,
 *          여기서는 페이지·폰트 업로드·캐시 갱신 같은 이 프로젝트 전용 경로만 담당한다.
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
    // [리뷰 §3.5] 선언만 있고 정의·호출이 없던 handleFileUpload()를 지웠다 —
    //   업로드를 받지 않는다는 사실이 헤더만 보고는 알 수 없었다.
    void handleUploadData();
    
    // API 핸들러
    void handleRefreshCache();

    ds::WebConfigApi _configApi;   // GET/POST /api/config
};

extern WebManager webManager;

#endif
