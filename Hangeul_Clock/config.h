// worynim@gmail.com
/**
 * @file config.h
 * @brief 프로젝트 전역 설정 및 하드웨어 핀 맵 정의
 * @details I2C 주소, Wi-Fi 초기 접속 정보, 애니메이션 타이밍 등 시스템 상수를 관리
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
// 고정 오프셋 대신 POSIX TZ 문자열을 쓴다 (웹에서 설정, DST 자동 처리).
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
#define ANIMATION_TYPE_SNOW_ASSEMBLE 6
#define ANIMATION_TYPE_SPLIT_FLAP 7
#define ANIMATION_TYPE_COUNT 8      // BTN3 순환 및 웹 입력 검증 상한
#define ANIMATION_STEP_DELAY_MS 10  // 고속 프레임 (기존 모드 공통)

#define ANIMATION_STEPS_DEFAULT 16  // 기존 모드 및 분할 플랩 프레임 수
#define ANIMATION_STEP_DELAY_SNOW_MS 16
#define ANIMATION_STEPS_SNOW 48    // 눈 조립 모드: 48 x 16ms = 약 0.8초
#define ANIM_SNOW_FALLBACK_THRESHOLD 128 // 픽셀 조립이 불가능한 글자는 진행도 절반 이후에 시스템 폰트로 그린다
#define ANIM_PROGRESS_FULL 255          // 픽셀/행 단위 애니메이션 진행도의 만점 (스텝 → 0~255 매핑 상한)

// 분할 플랩 접힘 기하 (web_pages.h의 FLAP_HALF_H / FLAP_PHASE_SPLIT과 맞춰야 한다)
#define ANIM_FLAP_HALF_H (SCREEN_HEIGHT / 2) // 글자를 접는 축의 높이 절반 (위쪽 0~31, 아래쪽 32~63)
#define ANIM_FLAP_PHASE_SPLIT 128              // 이 진행도부터 아래쪽 절반 접힘으로 넘어간다

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
#define DEBUG_MODE 1                             // 1: 디버그 정보 출력 활성화, 0: 비활성화

#endif
