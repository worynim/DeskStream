// worynim@gmail.com
/**
 * @file ButtonManager.cpp
 * @brief 4버튼 비차단 입력 매니저 구현
 * @details 디바운스, 숏/롱 프레스, 누르고 있을 때 가속 반복 입력 처리
 */
#include "ButtonManager.h"

// 정적 멤버 초기화
constexpr uint8_t ButtonManager::PINS[4];

ButtonManager::ButtonManager() {
    // 상태 초기화
    for (int i = 0; i < 4; i++) {
        buttons[i] = { PINS[i], false, false, 0, 0, false };
        events[i] = ButtonEvent::NONE;
    }
}

void ButtonManager::begin() {
    // 모든 버튼 핀을 INPUT_PULLUP 으로 설정 (GND 연결 시 LOW)
    for (int i = 0; i < 4; i++) {
        pinMode(buttons[i].pin, INPUT_PULLUP);
    }
}

void ButtonManager::update() {
    uint32_t now = millis();

    for (int i = 0; i < 4; i++) {
        events[i] = ButtonEvent::NONE;  // 매 프레임 초기화
        bool raw_pressed = (digitalRead(buttons[i].pin) == LOW);

        if (raw_pressed && !buttons[i].pressed) {
            // ── 새로 눌림 (디바운스 적용) ──
            if (now - buttons[i].press_start_ms > BTN_DEBOUNCE_MS) {
                buttons[i].pressed = true;
                buttons[i].press_start_ms = now;
                buttons[i].long_fired = false;
                buttons[i].repeat_started = false;
                buttons[i].last_repeat_ms = now;
            }
        }
        else if (raw_pressed && buttons[i].pressed) {
            // ── 계속 누르고 있는 중 ──
            uint32_t held_ms = now - buttons[i].press_start_ms;

            // 롱 프레스 감지 (1회만 발생)
            if (!buttons[i].long_fired && held_ms >= BTN_LONG_PRESS_MS) {
                buttons[i].long_fired = true;
                events[i] = ButtonEvent::LONG_PRESS;
            }

            // 반복 입력 처리 (롱 프레스 발생 전에도 작동)
            if (!buttons[i].repeat_started) {
                // 초기 대기 시간 후 반복 시작
                if (held_ms >= BTN_REPEAT_INITIAL_MS) {
                    buttons[i].repeat_started = true;
                    buttons[i].last_repeat_ms = now;
                    events[i] = ButtonEvent::REPEAT;
                }
            } else {
                // 빠른 반복 간격으로 반복
                if (now - buttons[i].last_repeat_ms >= BTN_REPEAT_FAST_MS) {
                    buttons[i].last_repeat_ms = now;
                    events[i] = ButtonEvent::REPEAT;
                }
            }
        }
        else if (!raw_pressed && buttons[i].pressed) {
            // ── 버튼에서 손을 뗌 ──
            buttons[i].pressed = false;
            uint32_t held_ms = now - buttons[i].press_start_ms;

            // 숏 프레스: 롱 프레스도 아니고 반복도 시작 안 했을 때만
            if (!buttons[i].long_fired && !buttons[i].repeat_started
                && held_ms >= BTN_DEBOUNCE_MS) {
                events[i] = ButtonEvent::SHORT_PRESS;
            }
        }
    }
}

ButtonEvent ButtonManager::getEvent(int idx) const {
    if (idx < 0 || idx >= 4) return ButtonEvent::NONE;
#if REVERSE_BUTTON_ORDER
    return events[3 - idx];
#else
    return events[idx];
#endif
}

bool ButtonManager::isPressed(int idx) const {
    if (idx < 0 || idx >= 4) return false;
#if REVERSE_BUTTON_ORDER
    return buttons[3 - idx].pressed;
#else
    return buttons[idx].pressed;
#endif
}

// 전역 인스턴스
ButtonManager btnManager;

