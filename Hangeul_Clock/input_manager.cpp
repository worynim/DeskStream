#include "input_manager.h"

InputManager inputManager;

// 인터럽트 발생 여부를 기록하는 실제 변수 정의
volatile bool btnInterruptFlags[4] = {false, false, false, false};

// 인터럽트 서비스 루틴 (ISR) - 최소한의 작업만 수행
void IRAM_ATTR handleBtn1() { btnInterruptFlags[0] = true; }
void IRAM_ATTR handleBtn2() { btnInterruptFlags[1] = true; }
void IRAM_ATTR handleBtn3() { btnInterruptFlags[2] = true; }
void IRAM_ATTR handleBtn4() { btnInterruptFlags[3] = true; }

// --- Button 클래스 구현 ---

Button::Button(int i, int p) : id(i), pin(p), lastState(HIGH), fallTime(0), isPressed(false), isLongPressFired(false), onShortPress(nullptr), onLongPress(nullptr) {
    pinMode(pin, INPUT_PULLUP);
    if (id == 0) attachInterrupt(digitalPinToInterrupt(pin), handleBtn1, CHANGE);
    else if (id == 1) attachInterrupt(digitalPinToInterrupt(pin), handleBtn2, CHANGE);
    else if (id == 2) attachInterrupt(digitalPinToInterrupt(pin), handleBtn3, CHANGE);
    else if (id == 3) attachInterrupt(digitalPinToInterrupt(pin), handleBtn4, CHANGE);
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
    btnInterruptFlags[id] = false;
}

// --- InputManager 클래스 구현 ---

InputManager::InputManager() {
    btns[0] = nullptr; btns[1] = nullptr; btns[2] = nullptr; btns[3] = nullptr;
}

void InputManager::begin() {
    btns[0] = new Button(0, BTN1_PIN);
    btns[1] = new Button(1, BTN2_PIN);
    btns[2] = new Button(2, BTN3_PIN);
    btns[3] = new Button(3, BTN4_PIN);
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
