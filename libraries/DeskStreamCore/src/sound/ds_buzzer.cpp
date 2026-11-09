// worynim@gmail.com
/**
 * @file ds_buzzer.cpp
 * @brief 비차단 부저 구현
 * @details 타이머 콜백은 FreeRTOS 타이머 태스크에서 돈다 (ISR 아님 → tone()/noTone() 호출 가능).
 */
#include "ds_buzzer.h"

namespace ds {

void Buzzer::begin(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
    if (!_timer) _timer = xTimerCreate("Buzzer", pdMS_TO_TICKS(50), pdFALSE, this, onTimer);
}

void Buzzer::beep(uint16_t durationMs, uint16_t freq) {
    if (!_timer) return;
    _notes = nullptr;   // 진행 중인 멜로디를 끊는다
    _loop = false;
    _playing = true;
    tone(_pin, freq);
    xTimerChangePeriod(_timer, pdMS_TO_TICKS(durationMs ? durationMs : 1), 0);   // 주기 변경 = 시작
}

void Buzzer::playMelody(const Note* notes, uint8_t count, bool loop) {
    if (!_timer || !notes || count == 0) return;
    _notes = notes;
    _count = count;
    _idx = 0;
    _loop = loop;
    _playing = true;
    startNote();
}

void Buzzer::startNote() {
    const Note& n = _notes[_idx];
    _inGap = false;
    if (n.freq) tone(_pin, n.freq);
    else noTone(_pin);
    xTimerChangePeriod(_timer, pdMS_TO_TICKS(n.durMs ? n.durMs : 1), 0);
}

void Buzzer::stop() {
    if (_timer) xTimerStop(_timer, 0);
    noTone(_pin);
    digitalWrite(_pin, LOW);
    _notes = nullptr;
    _loop = false;
    _playing = false;
}

void Buzzer::onTimer(TimerHandle_t t) {
    Buzzer* self = (Buzzer*)pvTimerGetTimerID(t);
    if (!self->_notes) { self->stop(); return; }   // 단발 비프 종료

    const Note& n = self->_notes[self->_idx];
    if (!self->_inGap && n.gapMs) {                  // 음 → 쉼
        noTone(self->_pin);
        self->_inGap = true;
        xTimerChangePeriod(t, pdMS_TO_TICKS(n.gapMs), 0);
        return;
    }
    if (++self->_idx < self->_count) self->startNote();          // 다음 음
    else if (self->_loop) { self->_idx = 0; self->startNote(); }  // 처음부터 반복 (알람)
    else self->stop();
}

}  // namespace ds
