#ifndef USDKRW_H
#define USDKRW_H

#include <Arduino.h>
#include "market.h" // MarketData 구조체 참조

// --- 환율 데이터 선언 (정의는 data_manager.cpp 에서 담당) ---
extern MarketData usdkrw_data;

// --- 함수 프로토타입 ---
void get_usdkrw();

#endif
