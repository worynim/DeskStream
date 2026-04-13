// worynim@gmail.com
/**
 * @file dust.h
 * @brief 공공데이터 API 기반 대기질 및 미세먼지 정보 처리
 * @details 지정된 측정소의 미세먼지(PM10), 초미세먼지(PM2.5) 농도 수집 및 등급 판정
 */
#ifndef DUST_H
#define DUST_H

#include <Arduino.h>

// --- 미세먼지 데이터 선언 (정의는 data_manager.cpp 에서 담당) ---
extern String dust_location;
extern String dust_host;
extern int dust_10_num;
extern int dust_2_5_num;

// --- 함수 프로토타입 ---
void get_dust();

#endif