// worynim@gmail.com
/**
 * @file BellBuzzer.cpp
 * @brief duty 감쇠 부저 구현
 * @details 타이머 콜백은 FreeRTOS 타이머 태스크에서 돈다 (ISR 아님 → LEDC 함수 호출 가능).
 */
#include "BellBuzzer.h"
#include "config.h"

static constexpr uint8_t kDecaySteps = sizeof(BUZZER_DECAY_DUTY) / sizeof(BUZZER_DECAY_DUTY[0]);

void BellBuzzer::begin(uint8_t pin) {
    _pin = pin;
    ledcAttach(_pin, 1000, BUZZER_PWM_RES_BITS);
    ledcWrite(_pin, 0);
    if (!_timer) _timer = xTimerCreate("BellBuzzer", pdMS_TO_TICKS(50), pdFALSE, this, onTimer);
}

void BellBuzzer::arm(uint16_t ms) {
    xTimerChangePeriod(_timer, pdMS_TO_TICKS(ms ? ms : 1), 0);   // 주기 변경 = 시작
}

void BellBuzzer::beep(uint16_t durationMs, uint16_t freq) {
    if (!_timer) return;
    _notes = nullptr;   // 진행 중인 멜로디를 끊는다
    _loop = false;
    _playing = true;
    ledcChangeFrequency(_pin, freq, BUZZER_PWM_RES_BITS);
    ledcWrite(_pin, BUZZER_DECAY_DUTY[0]);
    arm(durationMs);
}

void BellBuzzer::playMelody(const ds::Note* notes, uint8_t count, bool loop) {
    if (!_timer || !notes || count == 0) return;
    _notes = notes;
    _count = count;
    _idx = 0;
    _loop = loop;
    _playing = true;
    startNote();
}

void BellBuzzer::startNote() {
    const ds::Note& n = _notes[_idx];
    _inGap = false;
    _step = 0;
    _elapsed = 0;
    if (n.freq) {
        ledcChangeFrequency(_pin, n.freq, BUZZER_PWM_RES_BITS);
        ledcWrite(_pin, BUZZER_DECAY_DUTY[0]);
        armSegment(n);
    } else {
        ledcWrite(_pin, 0);   // 쉼표
        arm(n.durMs);
    }
}

void BellBuzzer::armSegment(const ds::Note& n) {
    const uint16_t seg = min<uint16_t>(BUZZER_DECAY_STEP_MS, n.durMs - _elapsed);
    _elapsed += seg;
    arm(seg);
}

void BellBuzzer::stop() {
    if (_timer) xTimerStop(_timer, 0);
    ledcWrite(_pin, 0);
    _notes = nullptr;
    _loop = false;
    _playing = false;
}

void BellBuzzer::onTimer(TimerHandle_t t) {
    BellBuzzer* self = (BellBuzzer*)pvTimerGetTimerID(t);
    if (!self->_notes) { self->stop(); return; }   // 단발 비프 종료

    const ds::Note& n = self->_notes[self->_idx];
    if (!self->_inGap && n.freq && self->_elapsed < n.durMs) {   // 음 계속 → 다음 감쇠 단계
        if (self->_step < kDecaySteps - 1) ledcWrite(self->_pin, BUZZER_DECAY_DUTY[++self->_step]);
        self->armSegment(n);
        return;
    }
    if (!self->_inGap && n.gapMs) {                  // 음 → 쉼
        ledcWrite(self->_pin, 0);
        self->_inGap = true;
        self->arm(n.gapMs);
        return;
    }
    if (++self->_idx < self->_count) self->startNote();          // 다음 음
    else if (self->_loop) { self->_idx = 0; self->startNote(); }  // 처음부터 반복 (알람)
    else self->stop();
}
