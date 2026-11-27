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
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"
#include "widgets_display.h"

// --- 수집 데이터 잠금 ---
// dataTask가 쓰고 loop()가 그리는 전역 데이터(시세·구독자·날씨·미세먼지)와
// 웹에서 바꾸는 수집 설정(채널·지역·날씨 코드)은 이 잠금 안에서만 읽고 쓴다 (String 해제 중 읽기 방지).
extern SemaphoreHandle_t dataMutex;

struct DataLock {
  DataLock() { xSemaphoreTake(dataMutex, portMAX_DELAY); }
  ~DataLock() { xSemaphoreGive(dataMutex); }
};

// --- 데이터 업데이트 관리 구조체 ---
typedef bool (*FetchFn)();   // 수집 성공 시 true

struct WidgetUpdateInfo {
  WidgetType type;
  volatile bool *is_updating;
  uint32_t flag_mask;
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
extern volatile uint32_t update_flag;
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
extern volatile bool is_updating_intl;
extern volatile bool is_updating_fx;
extern volatile bool is_updating_eth;
extern volatile bool is_updating_wc;
extern volatile bool is_updating_sun;
extern volatile bool is_updating_weekly;
extern volatile bool is_updating_tgraph;
extern volatile bool is_updating_stock;

// --- 함수 프로토타입 ---
bool isWidgetActive(WidgetType type);
bool isDomesticMarketOpen();
bool isUsMarketOpen();
bool isFxMarketOpen();

void dataTask(void *pvParameters);

#define MAX_WIDGET_RECORDS 24   // widgets_info 항목 수 상한 (data_manager.cpp에서 static_assert로 확인)

#endif
