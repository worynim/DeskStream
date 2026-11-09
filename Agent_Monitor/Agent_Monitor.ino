// worynim@gmail.com
/**
 * @file Agent_Monitor.ino
 * @brief AI 개발 에이전트(Claude Code) 상태 모니터 — DeskStream 4-OLED
 * @details PC 데몬(bridge_py/agent_bridge.py)이 USB 시리얼로 보내는 세션 상태(JSON Lines)를
 *          OLED 4장(세션 슬롯 4개)에 표시하고, 상태 변화 알림을 부저로 낸다.
 *          - 개요 화면: 버튼 i 짧게 = 슬롯 i 의 완료 표시 확인, 길게 = 데몬에 전달(PC 명령은 P3).
 *          - 승인 화면(권한 요청 대기 중): BTN1 승인 · BTN2 다음 요청 · BTN3 거절 · BTN4 나중에(터미널에 맡김).
 *            새 요청이 뜬 직후 AM_PERM_GUARD_MS 동안은 버튼을 무시한다.
 *          프로토콜: bridge_py/protocol.py 참조.
 */
#include <Arduino.h>
#include <DeskStreamCore.h>

#include "agent_state.h"
#include "agent_ui.h"
#include "config.h"

static ds::DisplayCore display;
static ds::Buttons buttons;
static ds::Buzzer buzzer;
static am::Model model;

// 알림음 (재생이 끝날 때까지 살아 있어야 하므로 static const)
static const ds::Note MELODY_DONE[] = {{2093, 80, 30}, {3136, 120, 0}};
static const ds::Note MELODY_ATTN[] = {{2637, 70, 50}, {2637, 70, 50}, {2637, 70, 0}};
static const ds::Note MELODY_ERROR[] = {{880, 200, 60}, {659, 300, 0}};
static const ds::Note MELODY_START[] = {{2093, 60, 20}, {2637, 60, 20}, {3136, 60, 0}};

static void playAlert(am::State kind) {
    switch (kind) {
        case am::State::Done:    buzzer.playMelody(MELODY_DONE, 2); break;
        case am::State::Approve:
        case am::State::Input:   buzzer.playMelody(MELODY_ATTN, 3); break;
        case am::State::Error:   buzzer.playMelody(MELODY_ERROR, 2); break;
        default: break;
    }
}

/** 버튼 이벤트를 데몬에 보낸다 */
static void sendButton(uint8_t b, const char* press) {
    Serial.printf("{\"t\":\"btn\",\"b\":%u,\"press\":\"%s\"}\n", b, press);
}

/** 승인 화면 결정을 데몬에 보낸다 (act = allow | next | deny | pass) */
static void sendDecision(uint32_t id, const char* act) {
    Serial.printf("{\"t\":\"decide\",\"id\":%lu,\"act\":\"%s\"}\n", (unsigned long)id, act);
}

/** 승인 화면이 떠 있는지 (데몬 연결 중 + 대기 요청 있음) */
static bool approvalActive(uint32_t now) {
    return model.perm.id != 0 && am::linkAlive(model, now, AM_LINK_TIMEOUT_MS);
}

/** 시리얼에서 줄을 모아 하나씩 모델에 반영한다 */
static void pollSerial() {
    static char line[AM_LINE_MAX];
    static size_t len = 0;
    static bool overflow = false;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r') continue;
        if (c != '\n') {
            if (len < sizeof(line) - 1) line[len++] = c;
            else overflow = true;
            continue;
        }
        line[len] = '\0';
        if (!overflow && len) {
            am::ParseResult r = am::applyLine(model, line, millis());
            if (r.type == am::MsgType::Alert) playAlert(r.alert);
        }
        len = 0;
        overflow = false;
    }
}

/** 승인 화면의 버튼 i 짧게 누름 */
static void onApprovalButton(uint8_t i, uint32_t now) {
    static const char* const acts[4] = {"allow", "next", "deny", "pass"};
    const am::Perm& p = model.perm;
    if (now - p.shownMs < AM_PERM_GUARD_MS) return;   // 방금 뜬 요청 — 다른 걸 누르려던 입력일 수 있다
    if (i == 1 && p.q < 2) return;                     // 다음 요청 없음
    buzzer.beep(AM_BEEP_MS, AM_BEEP_HZ);
    sendDecision(p.id, acts[i]);
}

static void pollButtons() {
    buttons.update();
    uint32_t now = millis();
    for (uint8_t i = 0; i < ds::kNumButtons; i++) {
        uint8_t ev = buttons.events(i);
        if (approvalActive(now)) {
            if (ev & ds::BUTTON_SHORT) onApprovalButton(i, now);
            continue;
        }
        if (ev & ds::BUTTON_SHORT) {
            buzzer.beep(AM_BEEP_MS, AM_BEEP_HZ);
            sendButton(i, "short");
        }
        if (ev & ds::BUTTON_LONG) {
            buzzer.beep(AM_BEEP_MS * 3, AM_BEEP_HZ / 2);
            sendButton(i, "long");
        }
    }
}

static void render(uint32_t now) {
    bool alive = am::linkAlive(model, now, AM_LINK_TIMEOUT_MS);
    bool approval = approvalActive(now);
    for (uint8_t i = 0; i < ds::kNumScreens; i++) {
        if (approval) am::drawApproval(display.screen(i), i, model.perm, now);
        else if (alive) am::drawSlot(display.screen(i), model.slots[i], i, now);
        else am::drawOffline(display.screen(i), i, model.everRx, now);
    }
    display.push();   // 바뀐 타일만 전송
}

void setup() {
    Serial.begin(AM_SERIAL_BAUD);
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(0);   // PC 가 포트를 열지 않아도 loop() 가 멈추지 않게
#endif
    display.begin();
    buttons.begin();
    buzzer.begin();
    buzzer.playMelody(MELODY_START, 3);
    Serial.println("{\"t\":\"hello\",\"fw\":\"" AM_FW_VERSION "\"}");
}

void loop() {
    pollSerial();
    pollButtons();
    static uint32_t lastFrame = 0;
    uint32_t now = millis();
    if (now - lastFrame >= AM_FRAME_MS) {
        lastFrame = now;
        render(now);
    }
}
