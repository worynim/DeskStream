// worynim@gmail.com
/**
 * @file weather.h
 * @brief OpenWeatherMap API를 이용한 실시간 날씨 정보 처리
 * @details 현재 온도, 습도, 기상 상태 및 예보 데이터를 수집하고 시각화 데이터로 변환
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
extern String weather_host;
extern String announcement_time;
extern Forecast forecast[3];

// --- 함수 프로토타입 ---
void get_weather();

#endif