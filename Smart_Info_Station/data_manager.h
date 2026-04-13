// worynim@gmail.com
/**
 * @file data_manager.h
 * @brief DataManager 클래스 정의 및 데이터 구조체 선언
 * @details 정보별 데이터 컨테이너 및 API 연동을 위한 전역 서비스 선언
 */
#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "widgets_display.h"

// --- 데이터 업데이트 관리 구조체 ---
typedef void (*FetchFn)();

struct WidgetUpdateInfo {
  WidgetType type;
  volatile bool *is_updating;
  uint16_t flag_mask;
  const char *update_msg;
  void (*render)(U8G2 &); // RenderFn
  FetchFn fetch;
  bool is_finance;
  bool animate;
};

// --- 상수를 관리하는 테이블 및 개수 ---
extern const WidgetUpdateInfo widgets_info[];
extern const int WIDGET_COUNT;

// --- 전역 상태 변수 (extern) ---
extern volatile uint16_t update_flag;
extern portMUX_TYPE updateMux;
extern volatile bool force_update;
extern volatile bool is_waiting;
extern TaskHandle_t dataTaskHandle;

// 업데이트 시각 기록용
extern unsigned long last_finance_update;
extern unsigned long last_slow_update;

// 위젯별 개별 업데이트 상태 플래그
extern volatile bool is_updating_dust;
extern volatile bool is_updating_weather;
extern volatile bool is_updating_youtube;
extern volatile bool is_updating_kospi;
extern volatile bool is_updating_kosdaq;
extern volatile bool is_updating_SnP500;
extern volatile bool is_updating_btc;
extern volatile bool is_updating_usdkrw;
extern volatile bool is_updating_nasdaq;
extern volatile bool is_updating_futures;
extern volatile bool is_updating_kpi200;

// --- 함수 프로토타입 ---
bool isWidgetActive(WidgetType type);
bool isWidgetVisible(WidgetType type);
bool isDomesticMarketOpen();
bool isUsMarketOpen();

void dataTask(void *pvParameters);

#define MAX_WIDGET_RECORDS 16

#endif
