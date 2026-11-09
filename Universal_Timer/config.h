// worynim@gmail.com
/**
 * @file config.h
 * @brief Universal Timer 전역 설정 및 하드웨어 핀 맵
 * @details DeskStream 보드 기반 4-OLED 범용 타이머 설정
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <DeskStreamCore.h>

// ==========================================
// [1] 디스플레이 / 버튼 / 부저
// ==========================================
// 핀·OLED 주소·화면 치수는 DeskStreamCore ds_board.h, I2C 속도(HW 1.1MHz·고속 SW I2C)는
// ds::DisplayConfig 기본값을 쓴다 (libraries/DeskStreamCore).

// 디스플레이 및 버튼 물리적 방향 (0: 정순 1, 2, 3, 4 / 1: 역순 4, 3, 2, 1)
#define REVERSE_SCREEN_ORDER 0
#define REVERSE_BUTTON_ORDER 0

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

