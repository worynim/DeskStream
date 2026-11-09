// worynim@gmail.com
/**
 * @file Universal_Timer.ino
 * @brief DeskStream 4-OLED 기반 범용 고속 반응 타이머 (Universal Timer)
 * @details 변경 감지(Dirty-Check) 기반 이벤트 렌더링으로 지연 0ms 및 초고속 버튼 반응성 달성
 */
#include <Arduino.h>
#include <DeskStreamCore.h>
#include "config.h"
#include "TimerLogic.h"

// === 공통 서비스 (DeskStreamCore) ===
static ds::DisplayCore display;   // 4-OLED 화면 엔진 — 바뀐 페이지만 병렬 전송
static ds::Buttons buttons;       // 짧게/길게/자동 반복
static ds::Buzzer buzzer;         // 비차단 비프·멜로디·알람

/** 화면 idx(0~3)의 U8G2 — REVERSE_SCREEN_ORDER면 물리 순서를 뒤집는다 */
static U8G2* screenAt(int idx) {
#if REVERSE_SCREEN_ORDER
    return &display.screen(ds::kNumScreens - 1 - idx);
#else
    return &display.screen(idx);
#endif
}

/** 버튼 idx(0~3)의 이번 루프 이벤트 비트 — REVERSE_BUTTON_ORDER면 물리 순서를 뒤집는다 */
static uint8_t buttonEvents(int idx) {
#if REVERSE_BUTTON_ORDER
    return buttons.events(ds::kNumButtons - 1 - idx);
#else
    return buttons.events(idx);
#endif
}

// 부팅 멜로디 (음 STARTUP_NOTE_DUR + 쉼 STARTUP_NOTE_PAUSE) — 비차단 재생
static const ds::Note STARTUP_MELODY[] = {
    {STARTUP_NOTE_1, STARTUP_NOTE_DUR, STARTUP_NOTE_PAUSE}, {STARTUP_NOTE_2, STARTUP_NOTE_DUR, STARTUP_NOTE_PAUSE},
    {STARTUP_NOTE_3, STARTUP_NOTE_DUR, STARTUP_NOTE_PAUSE}, {STARTUP_NOTE_4, STARTUP_NOTE_DUR, STARTUP_NOTE_PAUSE},
};
// 알람 한 사이클 (울림 ALARM_DURATION_MS + 쉼 ALARM_PAUSE_MS) — stop() 할 때까지 반복
static const ds::Note ALARM_MELODY[] = {{ALARM_FREQ, ALARM_DURATION_MS, ALARM_PAUSE_MS}};

/** 버튼 클릭 비프 — 알람이 울리는 중에는 무시한다 (알람을 끊지 않게) */
static void beep() {
    if (!buzzer.isLooping()) buzzer.beep(BEEP_DURATION_MS, BEEP_FREQ);
}

// ── 렌더링 헬퍼 함수 선언 ──
void renderScreen1(U8G2* u8g2); // 시 (00H)
void renderScreen2(U8G2* u8g2); // 분 (00M)
void renderScreen3(U8G2* u8g2); // 초 (00S)
void renderScreen4(U8G2* u8g2); // 제어 (START / PAUSE / RESET)

// 이전 상태 캐시 (더티 체킹용)
static int8_t  prev_hours    = -1;
static int8_t  prev_minutes  = -1;
static int8_t  prev_seconds  = -1;
static int8_t  prev_state    = -1;
static uint32_t last_alarm_blink_ms = 0;

