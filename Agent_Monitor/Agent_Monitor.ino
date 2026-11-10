// worynim@gmail.com
/**
 * @file Agent_Monitor.ino
 * @brief AI 개발 에이전트(Claude Code) 상태 모니터 — DeskStream 4-OLED
 * @details PC 데몬(bridge_py/agent_bridge.py)이 USB 시리얼로 보내는 세션 상태(JSON Lines)를
 *          OLED 4장(세션 슬롯 4개)에 표시하고, 상태 변화 알림을 부저로 낸다.
 *          - 개요 화면: 버튼 i 짧게 = 슬롯 i 상세 화면(+완료 확인), 길게 = PC 명령 i+1 (데몬 설정 파일).
 *          - 상세 화면: BTN1 개요로 · BTN2 세션 닫기(슬롯에서 지움) · BTN3 이전 · BTN4 다음 세션.
 *          - 음소거·OLED 밝기는 데몬 설정 파일 [device] 에서 받아(dev 메시지) NVS 에 저장한다 — 데몬이 없어도 유지.
 *            AM_DETAIL_IDLE_MS 동안 입력이 없거나 세션이 끝나면 개요로 돌아간다.
 *          - 승인 화면(권한 요청 대기 중): BTN1 승인 · BTN2 다음 요청 · BTN3 거절 · BTN4 나중에(터미널에 맡김).
 *            새 요청이 뜬 직후 AM_PERM_GUARD_MS 동안은 버튼을 무시한다.
 *          연결: USB 시리얼과 BLE UART(이름 AM_BLE_NAME) 둘 다 받는다 — 데몬이 USB 우선으로 하나를 쓴다.
 *          BLE 로 보내는 버튼·승인 신호는 USB 로 등록한 키로 서명한다 (agent_auth.h).
 *          프로토콜: bridge_py/protocol.py 참조.
 */
#include <Arduino.h>
#include <DeskStreamCore.h>

#include "agent_auth.h"
#include "agent_ble.h"
#include "agent_state.h"
#include "agent_ui.h"
#include "config.h"

static ds::DisplayCore display;
static ds::Buttons buttons;
static ds::Buzzer buzzer;
static am::Model model;

// 장치 설정 (NVS) — 음소거
struct Settings {
    bool muted = false;       // 켜면 버튼음·알림음·부팅음 모두 끔
    uint8_t brightness = 255; // OLED contrast (1~255)
};
static Settings settings;
static ds::ConfigStore store;
static const ds::ConfigField kFields[] = {
    DS_CFG_BOOL(Settings, muted, "muted", "muted", nullptr),
    DS_CFG_U8(Settings, brightness, "brightness", "bright", 1, 255, nullptr),
};

// 화면 모드 — 승인 화면이 오면 그것이 우선한다
static int8_t detailSlot = -1;       // 상세 화면으로 보는 슬롯 (-1 = 개요)
static uint8_t detailPage = 0;       // 상세 화면 마지막 답변 페이지 (0부터)
static uint32_t detailTouchMs = 0;   // 상세 화면에서 마지막으로 버튼을 누른 시각

// 알림음 (재생이 끝날 때까지 살아 있어야 하므로 static const)
static const ds::Note MELODY_DONE[] = {{2093, 80, 30}, {3136, 120, 0}};
static const ds::Note MELODY_ATTN[] = {{2637, 70, 50}, {2637, 70, 50}, {2637, 70, 0}};
static const ds::Note MELODY_ERROR[] = {{880, 200, 60}, {659, 300, 0}};
static const ds::Note MELODY_START[] = {{2093, 60, 20}, {2637, 60, 20}, {3136, 60, 0}};

/** 데몬 설정 파일의 음소거·밝기를 적용하고, 바뀌었으면 NVS 에 저장한다 */
static void applyDeviceSettings(bool mute, uint8_t bright) {
    if (mute == settings.muted && bright == settings.brightness) return;
    if (mute && !settings.muted) buzzer.stop();   // 재생 중인 소리도 멈춤
    settings.muted = mute;
    settings.brightness = bright;
    display.setBrightness(bright);
    store.setDirty();
}

