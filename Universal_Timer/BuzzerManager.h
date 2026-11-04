// worynim@gmail.com
/**
 * @file BuzzerManager.h
 * @brief 비차단(Non-blocking) 부저 관리 클래스
 * @details millis() 기반 구현으로 부저음 재생 중에도 메인 루프가 멈추지 않음
 */
#ifndef BUZZER_MANAGER_H
#define BUZZER_MANAGER_H

#include <Arduino.h>
#include "config.h"

/**
 * @brief 부저 상태 열거형
 */
enum class BuzzerState : uint8_t {
    IDLE = 0,       // 대기 중
    BEEP,           // 단발 비프음 재생 중
    ALARM_ON,       // 알람음 재생 중 (울리는 구간)
    ALARM_OFF       // 알람음 쉬는 구간
};

/**
 * @brief 비차단 부저 관리 클래스
 */
class BuzzerManager {
public:
    BuzzerManager();

    /**
     * @brief 부저 핀 초기화
     */
    void begin();

    /**
     * @brief 매 루프에서 호출 — 부저 상태 업데이트
     */
    void update();

    /**
     * @brief 짧은 비프음 발생 (버튼 클릭 피드백)
     */
    void beep();

    /**
     * @brief 알람 시작 (타이머 종료 시)
     */
    void startAlarm();

    /**
     * @brief 알람 중지
     */
    void stopAlarm();

    /**
     * @brief 부팅 멜로디 재생 (블로킹 — setup()에서만 사용)
     */
    void playStartupMelody();

    /**
     * @brief 알람이 현재 울리고 있는지 확인
     * @return true면 알람 중
     */
    bool isAlarming() const;

private:
    BuzzerState state;
    uint32_t state_start_ms;        // 현재 상태 시작 시각
    uint8_t alarm_count;            // 알람 반복 횟수 카운터
    uint16_t current_freq;          // 현재 재생 중인 주파수
    uint16_t current_duration;      // 현재 재생 지속 시간
};

extern BuzzerManager buzzer;

#endif // BUZZER_MANAGER_H

