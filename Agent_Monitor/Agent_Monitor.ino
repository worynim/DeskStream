// worynim@gmail.com
/**
 * @file Agent_Monitor.ino
 * @brief AI 개발 에이전트(Claude Code) 상태 모니터 — DeskStream 4-OLED
 * @details PC 데몬(bridge_py/agent_bridge.py)이 USB 시리얼로 보내는 세션 상태(JSON Lines)를
 *          OLED 4장(세션 슬롯 4개)에 표시하고, 상태 변화 알림을 부저로 낸다.
 *          - 개요 화면: 버튼 i 짧게 = 슬롯 i 상세 화면(+완료 확인), 길게 = PC 명령 i+1 (데몬 설정 파일).
 *          - 상세 화면: BTN1 개요로 · BTN2 음소거 켜기/끄기(NVS 저장) · BTN3 이전 세션 · BTN4 다음 세션.
 *            AM_DETAIL_IDLE_MS 동안 입력이 없거나 세션이 끝나면 개요로 돌아간다.
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

// 장치 설정 (NVS) — 음소거
struct Settings {
    bool muted = false;
};
static Settings settings;
static ds::ConfigStore store;
static const ds::ConfigField kFields[] = {
    DS_CFG_BOOL(Settings, muted, "muted", "muted", nullptr),
};

// 화면 모드 — 승인 화면이 오면 그것이 우선한다
static int8_t detailSlot = -1;       // 상세 화면으로 보는 슬롯 (-1 = 개요)
static uint32_t detailTouchMs = 0;   // 상세 화면에서 마지막으로 버튼을 누른 시각

// 알림음 (재생이 끝날 때까지 살아 있어야 하므로 static const)
static const ds::Note MELODY_DONE[] = {{2093, 80, 30}, {3136, 120, 0}};
static const ds::Note MELODY_ATTN[] = {{2637, 70, 50}, {2637, 70, 50}, {2637, 70, 0}};
static const ds::Note MELODY_ERROR[] = {{880, 200, 60}, {659, 300, 0}};
static const ds::Note MELODY_START[] = {{2093, 60, 20}, {2637, 60, 20}, {3136, 60, 0}};

static void playAlert(am::State kind) {
    if (settings.muted) return;   // 무음 시간대는 데몬이 알림을 보내지 않는다
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

/** 상세 화면으로 slot 을 연다 — 데몬에 알려 그 세션의 상세 정보를 받는다 (완료 확인도 함께) */
static void openDetail(int8_t slot, uint32_t now) {
    detailSlot = slot;
    detailTouchMs = now;
    sendButton(slot, "short");
}

/** from 에서 dir(+1/-1) 방향으로 세션이 있는 다음 슬롯 (없으면 from) */
static int8_t nextOnSlot(int8_t from, int dir) {
    for (int k = 1; k <= am::kSlots; k++) {
        int8_t s = (int8_t)((from + dir * k + am::kSlots * 2) % am::kSlots);
        if (model.slots[s].on) return s;
    }
    return from;
}

/** 상세 화면의 버튼 i 짧게 누름 */
static void onDetailButton(uint8_t i, uint32_t now) {
    detailTouchMs = now;
    switch (i) {
        case 0: detailSlot = -1; break;
        case 1: settings.muted = !settings.muted; store.setDirty(); break;
        case 2: openDetail(nextOnSlot(detailSlot, -1), now); break;
        default: openDetail(nextOnSlot(detailSlot, +1), now); break;
    }
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
            if (detailSlot >= 0) onDetailButton(i, now);
            else if (model.slots[i].on) openDetail(i, now);
        }
        if (ev & ds::BUTTON_LONG) {
            buzzer.beep(AM_BEEP_MS * 3, AM_BEEP_HZ / 2);
            sendButton(i, "long");
        }
    }
}

/** 상세 화면에 쓸 정보 — 데몬의 상세 정보가 아직 없으면 슬롯 정보로 머리줄만 채운다 */
static const am::Detail& detailFor(int8_t slot) {
    static am::Detail fallback;
    if (model.detail.slot == slot && model.detail.on) return model.detail;
    const am::Slot& s = model.slots[slot];
    fallback = am::Detail();
    fallback.slot = slot;
    strncpy(fallback.name, s.name, sizeof(fallback.name) - 1);
    fallback.st = s.st;
    fallback.elS = s.elS;
    fallback.rxMs = s.rxMs;
    return fallback;
}

static void render(uint32_t now) {
    bool alive = am::linkAlive(model, now, AM_LINK_TIMEOUT_MS);
    bool approval = approvalActive(now);
    if (detailSlot >= 0 && (!model.slots[detailSlot].on || now - detailTouchMs > AM_DETAIL_IDLE_MS)) {
        detailSlot = -1;   // 세션이 끝났거나 오래 안 봄 → 개요로
    }
    for (uint8_t i = 0; i < ds::kNumScreens; i++) {
        U8G2& u = display.screen(i);
        if (approval) {
            am::drawApproval(u, i, model.perm, now);
        } else if (!alive) {
            am::drawOffline(u, i, model.everRx, now);
        } else {
            if (detailSlot >= 0) am::drawDetail(u, i, detailFor(detailSlot), settings.muted, now);
            else am::drawSlot(u, model.slots[i], i, now);
            am::drawToast(u, model.toasts[i], now);
            if (detailSlot < 0 && i == ds::kNumScreens - 1 && (settings.muted || model.quiet)) am::drawMuteIcon(u);
        }
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
    store.begin("agentmon", &settings, kFields, sizeof(kFields) / sizeof(kFields[0]));
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
