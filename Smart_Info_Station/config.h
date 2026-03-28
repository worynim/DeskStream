#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- 설정 상수 (OLED 페이지 및 위젯) ---
#define MAX_WIDGET_TYPE 13
#define MAX_PAGE 4

// --- OLED 하드웨어 핀 설정 (상수) ---
const uint8_t HW_SDA_PIN = 5;
const uint8_t HW_SCL_PIN = 6;
const uint8_t SW_SDA_PIN = 2;
const uint8_t SW_SCL_PIN = 3;

// --- 입력 장치 핀 ---
const int BUTTON_PIN = 9;

// --- 데이터 업데이트 주기 ---
const unsigned long INTERVAL_FINANCE = 60 * 1000;    
const unsigned long INTERVAL_SLOW = 30 * 60 * 1000;  

/**
 * 📺 위젯 타입 정의
 */
enum WidgetType {
  W_NONE = 0, W_TIME, W_WEATHER, W_DUST, W_YOUTUBE, 
  W_KOSPI, W_KOSDAQ, W_KPI200, W_FUTURES, W_SNP500, 
  W_NASDAQ, W_BTC, W_USDKRW, W_CALENDAR
};

#endif
