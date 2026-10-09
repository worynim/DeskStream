// worynim@gmail.com
/**
 * @file bb_diagnostic.h
 * @brief 하드웨어 진단 진입점 (BTN4를 누른 채 부팅하면 실행된다)
 * @note [v1.0.5] 컴파일 스위치(BB_DIAG_MODE)를 없앴다. 그 스위치가 클라우드 동기화로
 *       되돌아가 "시계 대신 진단이 도는" 상황을 사용자가 알 수 없게 만들었다.
 *       지금은 **런타임 트리거**라 되돌아갈 스위치가 없다.
 */
#ifndef BB_DIAGNOSTIC_H
#define BB_DIAGNOSTIC_H

#include "config.h"

/**
 * @brief 부팅 시 진단 모드 진입 조건을 판정한다
 * @return true면 진단 모드로 들어가야 한다 (BTN4가 계속 눌려 있음)
 * @note 호출 전에 `pinMode(BB_DIAG_BUTTON_PIN, INPUT_PULLUP)`이 되어 있어야 한다.
 *       버튼을 놓을 때까지 기다리므로, 진단을 원하지 않으면 즉시 false로 빠진다.
 */
bool bbDiagnosticRequested();

/**
 * @brief BitBang 버스 진단을 실행하고 **돌아오지 않는다**
 * @details WiFi·NTP·시계를 모두 건너뛰고 여기서 무한 루프한다.
 */
void runBitBangDiagnostics();

#endif  // BB_DIAGNOSTIC_H
