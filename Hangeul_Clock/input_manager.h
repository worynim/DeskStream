// worynim@gmail.com
/**
 * @file input_manager.h
 * @brief 사용자 입력 핸들링 및 버튼 서비스 클래스 정의
 * @details 버튼 디바운싱, 짧은/긴 누름 판별 및 인터럽트 안전 콜백 인터페이스 관리
 */
#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <Arduino.h>
#include "config.h"

// 인터럽트 발생 여부를 기록하는 전역 플래그
extern volatile bool btnInterruptFlags[4];

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
