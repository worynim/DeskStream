// worynim@gmail.com
/**
 * @file agent_ui.cpp
 * @brief agent_ui.h 구현
 * @details 128x64 배치
 *   y 0~13  : 상태 아이콘(12x12) + 세션(프로젝트) 이름
 *   y 15    : 구분선
 *   y 17~29 : 상태 이름 + 경과 시간(오른쪽)
 *   y 31~63 : 도구 이름 · 요약 2줄 (6x10 글꼴, 줄당 21자)
 */
#include "agent_ui.h"

#include <stdio.h>
#include <string.h>

#include "config.h"

namespace am {

namespace {

constexpr int kCols = 21;            // 6x10 글꼴 한 줄 글자 수
constexpr int kNumScreensWide = 4;   // 승인 화면 본문 한 줄이 이어지는 화면 수

/** 상태 아이콘 (중심 cx, cy, 반지름 6 영역) */
void drawIcon(U8G2& u, State st, int cx, int cy, uint32_t nowMs) {
    uint8_t frame = (nowMs / AM_ANIM_STEP_MS) % 4;
    switch (st) {
        case State::Idle:
            u.drawCircle(cx, cy, 5);
            break;
        case State::Think:   // 점 3개가 차례로 커진다
            for (int k = 0; k < 3; k++) u.drawDisc(cx - 4 + k * 4, cy, (frame % 3) == k ? 2 : 1);
            break;
        case State::Run: {   // 사분원 하나가 도는 스피너
            static const uint8_t quads[4] = {U8G2_DRAW_UPPER_RIGHT, U8G2_DRAW_LOWER_RIGHT, U8G2_DRAW_LOWER_LEFT,
                                             U8G2_DRAW_UPPER_LEFT};
            u.drawCircle(cx, cy, 5);
            u.drawDisc(cx, cy, 5, quads[frame]);
            break;
        }
        case State::Approve:
        case State::Input:
            u.drawDisc(cx, cy, 6);
            u.setDrawColor(0);
            u.setFont(u8g2_font_6x10_tf);
            u.drawStr(cx - 2, cy + 4, st == State::Approve ? "!" : "?");
            u.setDrawColor(1);
            break;
        case State::Done:
            u.drawLine(cx - 5, cy, cx - 2, cy + 4);
            u.drawLine(cx - 5, cy + 1, cx - 2, cy + 5);
            u.drawLine(cx - 2, cy + 4, cx + 5, cy - 4);
            u.drawLine(cx - 2, cy + 5, cx + 5, cy - 3);
            break;
        case State::Error:
            u.drawLine(cx - 4, cy - 4, cx + 4, cy + 4);
            u.drawLine(cx - 4, cy + 4, cx + 4, cy - 4);
            u.drawLine(cx - 3, cy - 4, cx + 5, cy + 4);
            u.drawLine(cx - 3, cy + 4, cx + 5, cy - 4);
            break;
    }
}

/** text 를 off 위치부터 최대 kCols 글자 그린다 (줄바꿈용) */
void drawCols(U8G2& u, int y, const char* text, size_t off) {
    char line[kCols + 1];
    size_t n = strlen(text);
    if (off >= n) return;
    strncpy(line, text + off, kCols);
    line[kCols] = '\0';
    u.drawStr(0, y, line);
}

}  // namespace

bool isAnimated(State st) {
    return st == State::Think || st == State::Run || st == State::Approve || st == State::Input;
}

/** 빈 슬롯 — 버튼 idx 를 길게 누르면 하는 일 */
static void drawEmptySlot(U8G2& u, uint8_t idx, const CmdInfo& cmd) {
    char buf[24];
    u.setFont(u8g2_font_6x10_tf);
    snprintf(buf, sizeof(buf), "SLOT %u  empty", idx + 1);
    u.drawStr(0, 10, buf);
    u.drawHLine(0, 13, 128);
    snprintf(buf, sizeof(buf), "HOLD BTN%u:", idx + 1);
    u.drawStr(0, 26, buf);
    if (!cmd.label[0]) {
        u.drawStr(0, 42, "(not set)");
        u.drawStr(0, 58, "agent_monitor.toml");
        return;
    }
    u.setFont(u8g2_font_7x13B_tf);
    u.drawStr(0, 43, cmd.label);
    u.setFont(u8g2_font_6x10_tf);
    const char* what = strcmp(cmd.kind, "focus") == 0 ? "open app"
                     : strcmp(cmd.kind, "shell") == 0 ? "run command"
                     : strcmp(cmd.kind, "keys") == 0  ? "send keys"
                                                      : "check config";
    u.drawStr(0, 58, what);
}

void drawSlot(U8G2& u, const Slot& s, uint8_t idx, const CmdInfo& cmd, uint32_t nowMs) {
    u.clearBuffer();
    u.setDrawColor(1);
    u.setFontMode(1);

    if (!s.on) {
        drawEmptySlot(u, idx, cmd);
        return;
    }

    // 머리줄: 아이콘 + 이름
    drawIcon(u, s.st, 6, 6, nowMs);
    u.setFont(u8g2_font_7x13B_tf);
    u.drawStr(16, 11, s.name);
    u.drawHLine(0, 15, 128);

    // 상태 + 경과 시간
    char el[12];
    formatElapsed(elapsedS(s, nowMs), el, sizeof(el));
    u.drawStr(0, 29, stateLabel(s.st));
    u.setFont(u8g2_font_6x10_tf);
    u.drawStr(128 - u.getStrWidth(el), 28, el);

    // 도구 + 요약 (완료 상태면 요약 = 마지막 답변)
    int y = 41;
    if (s.tool[0]) {
        u.drawStr(0, y, s.tool);
        if (s.err) {
            char e[8];
            snprintf(e, sizeof(e), "E%u", s.err);
            u.drawStr(128 - u.getStrWidth(e), y, e);
        }
        y += 11;
    }
    drawCols(u, y, s.sum, 0);
    if (y + 11 <= 63) drawCols(u, y + 11, s.sum, kCols);

    // 강조 표시
    if (s.st == State::Done) {
        u.drawFrame(0, 16, 128, 48);
        u.drawFrame(1, 17, 126, 46);
    }
    if ((s.st == State::Approve || s.st == State::Input) && (nowMs / AM_BLINK_MS) % 2) {
        u.setDrawColor(2);   // XOR — 화면 전체 반전
        u.drawBox(0, 0, 128, 64);
        u.setDrawColor(1);
    }
}

void drawApproval(U8G2& u, uint8_t idx, const Perm& p, uint32_t nowMs) {
    u.clearBuffer();
    u.setFontMode(1);

    // 머리줄 — 반전 바, 깜빡일 때는 테두리만
    bool solid = (nowMs / AM_BLINK_MS) % 2 == 0;
    u.setDrawColor(1);
    if (solid) u.drawBox(0, 0, 128, 14);
    else u.drawFrame(0, 0, 128, 14);
    u.setDrawColor(solid ? 0 : 1);
    u.setFont(u8g2_font_7x13B_tf);
    char head[24];
    switch (idx) {
        case 0: snprintf(head, sizeof(head), "APPROVE?"); break;
        case 1: snprintf(head, sizeof(head), "%s", p.name); break;
        case 2: snprintf(head, sizeof(head), "%s", p.tool); break;
        default: {
            char wait[12];
            formatElapsed((uint32_t)(nowMs - p.shownMs) / 1000, wait, sizeof(wait));
            snprintf(head, sizeof(head), "%u/%u %s", p.n, p.q, wait);
        }
    }
    u.drawStr(2, 11, head);
    u.setDrawColor(1);

    // 본문 — 요약을 화면 0→3 순서로 21자씩 이어서 2줄
    u.setFont(u8g2_font_6x10_tf);
    drawCols(u, 28, p.sum, idx * kCols);
    drawCols(u, 40, p.sum, (kNumScreensWide + idx) * kCols);

    // 버튼 라벨 (각 화면 아래 = 그 화면 밑의 버튼)
    static const char* const labels[4] = {"APPROVE", "NEXT >", "DENY", "LATER"};
    if (idx == 1 && p.q < 2) return;   // 대기 요청이 하나면 NEXT 없음
    int w = u.getStrWidth(labels[idx]) + 10;
    int x = 64 - w / 2;
    u.drawRFrame(x, 49, w, 15, 3);
    u.drawStr(x + 5, 60, labels[idx]);
}

void drawDetail(U8G2& u, uint8_t idx, const Detail& d, uint32_t nowMs) {
    u.clearBuffer();
    u.setDrawColor(1);
    u.setFontMode(1);
    u.setFont(u8g2_font_6x10_tf);
    char buf[24];

    switch (idx) {
        case 0: {
            u.setFont(u8g2_font_7x13B_tf);
            u.drawStr(0, 11, d.name);
            u.drawHLine(0, 14, 128);
            u.setFont(u8g2_font_6x10_tf);
            char el[12];
            formatElapsed(d.elS + (uint32_t)(nowMs - d.rxMs) / 1000, el, sizeof(el));
            u.drawStr(0, 25, stateLabel(d.st));
            u.drawStr(128 - u.getStrWidth(el), 25, el);
            snprintf(buf, sizeof(buf), "mode %s", d.mode[0] ? d.mode : "-");
            u.drawStr(0, 36, buf);
            snprintf(buf, sizeof(buf), "sub %u  err %u", d.sub, d.err);
            u.drawStr(0, 47, buf);
            break;
        }
        case 1:
            u.drawStr(0, 9, "RECENT TOOLS");
            for (int k = 0; k < 4; k++) u.drawStr(0, 20 + k * 10, d.h[k]);
            break;
        case 2:
            u.drawStr(0, 9, "LAST MESSAGE");
            for (int k = 0; k < 4; k++) drawCols(u, 20 + k * 10, d.msg, k * kCols);
            break;
        default:   // 화면 2 의 4줄 뒤를 이어 5줄
            for (int k = 0; k < 5; k++) drawCols(u, 9 + k * 10, d.msg, (4 + k) * kCols);
    }

    static const char* const labels[4] = {"BACK", "CLOSE", "< PREV", "NEXT >"};
    const char* label = labels[idx];
    int w = u.getStrWidth(label) + 10;
    int x = 64 - w / 2;
    u.drawRFrame(x, 51, w, 13, 3);
    u.drawStr(x + 5, 61, label);
}

void drawNotice(U8G2& u, const char* text) {
    char buf[kCols + 1];
    strncpy(buf, text, kCols);
    buf[kCols] = '\0';
    u.setDrawColor(1);
    u.drawBox(0, 50, 128, 14);
    u.setDrawColor(0);
    u.setFont(u8g2_font_6x10_tf);
    u.drawStr(1, 61, buf);
    u.setDrawColor(1);
}

void drawToast(U8G2& u, const CmdToast& t, uint32_t nowMs) {
    if (t.st == CmdState::None) return;
    if (t.st != CmdState::Run && nowMs - t.rxMs > AM_TOAST_MS) return;
    char buf[48];
    const char* tag = t.st == CmdState::Run ? "RUN" : t.st == CmdState::Ok ? "OK" : "FAIL";
    snprintf(buf, sizeof(buf), "%s %s %s", tag, t.label, t.msg);
    drawNotice(u, buf);
}

void drawMuteIcon(U8G2& u) {
    u.setDrawColor(0);
    u.drawBox(116, 0, 12, 10);   // 이름과 겹치면 지우고 그린다
    u.setDrawColor(1);
    u.drawBox(117, 3, 2, 4);                 // 스피커 몸통
    u.drawTriangle(119, 3, 122, 0, 122, 9);  // 나팔
    u.drawTriangle(119, 6, 122, 0, 122, 9);
    u.drawLine(124, 2, 127, 7);              // X
    u.drawLine(124, 7, 127, 2);
}

void drawOffline(U8G2& u, uint8_t idx, bool everConnected, uint32_t nowMs) {
    u.clearBuffer();
    u.setDrawColor(1);
    u.setFont(u8g2_font_6x10_tf);
    if (idx == 0) {
        u.setFont(u8g2_font_7x13B_tf);
        u.drawStr(0, 14, "Agent Monitor");
        u.setFont(u8g2_font_6x10_tf);
        u.drawStr(0, 28, "v" AM_FW_VERSION);
        u.drawStr(0, 46, everConnected ? "PC link lost" : "Waiting for PC");
        uint8_t dots = (nowMs / 400) % 4;
        for (uint8_t k = 0; k < dots; k++) u.drawStr(everConnected ? 72 + k * 6 : 84 + k * 6, 46, ".");
    } else if (idx == 1) {
        u.drawStr(0, 14, "Run on PC:");
        u.drawStr(0, 30, "python3 agent_bridge");
        u.drawStr(0, 42, "  .py run");
    }
}

}  // namespace am
