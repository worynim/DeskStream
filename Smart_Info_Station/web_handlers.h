// worynim@gmail.com
/**
 * @file web_handlers.h
 * @brief 웹 서버 초기화 및 핸들러 함수 선언
 * @details 외부 웹 자원 연결 및 서버 시작을 위한 인터페이스 정의
 */
#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>
#include "config.h"

// --- 전역 설정 객체 및 변수 참조 ---
extern WebServer server;
extern Preferences preferences;
extern int timezone_offset;
extern bool isLoopingMode;
extern WidgetType SCREEN_MAP[MAX_DATA_PAGE * 4];

// --- 외부 함수 참조 ---
extern void redraw_current_page();

// --- 웹 서버 핸들러 함수 ---
void handleRoot();
void handleSet();

#endif
