// worynim@gmail.com
/**
 * @file BellBuzzer.h
 * @brief PWM duty로 음량을 줄여 종소리 여운을 내는 비차단 부저
 * @details ds::Buzzer는 tone()을 써서 duty가 50%로 고정된다. 이 클래스는 LEDC를 직접 써서
 *          멜로디의 한 음 안에서 BUZZER_DECAY_STEP_MS마다 duty를 BUZZER_DECAY_DUTY 순서로 낮춘다.
 *          음 형식(ds::Note)과 API는 ds::Buzzer와 같고, 끄는 시점·다음 음은 FreeRTOS 원샷 타이머가 처리한다.
 */
#ifndef BELL_BUZZER_H
#define BELL_BUZZER_H

#include <Arduino.h>
#include <DeskStreamCore.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

class BellBuzzer {
public:
    void begin(uint8_t pin = ds::kBuzzerPin);

    /** 진행 중인 소리를 끊고 duty 50%로 durationMs 동안 울린다 (감쇠 없음) */
    void beep(uint16_t durationMs, uint16_t freq);

    /**
     * @brief 멜로디를 감쇠를 넣어 백그라운드로 재생한다 (진행 중인 소리는 끊는다)
     * @param notes 재생이 끝날 때까지 유효해야 한다 — static const 배열을 넘긴다
     * @param loop  true면 stop()을 부를 때까지 처음부터 반복한다 (알람)
     */
    void playMelody(const ds::Note* notes, uint8_t count, bool loop = false);

    /** 진행 중인 비프·멜로디를 즉시 끊는다 */
    void stop();

    bool isPlaying() const { return _playing; }
    /** 반복 멜로디(알람)가 울리는 중인가 */
    bool isLooping() const { return _playing && _loop; }

private:
    static void onTimer(TimerHandle_t t);
    void startNote();
    void armSegment(const ds::Note& n);   // 다음 감쇠 단계까지(또는 음 끝까지) 타이머를 건다
    void arm(uint16_t ms);

    uint8_t _pin = ds::kBuzzerPin;
    TimerHandle_t _timer = nullptr;
    const ds::Note* _notes = nullptr;
    uint8_t _count = 0;
    uint8_t _idx = 0;
    uint8_t _step = 0;        // 현재 음의 감쇠 단계 (BUZZER_DECAY_DUTY 인덱스)
    uint16_t _elapsed = 0;    // 현재 음에서 타이머로 건 시간 합 (ms)
    bool _inGap = false;
    bool _loop = false;
    volatile bool _playing = false;
};

#endif
