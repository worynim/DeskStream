// worynim@gmail.com
/**
 * @file Universal_Timer.ino
 * @brief DeskStream 4-OLED 기반 범용 고속 반응 타이머 (Universal Timer)
 * @details 변경 감지(Dirty-Check) 기반 이벤트 렌더링으로 지연 0ms 및 초고속 버튼 반응성 달성
 */
#include <Arduino.h>
#include "config.h"
#include "DisplayEngine.h"
#include "ButtonManager.h"
#include "BuzzerManager.h"
#include "TimerLogic.h"

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

    // 1. 하드웨어 디스플레이 엔진 초기화
    display.begin();

    // 2. 버튼 입력 관리자 초기화
    btnManager.begin();

    // 3. 부저 초기화 및 시작 멜로디 연주
    buzzer.begin();
    buzzer.playStartupMelody();

    // 초기 화면 전체 1회 강제 렌더링
    renderScreen1(display.getScreen(0)); display.flushScreen(0);
    renderScreen2(display.getScreen(1)); display.flushScreen(1);
    renderScreen3(display.getScreen(2)); display.flushScreen(2);
    renderScreen4(display.getScreen(3)); display.flushScreen(3);

    prev_hours   = timer.getHours();
    prev_minutes = timer.getMinutes();
    prev_seconds = timer.getSeconds();
    prev_state   = (int8_t)timer.getState();

    Serial.println(F("[SYSTEM] Universal Timer Ready."));
}

void loop() {
    // [1] 버튼 및 부저 하드웨어 상태 갱신 (초고속 폴링)
    btnManager.update();
    buzzer.update();

    // [2] 타이머 카운트다운 틱 처리
    bool timer_finished = timer.update();
    if (timer_finished) {
        buzzer.startAlarm();
    }

    // [3] 버튼 입력 이벤트 처리 (지연 없이 즉각 반응)
    TimerState currentState = timer.getState();

    // ── BTN 1 (시 증가) ──
    ButtonEvent e1 = btnManager.getEvent(0);
    if (e1 == ButtonEvent::SHORT_PRESS || e1 == ButtonEvent::REPEAT) {
        if (currentState == TimerState::FINISHED) {
            buzzer.stopAlarm();
            timer.dismissAlarm();
            buzzer.beep();
        } else if (currentState == TimerState::SETTING) {
            timer.increment(0);
            buzzer.beep();
        }
    }

    // ── BTN 2 (분 증가) ──
    ButtonEvent e2 = btnManager.getEvent(1);
    if (e2 == ButtonEvent::SHORT_PRESS || e2 == ButtonEvent::REPEAT) {
        if (currentState == TimerState::FINISHED) {
            buzzer.stopAlarm();
            timer.dismissAlarm();
            buzzer.beep();
        } else if (currentState == TimerState::SETTING) {
            timer.increment(1);
            buzzer.beep();
        }
    }

    // ── BTN 3 (초 증가) ──
    ButtonEvent e3 = btnManager.getEvent(2);
    if (e3 == ButtonEvent::SHORT_PRESS || e3 == ButtonEvent::REPEAT) {
        if (currentState == TimerState::FINISHED) {
            buzzer.stopAlarm();
            timer.dismissAlarm();
            buzzer.beep();
        } else if (currentState == TimerState::SETTING) {
            timer.increment(2);
            buzzer.beep();
        }
    }

    // ── BTN 4 (시작 / 일시정지 / 롱프레스 리셋) ──
    ButtonEvent e4 = btnManager.getEvent(3);
    if (e4 == ButtonEvent::LONG_PRESS) {
        // 롱 프레스: 0시 0분 0초 전체 초기화
        buzzer.stopAlarm();
        timer.reset();
        buzzer.beep();
    } else if (e4 == ButtonEvent::SHORT_PRESS) {
        if (currentState == TimerState::FINISHED) {
            // 알람 해제
            buzzer.stopAlarm();
            timer.dismissAlarm();
            buzzer.beep();
        } else if (currentState == TimerState::SETTING) {
            // 시작
            if (timer.start()) {
                buzzer.beep();
            }
        } else if (currentState == TimerState::RUNNING) {
            // 일시 정지
            timer.pause();
            buzzer.beep();
        }
    }

    // [4] 지능형 더티 체킹 (Dirty-Check) UI 갱신
    // 값이 변경된 화면만 선별적으로 렌더링 및 I2C 전송 -> 루프 지연 0ms 달성!
    uint8_t cur_h = timer.getHours();
    uint8_t cur_m = timer.getMinutes();
    uint8_t cur_s = timer.getSeconds();
    int8_t  cur_st = (int8_t)timer.getState();

    // 시(Hour) 변경 감지
    if (cur_h != prev_hours) {
        prev_hours = cur_h;
        renderScreen1(display.getScreen(0));
        display.flushScreen(0);
    }

    // 분(Minute) 변경 감지
    if (cur_m != prev_minutes) {
        prev_minutes = cur_m;
        renderScreen2(display.getScreen(1));
        display.flushScreen(1);
    }

    // 초(Second) 변경 감지
    if (cur_s != prev_seconds) {
        prev_seconds = cur_s;
        renderScreen3(display.getScreen(2));
        display.flushScreen(2);
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
        renderScreen4(display.getScreen(3));
        display.flushScreen(3);
    }
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
