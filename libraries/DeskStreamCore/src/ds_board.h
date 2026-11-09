// worynim@gmail.com
/**
 * @file ds_board.h
 * @brief DeskStream 보드 공통 핀 맵 · 화면 치수 (hardware.md 기준)
 * @details 모든 서브 프로젝트가 같은 보드를 쓰므로 핀 정의를 한 곳에 둔다.
 *          Arduino 의존이 없어 호스트 테스트에서도 include 할 수 있다.
 */
#ifndef DS_BOARD_H
#define DS_BOARD_H

#include <stdint.h>

namespace ds {

// === 버튼 (INPUT_PULLUP, GND 연결 시 눌림) ===
constexpr uint8_t kNumButtons = 4;
constexpr uint8_t kButtonPins[kNumButtons] = {1, 4, 10, 9};   // BTN1~BTN4

// === 부저 ===
constexpr uint8_t kBuzzerPin = 7;

// === I2C 버스 (OLED 0·1 = HW, OLED 2·3 = SW) ===
constexpr uint8_t kHwSdaPin = 5;
constexpr uint8_t kHwSclPin = 6;
constexpr uint8_t kSwSdaPin = 2;
constexpr uint8_t kSwSclPin = 3;
constexpr uint8_t kOledAddr[2] = {0x3C, 0x3D};   // 버스마다 같은 주소 쌍 (7비트)

// === 화면 (SSD1306 128x64 × 4) ===
constexpr uint8_t  kNumScreens     = 4;
constexpr uint16_t kScreenWidth    = 128;
constexpr uint16_t kScreenHeight   = 64;
constexpr uint8_t  kPagesPerScreen = 8;     // 8px 높이 페이지
constexpr uint8_t  kTilesPerPage   = 16;    // 8px 폭 타일
constexpr uint16_t kScreenBufBytes = kScreenWidth * kPagesPerScreen;   // 1024

}  // namespace ds

#endif
