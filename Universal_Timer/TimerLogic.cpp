// worynim@gmail.com
/**
 * @file TimerLogic.cpp
 * @brief 범용 타이머 로직 구현체
 * @details 시/분/초 증가, 카운트다운 1초 틱 처리, 초기화 및 상태 관리
 */
#include "TimerLogic.h"

TimerLogic::TimerLogic()
    : hours(0)
    , minutes(0)
    , seconds(0)
    , init_hours(0)
    , init_minutes(0)
    , init_seconds(0)
    , state(TimerState::SETTING)
    , last_tick_ms(0) {}

void TimerLogic::increment(int unit) {
    // RUNNING 또는 FINISHED 상태일 때는 증감 방지
    if (state != TimerState::SETTING) return;

    if (unit == 0) { // 시 (Hours: 0 ~ MAX_HOURS)
        hours++;
        if (hours > MAX_HOURS) {
            hours = 0;
        }
    } else if (unit == 1) { // 분 (Minutes: 0 ~ MAX_MINUTES)
        minutes++;
        if (minutes > MAX_MINUTES) {
            minutes = 0;
        }
    } else if (unit == 2) { // 초 (Seconds: 0 ~ MAX_SECONDS)
        seconds++;
        if (seconds > MAX_SECONDS) {
            seconds = 0;
        }
    }
}

bool TimerLogic::start() {
    if (getTotalSeconds() == 0) {
        return false; // 설정된 시간이 없으면 시작하지 않음
    }

    // [중요] 카운트다운 시작 시 직전 설정 시간을 보관
    init_hours = hours;
    init_minutes = minutes;
    init_seconds = seconds;

    state = TimerState::RUNNING;
    last_tick_ms = millis();
    return true;
}

void TimerLogic::pause() {
    if (state == TimerState::RUNNING) {
        state = TimerState::SETTING;
    }
}

void TimerLogic::reset() {
    hours = 0;
    minutes = 0;
    seconds = 0;
    init_hours = 0;
    init_minutes = 0;
    init_seconds = 0;
    state = TimerState::SETTING;
}

void TimerLogic::dismissAlarm() {
    // [중요] 알람 해제 시 직전에 설정했던 시간으로 자동 복원
    hours = init_hours;
    minutes = init_minutes;
    seconds = init_seconds;
    state = TimerState::SETTING;
}

uint32_t TimerLogic::getTotalSeconds() const {
    return ((uint32_t)hours * 3600UL) + ((uint32_t)minutes * 60UL) + (uint32_t)seconds;
}

bool TimerLogic::update() {
    if (state != TimerState::RUNNING) {
        return false;
    }

    uint32_t now = millis();
    if (now - last_tick_ms >= COUNTDOWN_TICK_MS) {
        last_tick_ms += COUNTDOWN_TICK_MS;

        if (seconds > 0) {
            seconds--;
        } else {
            if (minutes > 0) {
                minutes--;
                seconds = 59;
            } else {
                if (hours > 0) {
                    hours--;
                    minutes = 59;
                    seconds = 59;
                } else {
                    // 00:00:00 도달 시 알람 발생
                    state = TimerState::FINISHED;
                    return true;
                }
            }
        }
    }
    return false;
}

// 전역 인스턴스
TimerLogic timer;

