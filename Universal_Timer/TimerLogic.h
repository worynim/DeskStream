// worynim@gmail.com
/**
 * @file TimerLogic.h
 * @brief 범용 타이머 핵심 로직 (설정 모드 + 카운트다운)
 * @details 시/분/초 설정, 카운트다운 실행, 완료 알림을 관리
 */
#ifndef TIMER_LOGIC_H
#define TIMER_LOGIC_H

#include <Arduino.h>
#include "config.h"

/**
 * @brief 타이머 동작 상태
 */
enum class TimerState : uint8_t {
    SETTING = 0,    // 설정 모드 — 버튼으로 시/분/초 조절
    RUNNING,        // 카운트다운 진행 중
    FINISHED        // 카운트다운 완료 — 알람 울림
};

/**
 * @brief 범용 타이머 로직 클래스
 */
class TimerLogic {
public:
    TimerLogic();

    /**
     * @brief 시간 값 증가 (+1)
     * @param unit 0=시, 1=분, 2=초
     */
    void increment(int unit);

    /**
     * @brief 카운트다운 시작
     * @return true면 시작 성공, false면 시간이 0이라 시작 불가
     */
    bool start();

    /**
     * @brief 카운트다운 일시정지 (RUNNING → SETTING)
     */
    void pause();

    /**
     * @brief 시간 초기화 (00:00:00)
     */
    void reset();

    /**
     * @brief 알람 상태 해제 (FINISHED → SETTING)
     */
    void dismissAlarm();

    /**
     * @brief 매 루프에서 호출 — 카운트다운 틱 처리
     * @return true면 카운트다운이 0에 도달 (알람 트리거)
     */
    bool update();

    // ── Getters ──
    uint8_t getHours() const   { return hours; }
    uint8_t getMinutes() const { return minutes; }
    uint8_t getSeconds() const { return seconds; }
    TimerState getState() const { return state; }

    /**
     * @brief 전체 남은 시간(초 단위) 반환
     * @return 남은 초
     */
    uint32_t getTotalSeconds() const;

private:
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;

    // 직전 설정 시간 기억용 (타임아웃 복원용)
    uint8_t init_hours;
    uint8_t init_minutes;
    uint8_t init_seconds;

    TimerState state;
    uint32_t last_tick_ms;          // 마지막 카운트다운 틱 시각
};

extern TimerLogic timer;

#endif // TIMER_LOGIC_H

