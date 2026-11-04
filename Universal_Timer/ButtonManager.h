// worynim@gmail.com
/**
 * @file ButtonManager.h
 * @brief 4버튼 비차단 입력 매니저 (디바운스 + 롱프레스 + 자동 반복)
 * @details 숏 프레스, 롱 프레스, 누르고 있을 때 빠르게 반복 입력 지원
 */
#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>
#include "config.h"

/**
 * @brief 버튼 이벤트 종류
 */
enum class ButtonEvent : uint8_t {
    NONE = 0,       // 이벤트 없음
    SHORT_PRESS,    // 짧게 누름 (눌렀다 뗌)
    LONG_PRESS,     // 길게 누름 (1초 이상 유지 후 발생)
    REPEAT          // 반복 입력 (계속 누르고 있을 때 빠르게 반복)
};

/**
 * @brief 개별 버튼 상태 관리 구조체
 */
struct ButtonState {
    uint8_t pin;                    // GPIO 핀 번호
    bool pressed;                   // 현재 눌림 상태
    bool long_fired;                // 롱 프레스 이벤트가 이미 발생했는지
    uint32_t press_start_ms;        // 누르기 시작한 시각 (ms)
    uint32_t last_repeat_ms;        // 마지막 반복 입력 시각 (ms)
    bool repeat_started;            // 반복 입력 모드 진입 여부
};

/**
 * @brief 4개 버튼 입력 관리 클래스
 */
class ButtonManager {
public:
    ButtonManager();

    /**
     * @brief 버튼 GPIO 초기화 (INPUT_PULLUP)
     */
    void begin();

    /**
     * @brief 매 루프에서 호출 — 버튼 상태 업데이트
     */
    void update();

    /**
     * @brief 특정 버튼의 이벤트 확인
     * @param idx 버튼 인덱스 (0~3)
     * @return ButtonEvent 이벤트 종류
     */
    ButtonEvent getEvent(int idx) const;

    /**
     * @brief 특정 버튼이 현재 눌려 있는지 확인
     * @param idx 버튼 인덱스 (0~3)
     * @return true면 눌려 있음
     */
    bool isPressed(int idx) const;

private:
    static constexpr uint8_t PINS[4] = { BTN1_PIN, BTN2_PIN, BTN3_PIN, BTN4_PIN };
    ButtonState buttons[4];
    ButtonEvent events[4];         // 현재 프레임의 이벤트 (update마다 초기화)
};

extern ButtonManager btnManager;

#endif // BUTTON_MANAGER_H

