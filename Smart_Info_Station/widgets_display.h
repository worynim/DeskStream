// worynim@gmail.com
/**
 * @file widgets_display.h
 * @brief 위젯 인터페이스 및 드로잉 함수 선언
 * @details 각 정보 소스별 렌더링 함수들을 관리하기 위한 헤더
 */
#ifndef WIDGETS_DISPLAY_H
#define WIDGETS_DISPLAY_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"
#include "display_utils.h"

// 개별 위젯 렌더링 함수 프로토타입
void display_clock_oled(U8G2 &u8g2);
void display_calendar_oled(U8G2 &u8g2);
void display_weather_oled(U8G2 &u8g2);
void display_dust_oled(U8G2 &u8g2);
void display_youtube_oled(U8G2 &u8g2);

// 공통 시세 위젯 표시 함수 
void display_market_oled(U8G2 &u8g2, const String &title, const String &price, const String &change, const String &percent);

// 개별 위젯 래퍼 함수 (Table Driven 배치를 위해 사용)
void display_kospi_oled(U8G2 &u8g2);
void display_kosdaq_oled(U8G2 &u8g2);
void display_SnP500_oled(U8G2 &u8g2);
void display_btc_oled(U8G2 &u8g2);
void display_usdkrw_oled(U8G2 &u8g2);
void display_nasdaq_oled(U8G2 &u8g2);
void display_futures_oled(U8G2 &u8g2);
void display_kpi200_oled(U8G2 &u8g2);

#endif
