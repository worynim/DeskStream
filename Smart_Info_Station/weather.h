// worynim@gmail.com
/**
 * @file weather.h
 * @brief 동네 날씨(기상청 디지털 예보 페이지)·해외 날씨(Open-Meteo) 데이터 선언
 * @details 3시간 간격 3개 시각의 날씨·기온 예보를 수집해 날씨 위젯에 제공
 */
#ifndef WEATHER_H
#define WEATHER_H

#include <Arduino.h>

// --- 날씨 예보 구조체 ---
struct Forecast {
  int time;
  int type;
  int temp;
};

// --- 외부 데이터 참조 (정의는 data_manager.cpp 에서 담당) ---
extern String weather_location_code;
extern String weather_location_name;   // OLED 윗줄에 보일 동네 이름 (웹 동네 검색으로 고른 이름, 비면 코드 표시)
extern String weather_host;
extern String announcement_time;
extern Forecast forecast[3];

// --- 해외 날씨 (Open-Meteo, API 키 불필요) ---
struct IntlForecast {
  int time;   // 현지 시각(시)
  int code;   // WMO 날씨 코드 (0: 맑음)
  int temp;   // 기온(°C, 반올림)
};

extern String intl_city;              // OLED 표시용 도시 이름(영문) — 비어 있으면 미설정
extern float intl_lat, intl_lon;      // 도시 좌표
extern IntlForecast intl_forecast[3]; // 3시간 간격 3개 시각
extern bool intl_valid;               // intl_forecast를 받았는가

// --- 함수 프로토타입 ---
bool get_weather();  // 성공 시 true
bool get_intl_weather();  // 성공 시 true (도시 미설정이면 할 일이 없어 true)

#endif