/** 버튼 클릭음 — 음소거면 내지 않는다 */
static void click(uint16_t ms, uint16_t hz) {
    if (!settings.muted) buzzer.beep(ms, hz);
}

static void playAlert(am::State kind) {
    if (settings.muted) return;
    switch (kind) {
        case am::State::Done:    buzzer.playMelody(MELODY_DONE, 2); break;
        case am::State::Approve:
        case am::State::Input:   buzzer.playMelody(MELODY_ATTN, 3); break;
        case am::State::Error:   buzzer.playMelody(MELODY_ERROR, 2); break;
        default: break;
    }
}

/** 한 줄을 USB·BLE 양쪽으로 보낸다 (데몬은 연결한 쪽 하나만 읽는다) */
static void writeLine(const char* line) {
    Serial.println(line);
    am::bleWriteLine(line);
}

/** 버튼·승인 신호 — 카운터와 서명을 붙여 보낸다 */
static void sendSigned(const char* body) {
    char out[256];   // btn·decide 본문 + 카운터 + 서명 (스택 절약 — AM_LINE_MAX 는 수신 줄 한계)
    writeLine(am::authSignLine(body, out, sizeof(out)));
}

static void sendHello() {
    char line[80];
    snprintf(line, sizeof(line), "{\"t\":\"hello\",\"fw\":\"%s\",\"kid\":\"%s\"}", AM_FW_VERSION, am::authKid());
    writeLine(line);
}

/** 버튼 이벤트를 데몬에 보낸다 */
static void sendButton(uint8_t b, const char* press) {
    char body[64];
    snprintf(body, sizeof(body), "{\"t\":\"btn\",\"b\":%u,\"press\":\"%s\"}", b, press);
    sendSigned(body);
}

/** 승인 화면 결정을 데몬에 보낸다 (act = allow | next | deny | pass) */
static void sendDecision(uint32_t id, const char* act) {
    char body[64];
    snprintf(body, sizeof(body), "{\"t\":\"decide\",\"id\":%lu,\"act\":\"%s\"}", (unsigned long)id, act);
    sendSigned(body);
}

/** 승인 화면이 떠 있는지 (데몬 연결 중 + 대기 요청 있음) */
static bool approvalActive(uint32_t now) {
    return model.perm.id != 0 && am::linkAlive(model, now, AM_LINK_TIMEOUT_MS);
}

/** 데몬이 보낸 한 줄 처리. setkey 는 USB 에서만, nonce 는 BLE 에서만 받는다. who 에는 hello 로 답한다 */
static void handleLine(const char* line, bool fromUsb) {
    char type[8], val[72];
    if (ds::jsonGetString(line, "t", type, sizeof(type))) {
        if (strcmp(type, "who") == 0) {   // 이미 켜져 있던 장치에 데몬이 새로 붙었을 때 — 키 id 확인용
            sendHello();
            return;
        }
        if (strcmp(type, "setkey") == 0) {
            if (fromUsb && ds::jsonGetString(line, "key", val, sizeof(val)) && am::authSetKey(val)) sendHello();
            return;
        }
        if (strcmp(type, "nonce") == 0) {
            if (!fromUsb && ds::jsonGetString(line, "n", val, sizeof(val))) {
                am::authSetNonce(val);
                sendHello();   // 데몬이 키 일치 여부(kid)를 확인한다
            }
            return;
        }
    }
    am::ParseResult r = am::applyLine(model, line, millis());
    if (r.type == am::MsgType::Alert) playAlert(r.alert);
    if (r.type == am::MsgType::Dev) applyDeviceSettings(model.dev.mute, model.dev.bright);
}

/** 바이트를 모아 줄 단위로 handleLine 에 넘긴다 (넘치는 줄은 버림) */
struct LineAssembler {
    char buf[AM_LINE_MAX];
    size_t len = 0;
    bool overflow = false;

    void feed(char c, bool fromUsb) {
        if (c == '\r') return;
        if (c != '\n') {
            if (len < sizeof(buf) - 1) buf[len++] = c;
            else overflow = true;
            return;
        }
        buf[len] = '\0';
        if (!overflow && len) handleLine(buf, fromUsb);
        len = 0;
        overflow = false;
    }
};

