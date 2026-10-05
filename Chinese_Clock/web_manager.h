// worynim@gmail.com
/**
 * @file web_manager.h
 * @brief 웹 설정 대시보드 및 API 서버 클래스 정의
 * @details 비동기 웹 서버를 통한 기기 설정 제어, 폰트 업로드 API 및 실시간 상태 동기화 관리
 * @note [SYNC] 원본: ENG_Clock/web_manager.h — 현재 바이트 단위로 동일.
 *       web_manager.cpp에서 date_order 제거(Step 9), script_type + setConfig 분할 추가(Step 11).
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

    /**
     * @brief handleSetConfig의 분할 단위 — **JSON 값의 종류**가 경계다
     * @details 정수·불리언은 parseVal/parseBool로 읽고 범위만 검사하면 되는 반면,
     *          문자열(timezone·font_name)은 별도 파싱(따옴표 위치)과 별도 검증기가 필요하다.
     *          두 종류를 한 함수에 두면 어느 쪽 규칙이 걸렸는지 알 수 없어
     *          "값이 안 바뀌었는데 이유를 알 수 없다"가 된다.
     * @note 각각은 부작용 경계다 — 설정값을 바꾸고 즉시 반영한다(저장은 Lazy Save).
     */
    void applyIntSettings(const String& body);
    void applyBoolSettings(const String& body);
    void applyPresentation(const String& body);
    void applyTimezone(const String& body);
    void applyFontName(const String& body);

    // 헬퍼 메서드
    int parseVal(const String& body, const String& key);
    bool parseBool(const String& body, const String& key);
};

extern WebManager webManager;

#endif
