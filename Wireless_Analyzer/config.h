// worynim@gmail.com
/**
 * @file config.h
 * @brief Wireless_Analyzer 전역 설정
 * @details 핀·화면 치수는 DeskStreamCore ds_board.h 를 쓴다. 스캔 값의 근거는 p0_spike/RESULTS.md.
 */
#ifndef CONFIG_H
#define CONFIG_H

#define WA_FW_VERSION "0.5.0"

// --- WiFi 스캔 ---
#define WA_COUNTRY_CODE     "KR"    // 채널 1~13
#define WA_SCAN_MS_PER_CH   120     // 액티브 스캔 채널당 시간 — 한 번에 약 2.3초 (P0 실측)
#define WA_AP_TTL_MS        60000   // 이만큼 안 보인 AP는 목록에서 지운다 (스캔마다 흔들리는 AP 수 흡수)

// --- 채널 순회 측정 (혼잡도 모드에서 스캔 사이에 한 바퀴) ---
#define WA_SNIFF_DWELL_MS   150     // 채널당 머무는 시간 — 한 바퀴 약 2초 (P0 실측)
#define WA_SNIFF_ALPHA      0.3f    // 바퀴마다 새 값을 섞는 비율 (한 번 측정은 프레임이 적어 흔들린다)

// --- BLE (BLE 화면에서만 스캔) ---
#define WA_BLE_SCAN_MS      3000    // 액티브 스캔 한 번 (P0: 패시브 3초에 15~17대)
#define WA_BLE_TTL_MS       30000   // 이만큼 안 보인 기기는 지운다 — 기기 수 = 최근 30초 동안 본 서로 다른 주소 수

// --- 화면 ---
#define WA_FRAME_MS         200     // 다시 그리는 주기 (바뀐 타일만 전송된다)
#define WA_MODE_TOAST_MS    1500    // 모드를 바꾼 뒤 모드 이름을 띄워 두는 시간 (부팅 직후 첫 모드도)
#define WA_BOOT_MS          1500    // 부팅 화면을 보여주는 최소 시간
#define WA_BRIGHT_STEPS     4
#define WA_BRIGHT_LEVELS    {10, 50, 128, 255}   // BTN4 길게로 돌아가는 밝기 단계 (SSD1306 contrast), 기본 = 마지막(255)
#define WA_RSSI_LO          -95.0f  // 그래프 바닥 (dBm)
#define WA_RSSI_HI          -30.0f  // 그래프 꼭대기 (dBm)

// --- AP 추적 부저 (간격은 analysis beepIntervalMs: −90dBm 1초 ~ −30dBm 0.1초) ---
#define WA_BEEP_MS          30
#define WA_BEEP_HZ          2500
#define WA_TRACK_STALE_MS   2000    // 마지막 유효 기록이 이보다 오래되면 부저를 멈춘다

#endif
