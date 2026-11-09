// worynim@gmail.com
/**
 * @file ds_buttons.cpp
 * @brief 4버튼 입력 서비스 구현
 */
#include "ds_buttons.h"

namespace ds {

// 이벤트 비트 → 콜백 칸. 비트 순서와 같다 (SHORT 0x01, LONG 0x02, PRESS 0x04, REPEAT 0x08).
int Buttons::slotOf(ButtonEvent ev) {
    switch (ev) {
        case BUTTON_SHORT:  return 0;
        case BUTTON_LONG:   return 1;
        case BUTTON_PRESS:  return 2;
        case BUTTON_REPEAT: return 3;
        default:            return -1;
    }
}

void Buttons::begin(const ButtonTiming& timing) {
    _timing = timing;
    for (int i = 0; i < kNumButtons; i++) pinMode(kButtonPins[i], INPUT_PULLUP);
}

void Buttons::setCallbacks(uint8_t idx, Callback shortPress, Callback longPress) {
    setCallback(idx, BUTTON_SHORT, shortPress);
    setCallback(idx, BUTTON_LONG, longPress);
}

void Buttons::setCallback(uint8_t idx, ButtonEvent ev, Callback cb) {
    const int slot = slotOf(ev);
    if (idx >= kNumButtons || slot < 0) return;
    _cb[idx][slot] = cb;
}

void Buttons::update() {
    const uint32_t now = millis();
    for (int i = 0; i < kNumButtons; i++) {
        const uint8_t ev = _fsm[i].update(digitalRead(kButtonPins[i]) == HIGH, now, _timing);
        _events[i] = ev;
        for (int slot = 0; slot < 4; slot++) {
            if ((ev & (1u << slot)) && _cb[i][slot]) _cb[i][slot]();
        }
    }
}

}  // namespace ds
