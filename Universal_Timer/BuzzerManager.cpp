// worynim@gmail.com
/**
 * @file BuzzerManager.cpp
 * @brief 비차단 부저 관리 구현
 * @details millis() 기반의 상태 머신으로 비프음과 알람을 관리
 */
#include "BuzzerManager.h"

BuzzerManager::BuzzerManager()
    : state(BuzzerState::IDLE)
    , state_start_ms(0)
    , alarm_count(0)
    , current_freq(0)
    , current_duration(0) {}

void BuzzerManager::begin() {
    pinMode(BUZZER_PIN, OUTPUT);
    noTone(BUZZER_PIN);
}

void BuzzerManager::update() {
    uint32_t now = millis();
    uint32_t elapsed = now - state_start_ms;

    switch (state) {
        case BuzzerState::IDLE:
            // 대기 상태 — 아무것도 하지 않음
            break;

        case BuzzerState::BEEP:
            // 비프음 종료 확인
            if (elapsed >= current_duration) {
                noTone(BUZZER_PIN);
                state = BuzzerState::IDLE;
            }
            break;

        case BuzzerState::ALARM_ON:
            // 알람 울림 구간 종료 → 멈춤 구간으로 전환
            if (elapsed >= ALARM_DURATION_MS) {
                noTone(BUZZER_PIN);
                alarm_count++;
                if (alarm_count >= ALARM_REPEAT) {
                    // 알람 완료 — 처음부터 다시 반복 (사용자가 중지할 때까지)
                    alarm_count = 0;
                }
                state = BuzzerState::ALARM_OFF;
                state_start_ms = now;
            }
            break;

        case BuzzerState::ALARM_OFF:
            // 멈춤 구간 종료 → 다시 울림
            if (elapsed >= ALARM_PAUSE_MS) {
                tone(BUZZER_PIN, ALARM_FREQ);
                state = BuzzerState::ALARM_ON;
                state_start_ms = now;
            }
            break;
    }
}

void BuzzerManager::beep() {
    // 알람 중에는 비프음 무시
    if (state == BuzzerState::ALARM_ON || state == BuzzerState::ALARM_OFF) return;

    tone(BUZZER_PIN, BEEP_FREQ);
    current_freq = BEEP_FREQ;
    current_duration = BEEP_DURATION_MS;
    state = BuzzerState::BEEP;
    state_start_ms = millis();
}

void BuzzerManager::startAlarm() {
    alarm_count = 0;
    tone(BUZZER_PIN, ALARM_FREQ);
    state = BuzzerState::ALARM_ON;
    state_start_ms = millis();
}

void BuzzerManager::stopAlarm() {
    noTone(BUZZER_PIN);
    state = BuzzerState::IDLE;
    alarm_count = 0;
}

void BuzzerManager::playStartupMelody() {
    // 블로킹 방식 — setup()에서만 호출
    const uint16_t notes[] = { STARTUP_NOTE_1, STARTUP_NOTE_2, STARTUP_NOTE_3, STARTUP_NOTE_4 };
    for (int i = 0; i < 4; i++) {
        tone(BUZZER_PIN, notes[i]);
        delay(STARTUP_NOTE_DUR);
        noTone(BUZZER_PIN);
        delay(STARTUP_NOTE_PAUSE);
    }
}

bool BuzzerManager::isAlarming() const {
    return (state == BuzzerState::ALARM_ON || state == BuzzerState::ALARM_OFF);
}

// 전역 인스턴스
BuzzerManager buzzer;

