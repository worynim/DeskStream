// worynim@gmail.com
/**
 * @file usdkrw.h
 * @brief USD/KRW 환율 정보 처리
 * @details 실시간 환율 API를 연동하여 외환 정보를 수집하고 전역 상태에 갱신
 */
#ifndef USDKRW_H
#define USDKRW_H

#include <Arduino.h>
#include "market.h" // MarketData 구조체 참조

// --- 환율 데이터 선언 (정의는 data_manager.cpp 에서 담당) ---
extern MarketData usdkrw_data;

// --- 함수 프로토타입 ---
void get_usdkrw();

#endif
