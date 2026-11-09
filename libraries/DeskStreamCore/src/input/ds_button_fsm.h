// worynim@gmail.com
/**
 * @file ds_button_fsm.h
 * @brief 버튼 1개의 누름 판정 상태 머신 (순수 함수 — Arduino 의존 없음)
 * @details 입력은 핀 레벨(HIGH = 놓임, INPUT_PULLUP)과 현재 시각뿐이므로 호스트 단위 테스트
 *          (test/test_button_fsm.cpp)가 가능하다. 실제 핀 읽기·콜백 호출은 ds_buttons가 한다.
 *
 *          이벤트 (한 번의 update에서 여러 개가 동시에 날 수 있어 **비트마스크**로 돌려준다):
 *            · PRESS  : 눌린 순간 1회 (반응이 즉각적이어야 하는 버튼 — Game_Of_Life)
 *            · LONG   : 누른 채 longPressMs를 **넘기면** 놓기 전에 즉시 1회
 *            · REPEAT : repeatDelayMs > 0이면, 누른 채 repeatDelayMs 이후 1회, 그 뒤 repeatIntervalMs마다
 *            · SHORT  : 놓을 때 — 길게·반복이 나지 않았고 누른 시간이 debounceMs를 **넘겼으면**
 *          채터링: stableMs > 0이면 레벨이 stableMs 넘게 그대로일 때만 바뀐 것으로 본다(안정 디바운스).
 *          0이면 원시 레벨을 쓰고, 누름/뗌 바운스는 debounceMs 이하의 짧은 누름이 되어 SHORT에서 걸러진다.
 * @note 시각 비교는 uint32_t 뺄셈이다 — millis() 49.7일 랩에서도 경과 시간이 맞다.
 * @note 출처: Hangeul_Clock input_manager(짧게/길게) + Universal_Timer ButtonManager(자동 반복)
 *       + Game_Of_Life buttonTask(안정 디바운스·누름 즉시). UT에서 1000ms 시점에 REPEAT가 LONG을
 *       덮어쓰던 문제는 비트마스크로 사라졌다.
 */
#ifndef DS_BUTTON_FSM_H
#define DS_BUTTON_FSM_H

#include <stdint.h>

namespace ds {

struct ButtonTiming {
    uint32_t debounceMs = 50;        ///< SHORT로 인정할 최소 누름 시간 (초과)
    uint32_t longPressMs = 1000;     ///< LONG 시점 (초과)
    uint32_t repeatDelayMs = 0;      ///< 첫 REPEAT까지 (0 = 반복 끔)
    uint32_t repeatIntervalMs = 60;  ///< 이후 REPEAT 간격
    uint32_t stableMs = 0;           ///< 안정 디바운스 (0 = 끔)
};

/** 이벤트 비트 — update()의 반환값은 이 값들의 OR */
enum ButtonEvent : uint8_t {
    BUTTON_NONE   = 0,
    BUTTON_SHORT  = 0x01,
    BUTTON_LONG   = 0x02,
    BUTTON_PRESS  = 0x04,
    BUTTON_REPEAT = 0x08,
};

struct ButtonFsm {
    bool lastHigh = true;       // 직전 (안정) 레벨 (HIGH = 놓임)
    bool pressed = false;       // 이번 누름이 진행 중인가
    bool longFired = false;     // 이번 누름에서 LONG이 이미 발생했는가
    bool repeating = false;     // 이번 누름에서 REPEAT가 시작됐는가
    uint32_t fallMs = 0;        // 누른 시각
    uint32_t lastRepeatMs = 0;  // 마지막 REPEAT 시각
    bool rawHigh = true;        // 안정 디바운스용 직전 원시 레벨
    uint32_t rawChangeMs = 0;   // 원시 레벨이 마지막으로 바뀐 시각

    /** @brief 현재 레벨과 시각으로 상태를 갱신하고, 발생한 이벤트 비트를 돌려준다 */
    uint8_t update(bool levelHigh, uint32_t nowMs, const ButtonTiming& t) {
        if (t.stableMs) {
            if (levelHigh != rawHigh) { rawHigh = levelHigh; rawChangeMs = nowMs; }
            if ((uint32_t)(nowMs - rawChangeMs) <= t.stableMs) levelHigh = lastHigh;   // 아직 흔들리는 중
        }

        uint8_t ev = BUTTON_NONE;
        if (lastHigh && !levelHigh) {                       // FALLING
            fallMs = nowMs;
            pressed = true;
            longFired = false;
            repeating = false;
            ev |= BUTTON_PRESS;
        } else if (!levelHigh) {                            // STILL PRESSED
            const uint32_t held = (uint32_t)(nowMs - fallMs);
            if (pressed && !longFired && held > t.longPressMs) {
                longFired = true;
                ev |= BUTTON_LONG;
            }
            if (pressed && t.repeatDelayMs) {
                if (!repeating) {
                    if (held >= t.repeatDelayMs) { repeating = true; lastRepeatMs = nowMs; ev |= BUTTON_REPEAT; }
                } else if ((uint32_t)(nowMs - lastRepeatMs) >= t.repeatIntervalMs) {
                    lastRepeatMs = nowMs;
                    ev |= BUTTON_REPEAT;
                }
            }
        } else if (!lastHigh && levelHigh) {                // RISING
            if (pressed && !longFired && !repeating && (uint32_t)(nowMs - fallMs) > t.debounceMs) ev |= BUTTON_SHORT;
            pressed = false;
        }
        lastHigh = levelHigh;
        return ev;
    }
};

}  // namespace ds

#endif