void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println(F("[SYSTEM] DeskStream Universal Timer Starting..."));

    // 1. 화면 엔진 초기화 (U8G2_R2 · setFlipMode 미호출 = 정방향 — 이전 구성과 같다)
    display.begin();

    // 2. 버튼 입력 초기화 (디바운스 30ms · 길게 1000ms · 300ms 후 60ms 간격 자동 반복)
    ds::ButtonTiming timing;
    timing.debounceMs = BTN_DEBOUNCE_MS;
    timing.longPressMs = BTN_LONG_PRESS_MS;
    timing.repeatDelayMs = BTN_REPEAT_INITIAL_MS;
    timing.repeatIntervalMs = BTN_REPEAT_FAST_MS;
    buttons.begin(timing);

    // 3. 부저 초기화 및 시작 멜로디 연주
    buzzer.begin();
    buzzer.playMelody(STARTUP_MELODY, sizeof(STARTUP_MELODY) / sizeof(STARTUP_MELODY[0]));

    // 초기 화면 전체 1회 강제 렌더링
    renderScreen1(screenAt(0));
    renderScreen2(screenAt(1));
    renderScreen3(screenAt(2));
    renderScreen4(screenAt(3));
    display.push();

    prev_hours   = timer.getHours();
    prev_minutes = timer.getMinutes();
    prev_seconds = timer.getSeconds();
    prev_state   = (int8_t)timer.getState();

    Serial.println(F("[SYSTEM] Universal Timer Ready."));
}

void loop() {
    // [1] 버튼 및 부저 하드웨어 상태 갱신 (초고속 폴링)
    buttons.update();

    // [2] 타이머 카운트다운 틱 처리
    bool timer_finished = timer.update();
    if (timer_finished) {
        buzzer.playMelody(ALARM_MELODY, 1, true);   // 사용자가 끌 때까지 반복
    }

    // [3] 버튼 입력 이벤트 처리 (지연 없이 즉각 반응)
    TimerState currentState = timer.getState();

    // ── BTN 1 (시 증가) ──
    const uint8_t e1 = buttonEvents(0);
    if (e1 & (ds::BUTTON_SHORT | ds::BUTTON_REPEAT)) {
        if (currentState == TimerState::FINISHED) {
            buzzer.stop();
            timer.dismissAlarm();
            beep();
        } else if (currentState == TimerState::SETTING) {
            timer.increment(0);
            beep();
        }
    }

    // ── BTN 2 (분 증가) ──
    const uint8_t e2 = buttonEvents(1);
    if (e2 & (ds::BUTTON_SHORT | ds::BUTTON_REPEAT)) {
        if (currentState == TimerState::FINISHED) {
            buzzer.stop();
            timer.dismissAlarm();
            beep();
        } else if (currentState == TimerState::SETTING) {
            timer.increment(1);
            beep();
        }
    }

    // ── BTN 3 (초 증가) ──
    const uint8_t e3 = buttonEvents(2);
    if (e3 & (ds::BUTTON_SHORT | ds::BUTTON_REPEAT)) {
        if (currentState == TimerState::FINISHED) {
            buzzer.stop();
            timer.dismissAlarm();
            beep();
        } else if (currentState == TimerState::SETTING) {
            timer.increment(2);
            beep();
        }
    }

    // ── BTN 4 (시작 / 일시정지 / 롱프레스 리셋) ──
    const uint8_t e4 = buttonEvents(3);
    if (e4 & ds::BUTTON_LONG) {
        // 롱 프레스: 0시 0분 0초 전체 초기화
        buzzer.stop();
        timer.reset();
        beep();
    } else if (e4 & ds::BUTTON_SHORT) {
        if (currentState == TimerState::FINISHED) {
            // 알람 해제
            buzzer.stop();
            timer.dismissAlarm();
            beep();
        } else if (currentState == TimerState::SETTING) {
            // 시작
            if (timer.start()) {
                beep();
            }
        } else if (currentState == TimerState::RUNNING) {
            // 일시 정지
            timer.pause();
            beep();
        }
    }

    // [4] 지능형 더티 체킹 (Dirty-Check) UI 갱신
    // 값이 변경된 화면만 선별적으로 렌더링 및 I2C 전송 -> 루프 지연 0ms 달성!
    uint8_t cur_h = timer.getHours();
    uint8_t cur_m = timer.getMinutes();
    uint8_t cur_s = timer.getSeconds();
    int8_t  cur_st = (int8_t)timer.getState();

    bool dirty = false;   // 다시 그린 화면이 있으면 루프 끝에서 한 번 전송한다 (바뀐 페이지만)

    // 시(Hour) 변경 감지
    if (cur_h != prev_hours) {
        prev_hours = cur_h;
        renderScreen1(screenAt(0));
        dirty = true;
    }

    // 분(Minute) 변경 감지
    if (cur_m != prev_minutes) {
        prev_minutes = cur_m;
        renderScreen2(screenAt(1));
        dirty = true;
    }

    // 초(Second) 변경 감지
    if (cur_s != prev_seconds) {
        prev_seconds = cur_s;
        renderScreen3(screenAt(2));
        dirty = true;
    }

    // 상태(State) 변경 감지 또는 FINISHED 알람 깜빡임
    bool need_state_update = (cur_st != prev_state);
    if (cur_st == (int8_t)TimerState::FINISHED) {
        if (millis() - last_alarm_blink_ms >= 250) {
            last_alarm_blink_ms = millis();
            need_state_update = true;
        }
    }

    if (need_state_update) {
        prev_state = cur_st;
        renderScreen4(screenAt(3));
        dirty = true;
    }

    if (dirty) display.push();
}

