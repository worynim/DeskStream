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
extern String weather_host;
extern String announcement_time;
extern Forecast forecast[3];

// --- 함수 프로토타입 ---
void get_weather();

#endif