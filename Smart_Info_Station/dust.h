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