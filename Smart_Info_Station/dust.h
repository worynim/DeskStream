// worynim@gmail.com
/**
 * @file dust.h
 * @brief 미세먼지 데이터 선언
 * @details 네이버 모바일 검색 결과에서 지역의 미세먼지(PM10)·초미세먼지(PM2.5) 수치를 읽는다
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
bool get_dust();  // 성공 시 true

#endif