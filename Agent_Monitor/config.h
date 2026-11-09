// worynim@gmail.com
/**
 * @file config.h
 * @brief Agent_Monitor 전역 설정
 * @details 핀·OLED 주소·화면 치수는 DeskStreamCore ds_board.h 를 쓴다.
 */
#ifndef CONFIG_H
#define CONFIG_H

#define AM_FW_VERSION "0.1.0"

// --- 데몬 링크 (USB 시리얼, JSON Lines) ---
#define AM_SERIAL_BAUD      115200
#define AM_LINE_MAX         256     // 한 줄 최대 바이트 (넘는 줄은 버린다)
#define AM_LINK_TIMEOUT_MS  6000    // 하트비트(2초)가 이만큼 없으면 "PC 연결 끊김"

// --- 화면 ---
#define AM_FRAME_MS         100     // 다시 그리는 주기 (바뀐 타일만 전송된다)
#define AM_ANIM_STEP_MS     150     // 스피너·점 애니메이션 한 칸
#define AM_BLINK_MS         500     // 승인/입력 대기 화면 반전 깜빡임 주기

// --- 소리 ---
#define AM_BEEP_MS          40
#define AM_BEEP_HZ          3000

#endif
