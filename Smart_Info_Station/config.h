// worynim@gmail.com
/**
 * @file config.h
 * @brief 스마트 정보 스테이션 전역 설정 및 상수 정의
 * @details Wi-Fi 정보, API 키, 핀 맵 및 서비스 갱신 간격 관리
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- 설정 상수 (OLED 페이지 및 위젯) ---
#define MAX_WIDGET_TYPE 13
#define MAX_DATA_PAGE   3   // 데이터 페이지 수 (4번: IP, 5번: 도움말은 고정 시스템 페이지)
#define MAX_PAGE        5   // 전체 페이지 수
#define MAIN_NUM_FONT u8g2_font_maniac_tr

// --- OLED 하드웨어 핀 설정 (상수) ---
const uint8_t HW_SDA_PIN = 5;
const uint8_t HW_SCL_PIN = 6;
const uint8_t SW_SDA_PIN = 2;
const uint8_t SW_SCL_PIN = 3;

// --- 입력 장치 핀 (hardware.md 참조) ---
const int BTN1_PIN = 1;  // 버튼 1 (화면 회전 Flip)
const int BTN2_PIN = 4;  // 버튼 2 (밝기 조절 Bright)
const int BTN3_PIN = 10; // 버튼 3 (자동 루핑 Loop)
const int BTN4_PIN = 9;  // 버튼 4 (페이지 전환 Next Page)
const int BUZZER_PIN = 7; // 부저 핀 (7번)

// --- 데이터 업데이트 주기 ---
const unsigned long INTERVAL_FINANCE = 60 * 1000;    
const unsigned long INTERVAL_SLOW = 30 * 60 * 1000;  

// --- NTP 시각 동기화 ---
// [2026-11-08] 시계 위젯(display_clock_oled)이 시스템 시각을 그대로 표시하므로 누적 오차
//   방지가 필요하다. ESP32-C3의 시스템 시각은 40MHz 크리스털 기반 free-running 카운터라
//   (전형 ±10~30ppm = 하루 1~3초) 부팅 1회 동기화만 믿으면 오차가 쌓인다.
//   아래 값들이 time_sync.cpp의 "동기화 감독" 정책이다 — 근거는 그 파일 주석 참조.
#define NTP_SERVER1 "kr.pool.ntp.org"   // 주 NTP 서버
#define NTP_SERVER2 "time.nist.gov"     // 보조 NTP 서버

#define NTP_SNTP_POLL_MS        3600000UL   // SNTP 내부 재요청 주기 (코어 기본 3시간 → 1시간 안전망)
#define NTP_RESYNC_INTERVAL_MS  21600000UL  // 6시간마다 강제 재동기화 (누적 오차 상한 ≈ 0.2초)
#define NTP_SYNC_STALE_MS       43200000UL  // 12시간 동안 성공 기록이 없으면 즉시 강제
#define NTP_RETRY_INTERVAL_MS   30000UL     // 미동기(부팅 직후·실패) 상태 재시도 간격
#define NTP_MIN_RESYNC_GAP_MS   60000UL     // 연속 재동기화 요청 최소 간격
#define NTP_WIFI_RETRY_MS       60000UL     // WiFi 끊김 시 재접속 시도 간격

/**
 * 📺 위젯 타입 정의
 */
enum WidgetType {
  W_CALENDAR = 0, W_TIME, W_WEATHER, W_DUST, W_YOUTUBE, 
  W_KOSPI, W_KOSDAQ, W_KPI200, W_FUTURES, W_SNP500, 
  W_NASDAQ, W_BTC, W_USDKRW, W_NONE
};

#endif
