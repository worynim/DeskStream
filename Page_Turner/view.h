// worynim@gmail.com
/**
 * @file view.h
 * @brief 4화면 그리기 — 세로로 쥐는 방향(화면마다 64x128), OLED i는 옆 BTN(i+1)의 역할을 보여준다
 * @details 모든 draw 함수는 4화면을 다 그린 뒤 display.push()까지 한다.
 */
#pragma once
#include <DeskStreamCore.h>

#include "menu.h"
#include "shutter_timer.h"

namespace view {

void begin(ds::DisplayCore& d);
void drawSplash();
void drawNormal(int modeIdx, int slot, bool connected, const ShutterTimer& timer);   // slot 0..2
void drawMenu(const SettingsMenu& menu);
void drawPairing(int slot, uint8_t secondsLeft);   // secondsLeft 0 = 돌아갈 슬롯 없음(취소 불가)
void drawDevices(int selected, int active, const bool paired[3]);   // 기기(슬롯) 선택 화면, selected -1 = 아직 안 고름
void drawSleep();
void setScreensOn(bool on);   // 4화면 패널 끄기/켜기 (버퍼는 그대로)

}  // namespace view
