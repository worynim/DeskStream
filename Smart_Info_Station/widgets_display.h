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
void display_intl_weather_oled(U8G2 &u8g2);
void display_world_clock_oled(U8G2 &u8g2, int slot);   // slot: 화면 배치 칸 번호. 호출 측이 DataLock을 잡는다
void display_dday_oled(U8G2 &u8g2, int slot);   // slot: 화면 배치 칸 번호
void display_sun_oled(U8G2 &u8g2, int slot);      // 일출·일몰·달 모양 — 호출 측이 DataLock을 잡는다
void display_weekly_oled(U8G2 &u8g2, int slot);   // 주간 날씨 5일 — 호출 측이 DataLock을 잡는다
void display_fx_oled(U8G2 &u8g2);
void display_eth_oled(U8G2 &u8g2);

// 한국 시계·세계 시계 표시 형식 (웹에서 저장, NVS) — true면 12시간제(AM/PM)
extern bool clock12h;

// D-day 설정 — 화면 배치 칸마다 따로 (웹에서 저장, NVS)
constexpr int DDAY_SLOTS = SCREEN_SLOTS;
struct DdaySetting {
  String label;   // 이름 (한글 가능)
  String date;    // "YYYY-MM-DD", 비어 있으면 미설정
};
extern DdaySetting dday[DDAY_SLOTS];
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