static void pollLinks() {
    static LineAssembler usb, ble;
    while (Serial.available()) usb.feed((char)Serial.read(), true);
    char chunk[64];
    size_t n;
    while ((n = am::bleRead(chunk, sizeof(chunk))) > 0) {
        for (size_t i = 0; i < n; i++) ble.feed(chunk[i], false);
    }
}

/** 승인 화면의 버튼 i 짧게 누름 */
static void onApprovalButton(uint8_t i, uint32_t now) {
    static const char* const acts[4] = {"allow", "next", "deny", "pass"};
    const am::Perm& p = model.perm;
    if (now - p.shownMs < AM_PERM_GUARD_MS) return;   // 방금 뜬 요청 — 다른 걸 누르려던 입력일 수 있다
    if (i == 1 && p.q < 2) return;                     // 다음 요청 없음
    click(AM_BEEP_MS, AM_BEEP_HZ);
    sendDecision(p.id, acts[i]);
}

/** 상세 화면으로 slot 을 연다 — 데몬에 알려 그 세션의 상세 정보를 받는다 (완료 확인도 함께) */
static void openDetail(int8_t slot, uint32_t now) {
    detailSlot = slot;
    detailPage = 0;
    detailTouchMs = now;
    sendButton(slot, "short");
}

/** 상세 화면의 버튼 i 짧게 누름 — BACK · CLOSE · 이전 페이지 · 다음 페이지 */
static void onDetailButton(uint8_t i, uint32_t now) {
    detailTouchMs = now;
    switch (i) {
        case 0: detailSlot = -1; break;
        case 1:   // 세션 닫기 — 데몬이 슬롯에서 지운다 (그 세션에 다시 이벤트가 오면 다시 생김)
            sendButton(detailSlot, "close");
            detailSlot = -1;
            break;
        case 2:   // 마지막 답변 이전 페이지
            if (detailPage > 0) detailPage--;
            break;
        default:  // 다음 페이지
            if (detailPage + 1 < am::detailPageCount()) detailPage++;
            break;
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
            click(AM_BEEP_MS, AM_BEEP_HZ);
            if (detailSlot >= 0) onDetailButton(i, now);
            else if (model.slots[i].on) openDetail(i, now);
        }
        if (ev & ds::BUTTON_LONG) {
            click(AM_BEEP_MS * 3, AM_BEEP_HZ / 2);
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
            if (i == 0 && am::bleConnected() && !am::authHasKey()) am::drawNotice(u, "PAIR VIA USB");
        } else if (!alive) {
            am::drawOffline(u, i, model.everRx, now);
        } else {
            if (detailSlot >= 0) am::drawDetail(u, i, detailFor(detailSlot), detailPage, now);
            else am::drawSlot(u, model.slots[i], i, model.cmds[i], now);
            am::drawToast(u, model.toasts[i], now);
            if (detailSlot < 0 && i == ds::kNumScreens - 1 && settings.muted) am::drawMuteIcon(u);
        }
    }
    display.push();   // 바뀐 타일만 전송
}

void setup() {
    Serial.setRxBufferSize(4096);   // 긴 줄(상세 답변 약 2KB)이 와도 loop 가 읽기 전에 넘치지 않게 — begin 보다 먼저
    Serial.begin(AM_SERIAL_BAUD);
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(0);   // PC 가 포트를 열지 않아도 loop() 가 멈추지 않게
#endif
    display.begin();
    buttons.begin();
    buzzer.begin();
    store.begin("agentmon", &settings, kFields, sizeof(kFields) / sizeof(kFields[0]));
    display.setBrightness(settings.brightness);   // 마지막으로 받은 밝기 (데몬 연결 전에도)
    am::authBegin();
    am::bleBegin(AM_BLE_NAME);
    if (!settings.muted) buzzer.playMelody(MELODY_START, 3);
    sendHello();
}

void loop() {
    pollLinks();
    pollButtons();
    static uint32_t lastFrame = 0;
    uint32_t now = millis();
    if (now - lastFrame >= AM_FRAME_MS) {
        lastFrame = now;
        render(now);
    }
}
