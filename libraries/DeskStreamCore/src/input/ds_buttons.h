// worynim@gmail.com
/**
 * @file ds_buttons.h
 * @brief 4버튼 입력 서비스 (폴링 · 콜백 또는 이벤트 조회)
 * @details 메인 루프에서 update()를 자주 부른다. GPIO 인터럽트는 쓰지 않는다 —
 *          상태 머신이 매 루프 digitalRead로 판정하므로 ISR은 부하만 늘린다.
 *          판정 규칙은 ds_button_fsm.h(순수 함수, 호스트 테스트 대상)에 있다.
 *
 *          두 방식을 함께 쓸 수 있다:
 *            · 콜백: setCallbacks(idx, 짧게, 길게) / setCallback(idx, BUTTON_PRESS, fn)
 *            · 조회: update() 뒤 events(idx) & BUTTON_SHORT … (이번 update에서 난 이벤트 비트)
 * @note 출처: Hangeul_Clock/input_manager.* (v2.10.0) + Universal_Timer/ButtonManager.*
 */
#ifndef DS_BUTTONS_H
#define DS_BUTTONS_H

#include <Arduino.h>
#include "../ds_board.h"
#include "ds_button_fsm.h"

namespace ds {

class Buttons {
public:
    typedef void (*Callback)();

    /** 핀을 INPUT_PULLUP으로 설정한다. 핀 순서는 ds_board.h의 kButtonPins (BTN1~BTN4) */
    void begin(const ButtonTiming& timing = ButtonTiming());

    /** 버튼 idx(0~3)의 짧게/길게 콜백을 등록한다 (nullptr = 무시) */
    void setCallbacks(uint8_t idx, Callback shortPress, Callback longPress);

    /** 버튼 idx의 특정 이벤트(BUTTON_PRESS·BUTTON_REPEAT 등 한 비트) 콜백을 등록한다 */
    void setCallback(uint8_t idx, ButtonEvent ev, Callback cb);

    /** 모든 버튼 상태를 갱신하고 발생한 이벤트의 콜백을 부른다 */
    void update();

    /** 마지막 update()에서 버튼 idx에 난 이벤트 비트 (BUTTON_SHORT | BUTTON_REPEAT …) */
    uint8_t events(uint8_t idx) const { return idx < kNumButtons ? _events[idx] : 0; }

    /** 버튼 idx가 지금 눌려 있는가 */
    bool isPressed(uint8_t idx) const { return idx < kNumButtons && _fsm[idx].pressed; }

private:
    static int slotOf(ButtonEvent ev);

    ButtonTiming _timing;
    ButtonFsm _fsm[kNumButtons];
    uint8_t _events[kNumButtons] = {0, 0, 0, 0};
    Callback _cb[kNumButtons][4] = {};   // [버튼][SHORT, LONG, PRESS, REPEAT]
};

}  // namespace ds

#endif
