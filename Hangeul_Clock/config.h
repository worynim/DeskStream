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
#define NTP_SERVER1 "kr.pool.ntp.org"
#define NTP_SERVER2 "time.nist.gov"
const int TIMEZONE_OFFSET_SEC = 9 * 3600; // 대한민국 UTC+9
const int DAYLIGHT_OFFSET_SEC = 0;

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
#define WIFI_SSID_AP "Hangeul_Clock_Setup"
#define UPDATE_INTERVAL_MS 1000  // 1초마다 갱신

// === [7] 폰트 및 디자인 설정 ===
#define HANGEUL_FONT u8g2_font_unifont_t_korean1  // 경량 한글 폰트 (메모리 절약)
#define STATUS_FONT u8g2_font_6x10_tf           // 상태 메시지용 폰트
#define TEXT_Y_POS 42                            // 한글 텍스트 출력 높이 (0~63)

// === [8] 애니메이션 설정 ===
#define ANIMATION_TYPE_NONE 0
#define ANIMATION_TYPE_SCROLL_UP 1
#define ANIMATION_TYPE_SCROLL_DOWN 2
#define ANIMATION_TYPE_VERTICAL_FLIP 3
#define ANIMATION_TYPE_DITHERED_FADE 4
#define ANIMATION_TYPE_ZOOM 5
#define ANIMATION_STEP_DELAY_MS 10 // 고속 프레임

// === [9] 표시 형식 설정 ===
#define CLOCK_MODE_HANGUL 0
#define CLOCK_MODE_NUMERIC 1
#define HOUR_FORMAT_12H 0
#define HOUR_FORMAT_24H 1

// === [10] 기타 하드코딩 상수 통합 ===
#define I2C_ADDR_HW_0 0x3C
#define I2C_ADDR_HW_1 0x3D
#define CHAR_WIDTH 32
#define MAX_BITMAP_SIZE 512
#define WEB_PORT 80

// === [11] UI 및 버튼 동작 상수 ===
#define UI_STAGE_COUNT 3
#define LONG_PRESS_TIME_MS 1000
#define DEBOUNCE_TIME_MS 50
#define WIFI_CONFIG_TIMEOUT 120

#endif
