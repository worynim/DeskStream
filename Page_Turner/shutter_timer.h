// worynim@gmail.com
/**
 * @file shutter_timer.h
 * @brief 딜레이 셔터 카운트다운 (하드웨어 무관 — 호스트 테스트 대상)
 */
#pragma once
#include <stdint.h>

class ShutterTimer {
public:
    enum class Tick : uint8_t { None, Second, Fire };

    void start(uint32_t nowMs, uint8_t seconds);
    void cancel() { _running = false; }
    bool running() const { return _running; }
    uint8_t secondsLeft() const { return _shown; }   // 올림 (2.3초 남음 → 3)

    // 표시 초가 줄면 Second, 시간이 다 되면 Fire를 한 번 내고 멈춘다
    Tick tick(uint32_t nowMs);

private:
    bool _running = false;
    uint32_t _endMs = 0;
    uint8_t _shown = 0;
};
