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

constexpr int kCols = 21;   // 6x10 글꼴 한 줄 글자 수

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

void drawSlot(U8G2& u, const Slot& s, uint8_t idx, uint32_t nowMs) {
    u.clearBuffer();
    u.setDrawColor(1);
    u.setFontMode(1);

    if (!s.on) {
        u.setFont(u8g2_font_6x10_tf);
        char buf[12];
        snprintf(buf, sizeof(buf), "SLOT %u", idx + 1);
        u.drawStr(64 - u.getStrWidth(buf) / 2, 30, buf);
        u.drawStr(64 - u.getStrWidth("empty") / 2, 44, "empty");
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
