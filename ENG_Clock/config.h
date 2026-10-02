// worynim@gmail.com
/**
 * @file config.h
 * @brief 프로젝트 전역 설정 및 하드웨어 핀 맵 정의
 * @details I2C 주소, Wi-Fi 초기 접속 정보, 애니메이션 타이밍 등 시스템 상수를 관리
 * @note [SYNC] 원본: Hangeul_Clock/config.h — [1][3][4][5][8][10][11]은 그대로,
 *       [2] NTP·[7] 폰트·[9] 표시 형식은 영어판에 맞게 변경
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// === [1] 하드웨어 핀 정의 (ESP32-C3 기반) ===
#define BTN1_PIN 1
#define BTN2_PIN 4
#define BTN3_PIN 10
#define BTN4_PIN 9

#define HW_SDA_PIN 5
#define HW_SCL_PIN 6
#define SW_SDA_PIN 2
#define SW_SCL_PIN 3

#define BUZZER_PIN 7

// === [2] NTP 및 시간 설정 ===
// 영어판은 고정 오프셋 대신 POSIX TZ 문자열을 쓴다 (DST 자동 처리, PLAN §6.6).
// NTP는 UTC로 수신하고 localtime() 시점에 TZ 환경변수가 적용된다.
#define NTP_SERVER1 "pool.ntp.org"
#define NTP_SERVER2 "time.nist.gov"

// 웹에서 시간대를 설정하기 전까지의 폴백 (config_manager가 NVS에 보관)
#define DEFAULT_TIMEZONE "KST-9"
#define TIMEZONE_MAX_LEN 48   // SystemSettings.timezone 배열 크기와 일치해야 한다

// === [3] 디스플레이 설정 ===
#define NUM_SCREENS 4
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define PAGES_PER_SCREEN 8
#define TILES_PER_PAGE 16

// === [4] I2C 성능 설정 ===
#define I2C_SPEED_HZ 800000 // 1MHz 고속 전송
#define I2C_TX_TIMEOUT_MS 50
#define I2C_CMD_TIMEOUT_MS 10
#define I2C_SYNC_TIMEOUT_MS 100
#define HW_I2C_BUF_SIZE 256

// === [5] RTOS 태스크 설정 ===
#define HW_TASK_STACK 4096
#define HW_TASK_PRIO 10
#define HW_TASK_CORE 0

// === [6] 앱 설정 ===
#define WIFI_SSID_AP "ENG_Clock_Setup"
#define UPDATE_INTERVAL_MS 1000  // 1초마다 갱신

// === [7] 폰트 및 설계 설정 ===
// 한글판의 u8g2_font_unifont_t_korean1은 영문 글리프가 없어 사용 불가.
// logisoso20_tf는 굵은 산세리프라 14px 폴백 렌더링에서 가독성이 가장 좋음.
#define ENGLISH_FONT u8g2_font_logisoso20_tf
#define STATUS_FONT u8g2_font_6x10_tf           // 상태 메시지용 폰트

// 2줄 레이아웃 상수 (PLAN §3.1). 전수 검증으로 확정: 최장 단어 SEVENTEEN(9자).
#define GLYPH_W        14                        // 픽셀 폭 (피치 — 레이아웃 전용)
#define GLYPH_H        32                        // 픽셀 높이
// [레거시·미사용] 래스터 폭은 크기별 기하 테이블(renderer_geometry.cpp)이 결정한다.
//   새 업로드는 6 bytes/row(48px) — GEOM_192B, PLAN §6.13. 남겨두는 이유는 없지만
//   제거가 Hangeul_Clock 동기화 노트와 어긋나므로 주석만 갱신한다.
#define GLYPH_BYTES_PER_ROW 2                    // (구 형식 64B 폰트의 바이트 정렬)
#define GLYPH_SIZE     (GLYPH_BYTES_PER_ROW * GLYPH_H)  // (구 형식) 64 bytes
#define MAX_CHARS_PER_LINE 9                     // 9 × 14 = 126px ≤ 128px
#define MAX_LAYOUT_CHARS   18                    // 2줄 × 9자
#define LINE_HEIGHT    32                        // 2 × 32 = 64px
#define LINE_COUNT     2

// IP 주소 화면(PLAN §6.12): 도트는 마지막 숫자 옆("192.")에 숫자 하단에 맞춰 찍는다.
// IP_DOT_SIZE = 도트 한 변(픽셀), IP_DOT_GAP = 마지막 숫자와 도트 사이의 가로 간격.
#define IP_DOT_SIZE    4
#define IP_DOT_GAP     8

// 폴백 폰트 baseline. 줄마다 LINE_HEIGHT만큼 이동한다.
#define FALLBACK_BASELINE 26

// === [8] 애니메이션 설정 ===
#define ANIMATION_TYPE_NONE 0
#define ANIMATION_TYPE_SCROLL_UP 1
#define ANIMATION_TYPE_SCROLL_DOWN 2
#define ANIMATION_TYPE_VERTICAL_FLIP 3
#define ANIMATION_TYPE_DITHERED_FADE 4
#define ANIMATION_TYPE_ZOOM 5
#define ANIMATION_STEP_DELAY_MS 10 // 고속 프레임

// === [9] 표시 형식 설정 ===
// CLOCK_MODE_WORD는 한글판의 CLOCK_MODE_HANGUL과 같은 값(0)이다.
// NVS 키 "mode"의 기존 값 호환을 위해 값을 유지한다.
#define CLOCK_MODE_WORD 0
#define CLOCK_MODE_NUMERIC 1
#define HOUR_FORMAT_12H 0
#define HOUR_FORMAT_24H 1

// [수정할 사항 3] 24시간제 첫 화면의 날짜 표기 순서. 웹에서 선택한다.
#define DATE_ORDER_DAY_MONTH 0    // "2/10" (일/월, 기본값)
#define DATE_ORDER_MONTH_DAY 1    // "10/2" (월/일)

// === [10] 기타 하드코딩 상수 통합 ===
#define I2C_ADDR_HW_0 0x3C
#define I2C_ADDR_HW_1 0x3D
#define MAX_BITMAP_SIZE 512   // 한글판 512B 폰트도 계속 지원 (숫자 모드)
#define WEB_PORT 80

// === [11] UI 및 버튼 동작 상수 ===
#define UI_STAGE_COUNT 3
#define LONG_PRESS_TIME_MS 1000
#define DEBOUNCE_TIME_MS 50
#define WIFI_CONFIG_TIMEOUT 120
#define DEBUG_MODE 1                             // 1: 디버그 정보 출력 활성화, 0: 비활성화

#endif
