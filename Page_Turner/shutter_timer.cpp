// worynim@gmail.com
/**
 * @file shutter_timer.cpp
 * @brief 딜레이 셔터 카운트다운
 */
#include "shutter_timer.h"

void ShutterTimer::start(uint32_t nowMs, uint8_t seconds) {
    _endMs = nowMs + seconds * 1000u;
    _shown = seconds;
    _running = true;
}

ShutterTimer::Tick ShutterTimer::tick(uint32_t nowMs) {
    if (!_running) return Tick::None;
    int32_t left = (int32_t)(_endMs - nowMs);   // 랩을 걸쳐도 맞는 남은 시간
    if (left <= 0) {
        _running = false;
        _shown = 0;
        return Tick::Fire;
    }
    uint8_t sec = (uint8_t)((left + 999) / 1000);
    if (sec != _shown) {
        _shown = sec;
        return Tick::Second;
    }
    return Tick::None;
}
