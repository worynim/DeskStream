// worynim@gmail.com
/**
 * @file btc.h
 * @brief 비트코인 및 암호화폐 시세 정보 처리
 * @details 외부 API로부터 최신 비트코인 시세를 수집하고 캐싱하는 로직 인터페이스
 */
#ifndef BTC_H
#define BTC_H

#include <Arduino.h>
#include "market.h" // MarketData 구조체 참조

// --- 비트코인 데이터 선언 (정의는 data_manager.cpp 에서 담당) ---
extern MarketData btc_data;

// --- 함수 프로토타입 ---
String addCommas(String val);
void get_btc();

#endif
