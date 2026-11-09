// worynim@gmail.com
/**
 * @file config.h
 * @brief 스마트 정보 스테이션 전역 설정 및 상수 정의
 * @details Wi-Fi 정보, API 키, 핀 맵 및 서비스 갱신 간격 관리
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <DeskStreamCore.h>   // 공통 핀 맵·화면 엔진·NTP 감독 (libraries/DeskStreamCore)

// --- 설정 상수 (OLED 페이지 및 위젯) ---
#define MAX_WIDGET_TYPE 13
#define MAX_DATA_PAGE   3   // 데이터 페이지 수 (4번: IP, 5번: 도움말은 고정 시스템 페이지)
#define MAX_PAGE        5   // 전체 페이지 수
#define MAIN_NUM_FONT u8g2_font_maniac_tr

// --- 하드웨어 핀 ---
// OLED·버튼(BTN1 Flip / BTN2 Bright / BTN3 Loop / BTN4 Next Page)·부저 핀은 DeskStreamCore ds_board.h가 정의한다.

// --- 데이터 업데이트 주기 ---
const unsigned long INTERVAL_FINANCE = 60 * 1000;    
const unsigned long INTERVAL_SLOW = 30 * 60 * 1000;  

// --- NTP 시각 동기화 ---
// 시계 위젯(display_clock_oled)이 시스템 시각을 그대로 표시하므로 누적 오차 방지가 필요하다.
//   재동기화 감독(6h/12h/30s/1분)은 ds::TimeSync 기본값을 쓴다 (근거: libraries/DeskStreamCore/src/net/ds_time_sync.h).
#define NTP_SERVER1 "kr.pool.ntp.org"   // 주 NTP 서버
#define NTP_SERVER2 "time.nist.gov"     // 보조 NTP 서버

/**
 * 📺 위젯 타입 정의
 */
enum WidgetType {
  W_CALENDAR = 0, W_TIME, W_WEATHER, W_DUST, W_YOUTUBE, 
  W_KOSPI, W_KOSDAQ, W_KPI200, W_FUTURES, W_SNP500, 
  W_NASDAQ, W_BTC, W_USDKRW, W_NONE
};

#endif
