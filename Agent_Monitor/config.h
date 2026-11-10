// worynim@gmail.com
/**
 * @file config.h
 * @brief Agent_Monitor 전역 설정
 * @details 핀·OLED 주소·화면 치수는 DeskStreamCore ds_board.h 를 쓴다.
 */
#ifndef CONFIG_H
#define CONFIG_H

#define AM_FW_VERSION "0.6.1"

// --- 데몬 링크 (USB 시리얼 · BLE UART, JSON Lines) ---
#define AM_BLE_NAME         "DeskStream Agent"   // BLE 광고 이름 — 데몬이 이 이름으로 찾는다
#define AM_SERIAL_BAUD      115200
#define AM_LINE_MAX         2560    // 한 줄 최대 바이트 (넘는 줄은 버린다) — 상세 답변 1728바이트 + 나머지 필드 · 이스케이프 여유
#define AM_LINK_TIMEOUT_MS  6000    // 하트비트(2초)가 이만큼 없으면 "PC 연결 끊김"

// --- 화면 ---
#define AM_FRAME_MS         100     // 다시 그리는 주기 (바뀐 타일만 전송된다)
#define AM_ANIM_STEP_MS     150     // 스피너·점 애니메이션 한 칸
#define AM_BLINK_MS         500     // 승인/입력 대기 화면 반전 깜빡임 주기

// --- 승인 화면 ---
#define AM_PERM_GUARD_MS    700     // 새 승인 요청이 뜬 직후 이 시간 동안 결정 버튼 무시 (다른 걸 누르려다 승인 방지)

// --- 상세 화면 · PC 명령 ---
#define AM_DETAIL_IDLE_MS   60000   // 상세 화면에서 버튼을 이만큼 안 누르면 개요로 돌아간다
#define AM_TOAST_MS         4000    // PC 명령 결과(OK/FAIL) 띠를 보여주는 시간 (실행 중은 계속)

// --- 소리 ---
#define AM_BEEP_MS          40
#define AM_BEEP_HZ          3000

#endif
