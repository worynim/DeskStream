// worynim@gmail.com
/**
 * @file ds_buzzer.h
 * @brief 비차단 부저 (비프 · 멜로디)
 * @details tone()으로 소리를 내고, 끄는 시점과 다음 음은 FreeRTOS 원샷 타이머가 처리한다.
 *          그래서 beep()/playMelody()는 즉시 반환하고 메인 루프(버튼·애니메이션)가 멈추지 않는다.
 * @note 출처: Hangeul_Clock/display_manager.cpp 의 beep()·playStartupMelody()·playChimeMelody()
 *       (v2.10.0). 멜로디는 원래 tone()+delay()로 메인 루프를 막았다.
 */
#ifndef DS_BUZZER_H
#define DS_BUZZER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include "../ds_board.h"

namespace ds {

/** 멜로디의 한 음 — freq 0이면 쉼표 */
struct Note {
    uint16_t freq;    ///< Hz
    uint16_t durMs;   ///< 소리 나는 시간
    uint16_t gapMs;   ///< 다음 음까지 무음 시간
};

class Buzzer {
public:
    void begin(uint8_t pin = kBuzzerPin);

    /** 진행 중인 소리를 끊고 duration ms 동안 울린다 */
    void beep(uint16_t durationMs = 50, uint16_t freq = 3000);

    /**
     * @brief 멜로디를 백그라운드로 재생한다 (진행 중인 소리는 끊는다)
     * @param notes 재생이 끝날 때까지 유효해야 한다 — static const 배열을 넘긴다
     * @param loop  true면 stop()을 부를 때까지 처음부터 반복한다 (알람)
     */
    void playMelody(const Note* notes, uint8_t count, bool loop = false);

    /** 진행 중인 비프·멜로디를 즉시 끊는다 */
    void stop();

    bool isPlaying() const { return _playing; }
    /** 반복 멜로디(알람)가 울리는 중인가 */
    bool isLooping() const { return _playing && _loop; }

private:
    static void onTimer(TimerHandle_t t);
    void startNote();

    uint8_t _pin = kBuzzerPin;
    TimerHandle_t _timer = nullptr;
    const Note* _notes = nullptr;
    uint8_t _count = 0;
    uint8_t _idx = 0;
    bool _inGap = false;
    bool _loop = false;
    volatile bool _playing = false;
};

}  // namespace ds

#endif
