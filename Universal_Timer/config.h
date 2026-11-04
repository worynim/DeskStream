// worynim@gmail.com
/**
 * @file config.h
 * @brief Universal Timer 전역 설정 및 하드웨어 핀 맵
 * @details DeskStream 보드 기반 4-OLED 범용 타이머 설정
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==========================================
// [1] 디스플레이 및 I2C 통신 설정
// ==========================================
#define SCREEN_WIDTH        128         // OLED 개별 화면 가로 해상도
#define SCREEN_HEIGHT       64          // OLED 개별 화면 세로 해상도
#define NUM_SCREENS         4           // 총 연결된 OLED 화면 개수
#define PAGES_PER_SCREEN    8           // 화면당 페이지 수 (64 / 8)

// 디스플레이 및 버튼 물리적 방향 (0: 정순 1, 2, 3, 4)
#define REVERSE_SCREEN_ORDER 0
#define REVERSE_BUTTON_ORDER 0

#define I2C_SPEED_HZ        1000000     // I2C 통신 클럭 속도 (1MHz)
#define I2C_TX_TIMEOUT_MS   50          // 데이터 전송 최대 대기 시간 (ms)
#define I2C_CMD_TIMEOUT_MS  10          // 명령 전송 최대 대기 시간 (ms)
#define I2C_GLITCH_FILTER   7           // I2C 신호 잡음 제거 필터 강도
#define I2C_INTR_PRIO       3           // I2C 인터럽트 우선순위

// I2C 핀 설정 (DeskStream 보드 - ESP32-C3)
#define HW_SDA_PIN          5           // 하드웨어 I2C SDA (OLED 1, 2)
#define HW_SCL_PIN          6           // 하드웨어 I2C SCL (OLED 1, 2)
#define SW_SDA_PIN          2           // 소프트웨어 I2C SDA (OLED 3, 4)
#define SW_SCL_PIN          3           // 소프트웨어 I2C SCL (OLED 3, 4)

// ==========================================
// [2] 버튼 핀 설정
// ==========================================
#define BTN1_PIN            1           // BTN 1: 시(Hour) 조절
#define BTN2_PIN            4           // BTN 2: 분(Minute) 조절
#define BTN3_PIN            10          // BTN 3: 초(Second) 조절
#define BTN4_PIN            9           // BTN 4: 시작/리셋

// ==========================================
// [3] 부저 설정
// ==========================================
#define BUZZER_PIN          7           // 패시브 부저 핀

// ==========================================
// [4] 버튼 동작 타이밍 (ms)
// ==========================================
#define BTN_DEBOUNCE_MS     30          // 민첩한 디바운스 시간
#define BTN_LONG_PRESS_MS   1000        // 길게 누름 판단 시간
#define BTN_REPEAT_INITIAL_MS 300       // 반복 시작 대기 시간 (300ms 후 고속 반복)
#define BTN_REPEAT_FAST_MS  60          // 빠른 반복 간격 (초당 ~16회 연속 증가)

// ==========================================
// [5] 타이머 값 제한
// ==========================================
#define MAX_HOURS           99          // 시 최대값
#define MAX_MINUTES         59          // 분 최대값
#define MAX_SECONDS         59          // 초 최대값

// ==========================================
// [6] 부저 음 설정
// ==========================================
#define BEEP_FREQ           3000        // 버튼 클릭 비프음 주파수 (Hz)
#define BEEP_DURATION_MS    30          // 버튼 클릭 비프음 길이 (ms)
#define ALARM_FREQ          2500        // 알람 주파수 (Hz)
#define ALARM_DURATION_MS   200         // 알람 한 사이클 길이 (ms)
#define ALARM_REPEAT        5           // 알람 반복 횟수
#define ALARM_PAUSE_MS      100         // 알람 사이클 간 멈춤 (ms)

// ==========================================
// [7] 부팅 멜로디 설정
// ==========================================
#define STARTUP_NOTE_1      523         // 도 (C5)
#define STARTUP_NOTE_2      659         // 미 (E5)
#define STARTUP_NOTE_3      784         // 솔 (G5)
#define STARTUP_NOTE_4      1047        // 도 (C6)
#define STARTUP_NOTE_DUR    100         // 각 음 길이 (ms)
#define STARTUP_NOTE_PAUSE  50          // 음 사이 멈춤 (ms)

// ==========================================
// [8] UI 표시 상수
// ==========================================
#define UI_REFRESH_MS       50          // UI 갱신 주기 (ms)
#define COUNTDOWN_TICK_MS   1000        // 카운트다운 1초 틱 (ms)

#endif // CONFIG_H

