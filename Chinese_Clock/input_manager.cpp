// worynim@gmail.com
/**
 * @file input_manager.cpp
 * @brief 사용자 입력 핸들링 및 버튼 서비스 클래스 구현
 * @details 폴링 기반 디바운싱 및 상태 머신을 이용한 입력 판별 구현 (GPIO 인터럽트 미사용)
 * @note [SYNC] 원본: ENG_Clock/input_manager.cpp — 현재 바이트 단위로 동일. 원본 수정 시 함께 반영할 것.
 */
#include "input_manager.h"

InputManager inputManager;

// --- Button 클래스 구현 ---

Button::Button(int i, int p) : id(i), pin(p), lastState(HIGH), fallTime(0), isPressed(false), isLongPressFired(false), onShortPress(nullptr), onLongPress(nullptr) {
    // [제거] attachInterrupt를 쓰지 않는다 — 플래그를 읽는 코드가 없어 ISR이 아무 정보도
    //       전달하지 못했다. update()가 매 루프 digitalRead로 상태 머신을 그대로 돌기 때문에
    //       인터럽트는 GPIO 부하만 늘린다. (블로킹 없이 눌림을 놓치지 않으려면 폴링이 이득)
    pinMode(pin, INPUT_PULLUP);
}

void Button::setCallbacks(void (*sp)(), void (*lp)()) {
    onShortPress = sp;
    onLongPress = lp;
}

void Button::update() {
    bool currentState = digitalRead(pin);
    unsigned long now = millis();

    if (lastState == HIGH && currentState == LOW) { // FALLING
        fallTime = now;
        isPressed = true;
        isLongPressFired = false;
    } 
    else if (currentState == LOW) { // STILL PRESSED
        if (isPressed && !isLongPressFired && (now - fallTime > LONG_PRESS_TIME_MS)) {
            isLongPressFired = true;
            if (onLongPress) onLongPress();
        }
    } 
    else if (lastState == LOW && currentState == HIGH) { // RISING
        if (isPressed && !isLongPressFired && (now - fallTime > DEBOUNCE_TIME_MS)) { 
            if (onShortPress) onShortPress(); 
        }
        isPressed = false;
    }
    
    lastState = currentState;
}

// --- InputManager 클래스 구현 ---

InputManager::InputManager() {
    btns[0] = nullptr; btns[1] = nullptr; btns[2] = nullptr; btns[3] = nullptr;
}

void InputManager::begin() {
    static Button b1(0, BTN1_PIN);
    static Button b2(1, BTN2_PIN);
    static Button b3(2, BTN3_PIN);
    static Button b4(3, BTN4_PIN);
    
    btns[0] = &b1;
    btns[1] = &b2;
    btns[2] = &b3;
    btns[3] = &b4;
}

void InputManager::setCallbacks(int id, void (*shortPress)(), void (*longPress)()) {
    if (id >= 0 && id < 4 && btns[id]) {
        btns[id]->setCallbacks(shortPress, longPress);
    }
}

void InputManager::update() {
    for (int i = 0; i < 4; i++) {
        if (btns[i]) btns[i]->update();
    }
}
