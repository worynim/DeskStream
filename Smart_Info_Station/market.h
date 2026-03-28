#ifndef MARKET_H
#define MARKET_H

#include <Arduino.h>

// --- 시장 데이터 구조체 ---
struct MarketData {
  String price;
  String change;
  String percent;
};

// --- 외부 데이터 참조 (정의는 data_manager.cpp 에서 담당) ---
extern MarketData kospi_data;
extern MarketData kosdaq_data;
extern MarketData snp500_data;
extern MarketData nasdaq_data;
extern MarketData kpi200_data;
extern MarketData futures_data;

// --- 함수 프로토타입 ---
void get_kospi();
void get_kosdaq();
void get_kpi200();
void get_futures();
void get_SnP500();
void get_nasdaq();

#endif