/**
 * @brief OLED 1: 시 (00H) 표시
 */
void renderScreen1(U8G2* u8g2) {
    if (!u8g2) return;
    u8g2->clearBuffer();

    // 상단 라벨 헤더 박스
    u8g2->drawRBox(0, 0, 128, 14, 2);
    u8g2->setFont(u8g2_font_helvB08_tr);
    u8g2->setDrawColor(0);
    const char* title = "HOUR (+1)";
    int tw = u8g2->getStrWidth(title);
    u8g2->drawStr((128 - tw) / 2, 11, title);
    u8g2->setDrawColor(1);

    // 중앙 대형 "00H" 표시
    char buf[10];
    snprintf(buf, sizeof(buf), "%02dH", timer.getHours());
    u8g2->setFont(u8g2_font_logisoso32_tr);
    int nw = u8g2->getStrWidth(buf);
    u8g2->drawStr((128 - nw) / 2, 53, buf);

    // 하단 구분선
    u8g2->drawHLine(4, 60, 120);
}

/**
 * @brief OLED 2: 분 (00M) 표시
 */
void renderScreen2(U8G2* u8g2) {
    if (!u8g2) return;
    u8g2->clearBuffer();

    // 상단 라벨 헤더 박스
    u8g2->drawRBox(0, 0, 128, 14, 2);
    u8g2->setFont(u8g2_font_helvB08_tr);
    u8g2->setDrawColor(0);
    const char* title = "MINUTE (+1)";
    int tw = u8g2->getStrWidth(title);
    u8g2->drawStr((128 - tw) / 2, 11, title);
    u8g2->setDrawColor(1);

    // 중앙 대형 "00M" 표시
    char buf[10];
    snprintf(buf, sizeof(buf), "%02dM", timer.getMinutes());
    u8g2->setFont(u8g2_font_logisoso32_tr);
    int nw = u8g2->getStrWidth(buf);
    u8g2->drawStr((128 - nw) / 2, 53, buf);

    // 하단 구분선
    u8g2->drawHLine(4, 60, 120);
}

/**
 * @brief OLED 3: 초 (00S) 표시
 */
