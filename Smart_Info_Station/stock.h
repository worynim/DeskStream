// worynim@gmail.com
/**
 * @file stock.h
 * @brief 관심 종목 — 화면 칸마다 한 종목 (국내·해외, 네이버 증권 실시간 시세)
 * @details 종목은 웹 설정에서 이름으로 검색해 고른다 (기기가 네이버 종목 자동완성을 대신 불러 준다 — web_handlers.cpp).
 *          시세 경로는 네이버 증권 주소의 일부: 국내 "domestic/stock/005930", 해외 "worldstock/stock/AAPL.O".
 */
#ifndef STOCK_H
#define STOCK_H

#include <Arduino.h>
#include "config.h"
#include "market.h"

struct SlotStock {
  String name;   // 종목 이름 (한글, OLED 윗줄) — 비어 있으면 미설정
  String path;   // 시세 경로 (위 형식)
};
extern SlotStock slotStock[SCREEN_SLOTS];

struct StockQuote {
  MarketData data;         // 가격·등락폭·등락률 (쉼표 포함)
  bool closed;             // 마지막으로 받았을 때 장이 닫혀 있었는가
  unsigned long fetched;   // 마지막으로 받은 시각 (millis)
  bool valid;
};
extern StockQuote stockQuote[SCREEN_SLOTS];

/** 시세 경로가 네이버 증권 국내·해외 종목 형식인가 (웹 입력 검사) */
bool isStockPath(const String &path);

bool get_stock();   // 관심 종목 칸을 모두 받으면 true

#endif
