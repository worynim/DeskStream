// worynim@gmail.com
/**
 * @file input_manager.h
 * @brief 사용자 입력 핸들링 및 버튼 서비스 클래스 정의
 * @details 버튼 디바운싱, 짧은/긴 누름 판별 및 콜백 인터페이스 관리
 *          펄링 방식이므로 GPIO 인터럽트는 쓰지 않는다 (Button::update가 상태 머신을 전부 처리)
 * @note [SYNC] 원본: ENG_Clock/input_manager.h — 현재 바이트 단위로 동일. 원본 수정 시 함께 반영할 것.
 */
#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <Arduino.h>
#include "config.h"

/**
 * @brief 인터럽트 기반 고전능 버튼 클래스
 */
class Button {
public:
    int id;
    int pin;
    bool lastState;
    unsigned long fallTime;
    bool isPressed;
    bool isLongPressFired;
    void (*onShortPress)();
    void (*onLongPress)();

    Button(int i, int p);
    void setCallbacks(void (*sp)(), void (*lp)());
    void update();
};

/**
 * @brief 시스템 통합 입력 관리자 클래스
 */
class InputManager {
public:
    InputManager();
    void begin();
    void update();
    void setCallbacks(int id, void (*shortPress)(), void (*longPress)());

private:
    Button* btns[4];
};

extern InputManager inputManager;

#endif