void renderScreen3(U8G2* u8g2) {
    if (!u8g2) return;
    u8g2->clearBuffer();

    // 상단 라벨 헤더 박스
    u8g2->drawRBox(0, 0, 128, 14, 2);
    u8g2->setFont(u8g2_font_helvB08_tr);
    u8g2->setDrawColor(0);
    const char* title = "SECOND (+1)";
    int tw = u8g2->getStrWidth(title);
    u8g2->drawStr((128 - tw) / 2, 11, title);
    u8g2->setDrawColor(1);

    // 중앙 대형 "00S" 표시
    char buf[10];
    snprintf(buf, sizeof(buf), "%02dS", timer.getSeconds());
    u8g2->setFont(u8g2_font_logisoso32_tr);
    int nw = u8g2->getStrWidth(buf);
    u8g2->drawStr((128 - nw) / 2, 53, buf);

    // 하단 구분선
    u8g2->drawHLine(4, 60, 120);
}

/**
 * @brief OLED 4: 제어 및 상태 (START / PAUSE / RESET)
 */
void renderScreen4(U8G2* u8g2) {
    if (!u8g2) return;
    u8g2->clearBuffer();

    TimerState state = timer.getState();

    if (state == TimerState::FINISHED) {
        // 알람 종료 화면 (반전 깜빡임 효과)
        bool blink = (millis() / 250) % 2;
        if (blink) {
            u8g2->drawBox(0, 0, 128, 64);
            u8g2->setDrawColor(0);
        }

        u8g2->setFont(u8g2_font_helvB12_tr);
        const char* msg1 = "TIME OVER!";
        int w1 = u8g2->getStrWidth(msg1);
        u8g2->drawStr((128 - w1) / 2, 28, msg1);

        u8g2->setFont(u8g2_font_6x10_tr);
        const char* msg2 = "PRESS TO STOP";
        int w2 = u8g2->getStrWidth(msg2);
        u8g2->drawStr((128 - w2) / 2, 48, msg2);

        u8g2->setDrawColor(1);
    }
    else if (state == TimerState::RUNNING) {
        // 카운트다운 진행 중 화면
        u8g2->drawRBox(0, 0, 128, 14, 2);
        u8g2->setFont(u8g2_font_helvB08_tr);
        u8g2->setDrawColor(0);
        const char* title = "COUNTDOWN";
        int tw = u8g2->getStrWidth(title);
        u8g2->drawStr((128 - tw) / 2, 11, title);
        u8g2->setDrawColor(1);

        // 중앙 일시정지 안내
        u8g2->setFont(u8g2_font_helvB14_tr);
        const char* main_action = "PAUSE";
        int mw = u8g2->getStrWidth(main_action);
        u8g2->drawRFrame((128 - (mw + 24)) / 2, 18, mw + 24, 25, 4);
        u8g2->drawStr((128 - mw) / 2, 37, main_action);

        // 하단 리셋 안내
        u8g2->setFont(u8g2_font_5x7_tr);
        const char* hint = "HOLD: RESET";
        int hw = u8g2->getStrWidth(hint);
        u8g2->drawStr((128 - hw) / 2, 57, hint);
    }
    else {
        // 설정 대기 (START) 화면
        u8g2->drawRBox(0, 0, 128, 14, 2);
        u8g2->setFont(u8g2_font_helvB08_tr);
        u8g2->setDrawColor(0);
        const char* title = "TIMER CONTROL";
        int tw = u8g2->getStrWidth(title);
        u8g2->drawStr((128 - tw) / 2, 11, title);
        u8g2->setDrawColor(1);

        // 중앙 대형 START 버튼 박스
        u8g2->setFont(u8g2_font_helvB14_tr);
        const char* main_action = "START";
        int mw = u8g2->getStrWidth(main_action);
        u8g2->drawRBox((128 - (mw + 28)) / 2, 18, mw + 28, 25, 4);
        u8g2->setDrawColor(0);
        u8g2->drawStr((128 - mw) / 2, 37, main_action);
        u8g2->setDrawColor(1);

        // 하단 안내
        u8g2->setFont(u8g2_font_5x7_tr);
        const char* hint = "HOLD: RESET (00:00:00)";
        int hw = u8g2->getStrWidth(hint);
        u8g2->drawStr((128 - hw) / 2, 57, hint);
    }
}
