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
extern WidgetType SCREEN_MAP[12];

// --- 외부 함수 참조 ---
extern void redraw_current_page();

// --- 웹 서버 핸들러 함수 ---
void handleRoot();
void handleSet();

#endif
