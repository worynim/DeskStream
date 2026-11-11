// worynim@gmail.com
/**
 * @file agent_ui.cpp
 * @brief agent_ui.h 구현
 * @details 128x64 배치
 *   y 0~13  : 상태 아이콘(12x12) + 세션(프로젝트) 이름
 *   y 15    : 구분선
 *   y 17~29 : 상태 이름 + 경과 시간(오른쪽)
 *   y 31~63 : 도구 이름 · 요약 2줄 (갈무리7 글꼴, 줄 폭 128px — 한글 16자 · 영문 약 25자)
 * @details 글꼴은 갈무리7 하나(영문·한글). 제목은 글자마다 1px 어긋나게 두 번 그리고 간격을 넓혀 굵게 보이게 한다.
 */
// 갈무리7 한글 글꼴은 64KB 를 넘어 U8g2 가 큰 글꼴 지원을 켜야 한다 — U8g2lib.h 보다 먼저 정의
#ifndef U8G2_USE_LARGE_FONTS
#define U8G2_USE_LARGE_FONTS
#endif
#include "agent_ui.h"

#include <DeskStreamCore.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "u8g2_font_galmuri7_kr.h"

namespace am {

namespace {

constexpr int kLineW = 128;          // 화면 한 줄 폭(px)
constexpr int kNumScreensWide = 4;   // 승인 화면 본문이 줄 단위로 이어지는 화면 수
constexpr int kPageLines = 9;        // 상세 화면 마지막 답변 한 페이지 = 화면 2 장(4줄 + 5줄)

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
            u.setFont(u8g2_font_galmuri7_kr);
            u.drawUTF8(cx - 2, cy + 4, st == State::Approve ? "!" : "?");
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

constexpr int kBoldGap = 2;   // 굵은 글씨 글자 간격 추가(px) — 겹쳐 그리면 획이 1px 굵어져 보통 간격으로는 붙어 보인다

/** 제목용 굵은 글씨 — 글자마다 1px 어긋나게 두 번 그리고, 붙어 보이지 않게 글자 간격을 kBoldGap 만큼 넓힌다 */
void drawBold(U8G2& u, int x, int y, const char* text) {
    size_t n = strlen(text);
    for (size_t i = 0; i < n;) {
        uint8_t len = ds::utf8CharLen(text + i, (int)(n - i));
        char ch[5] = {};
        memcpy(ch, text + i, len);
        u.drawUTF8(x, y, ch);
        u.drawUTF8(x + 1, y, ch);
        x += u.getUTF8Width(ch) + kBoldGap;
        i += len;
    }
}

/** off 앞의 공백(줄바꿈 자리)을 건너뛴 위치 */
size_t skipSpaces(const char* text, size_t off) {
    while (text[off] == ' ') off++;
    return off;
}

/**
 * UTF-8 글자 하나(len 바이트)가 줄에서 차지하는 폭(px) — 글자 사이 여백 포함.
 * getUTF8Width 는 마지막 글자의 여백을 빼고 재므로, 글자마다 불러 더하면 실제 줄 폭보다 짧게 나온다.
 */
int charAdvance(U8G2& u, const char* p, uint8_t len) {
    uint16_t cp = len == 1   ? (uint8_t)p[0]
                  : len == 2 ? ((p[0] & 0x1F) << 6) | (p[1] & 0x3F)
                  : len == 3 ? ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F)
                             : 0;   // 4바이트 문자는 글꼴에 없다 (그려지지 않음)
    return u8g2_GetGlyphWidth(u.getU8g2(), cp);
}

/** off 부터 한 줄(kLineW px)에 들어가는 만큼의 끝 위치 — UTF-8 글자 단위로 자른다 */
size_t lineEnd(U8G2& u, const char* text, size_t off) {
    size_t n = strlen(text), i = off;
    int w = 0;
    while (i < n) {
        uint8_t len = ds::utf8CharLen(text + i, (int)(n - i));
        w += charAdvance(u, text + i, len);
        if (w > kLineW) break;
        i += len;
    }
    return i;
}

/** text 의 off 위치부터 한 줄 그리고 다음 줄 시작 위치를 돌려준다 (줄바꿈용) */
size_t drawLine(U8G2& u, int y, const char* text, size_t off) {
    off = skipSpaces(text, off);
    size_t end = lineEnd(u, text, off);
    char line[kLineW + 1];   // 글자 폭이 최소 2px 이므로 한 줄은 64바이트를 넘지 않는다
    size_t len = end - off < sizeof(line) - 1 ? end - off : sizeof(line) - 1;
    memcpy(line, text + off, len);
    line[len] = '\0';
    u.drawUTF8(0, y, line);
    return end;
}

/** text 의 from 위치부터 한 줄씩 n 줄 건너뛴 위치 */
size_t skipLines(U8G2& u, const char* text, size_t from, int n) {
    size_t off = from;
    while (n-- > 0 && text[off]) off = lineEnd(u, text, skipSpaces(text, off));
    return off;
}

/** 마지막 답변의 페이지 나눔 — 줄 계산이 무거워서(글자마다 폭 조회) 답변이 바뀔 때만 다시 계산한다 */
struct PageLayout {
    bool valid = false;
    int8_t slot = -1;
    uint32_t rxMs = 0;
    size_t len = 0;
    uint8_t pages = 1;
    size_t start[kMaxMsgPages] = {};   // 페이지별 시작 바이트 위치
};
PageLayout layout;

void layoutPages(U8G2& u, const Detail& d) {
    size_t len = strlen(d.msg);
    if (layout.valid && layout.slot == d.slot && layout.rxMs == d.rxMs && layout.len == len) return;
    layout.valid = true;
    layout.slot = d.slot;
    layout.rxMs = d.rxMs;
    layout.len = len;
    layout.pages = 1;
    layout.start[0] = 0;
    u.setFont(u8g2_font_galmuri7_kr);
    size_t off = 0;
    for (int lines = 1;; lines++) {
        off = skipSpaces(d.msg, off);
        size_t end = d.msg[off] ? lineEnd(u, d.msg, off) : off;
        if (end == off) break;                       // 끝 (또는 진행 불가)
        off = end;
        if (lines % kPageLines == 0 && d.msg[skipSpaces(d.msg, off)]) {
            if (layout.pages >= kMaxMsgPages) break;   // 4 페이지를 넘는 뒷부분은 보여주지 않는다
            layout.start[layout.pages++] = off;
        }
    }
}

}  // namespace

bool isAnimated(State st) {
    return st == State::Think || st == State::Run || st == State::Approve || st == State::Input;
}

/** 사용량 한 구간 — 제목 · 사용률(숫자 + 막대) · 리셋까지 남은 시간 */
static void drawUsage(U8G2& u, int window, const Usage& usage, uint32_t nowMs) {
    u.setFont(u8g2_font_galmuri7_kr);
    drawBold(u, 0, 10, window == 0 ? "5H USAGE" : "WEEK USAGE");
    u.drawHLine(0, 13, 128);
    if (!usage.known) {
        u.drawUTF8(0, 30, "--");
        return;
    }
    char buf[20];
    snprintf(buf, sizeof(buf), "%u%%", usage.pct);
    drawBold(u, 0, 29, buf);
    u.drawFrame(0, 34, 128, 10);
    u.drawBox(2, 36, 124 * usage.pct / 100, 6);
    uint32_t left = remainS(usage, nowMs);
    if (left == 0) return;   // 리셋 시각을 모르거나 이미 지남
    char t[12];
    formatRemain(left, t, sizeof(t));
    snprintf(buf, sizeof(buf), "reset %s", t);
    u.drawUTF8(0, 58, buf);
}

/** 빈 슬롯 — 마지막 kUsageWindows 화면은 사용량, 나머지는 슬롯 번호만 */
static void drawEmptySlot(U8G2& u, uint8_t idx, const Usage* usage, uint32_t nowMs) {
    int window = idx - (kSlots - kUsageWindows);
    if (window >= 0) {
        drawUsage(u, window, usage[window], nowMs);
        return;
    }
    char buf[24];
    u.setFont(u8g2_font_galmuri7_kr);
    snprintf(buf, sizeof(buf), "SLOT %u  empty", idx + 1);
    u.drawUTF8(0, 10, buf);
    u.drawHLine(0, 13, 128);
}

void drawSlot(U8G2& u, const Slot& s, uint8_t idx, const Usage* usage, uint32_t nowMs) {
    u.clearBuffer();
    u.setDrawColor(1);
    u.setFontMode(1);

    if (!s.on) {
        drawEmptySlot(u, idx, usage, nowMs);
        return;
    }

    // 머리줄: 아이콘 + 이름
    drawIcon(u, s.st, 6, 6, nowMs);
    u.setFont(u8g2_font_galmuri7_kr);
    drawBold(u, 16, 11, s.name);
    u.drawHLine(0, 15, 128);

    // 상태 + 경과 시간
    char el[12];
    formatElapsed(elapsedS(s, nowMs), el, sizeof(el));
    drawBold(u, 0, 29, stateLabel(s.st));
    u.setFont(u8g2_font_galmuri7_kr);
    u.drawUTF8(128 - u.getUTF8Width(el), 28, el);

    // 도구 + 요약 (완료 상태면 요약 = 마지막 답변)
    int y = 41;
    if (s.tool[0]) {
        u.drawUTF8(0, y, s.tool);
        if (s.err) {
            char e[8];
            snprintf(e, sizeof(e), "E%u", s.err);
            u.drawUTF8(128 - u.getUTF8Width(e), y, e);
        }
        y += 11;
    }
    size_t off = drawLine(u, y, s.sum, 0);
    if (y + 11 <= 63) drawLine(u, y + 11, s.sum, off);

    // 강조 표시
    if (s.st == State::Done) {
        u.setDrawColor(2);   // XOR — 구분선 아래 영역 반전
        u.drawBox(0, 16, 128, 48);
        u.setDrawColor(1);
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
    u.setFont(u8g2_font_galmuri7_kr);
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
    drawBold(u, 2, 11, head);
    u.setDrawColor(1);

    // 본문 — 요약을 한 줄(128px)씩 화면 0→3 순서로 이어서 2줄
    u.setFont(u8g2_font_galmuri7_kr);
    drawLine(u, 28, p.sum, skipLines(u, p.sum, 0, idx));
    drawLine(u, 40, p.sum, skipLines(u, p.sum, 0, kNumScreensWide + idx));

    // 버튼 라벨 (각 화면 아래 = 그 화면 밑의 버튼)
    static const char* const labels[4] = {"APPROVE", "NEXT >", "DENY", "LATER"};
    if (idx == 1 && p.q < 2) return;   // 대기 요청이 하나면 NEXT 없음
    int w = u.getUTF8Width(labels[idx]) + 10;
    int x = 64 - w / 2;
    u.drawRFrame(x, 49, w, 15, 3);
    u.drawUTF8(x + 5, 60, labels[idx]);
}

uint8_t detailPageCount() { return layout.pages; }

void drawDetail(U8G2& u, uint8_t idx, const Detail& d, uint8_t page, uint32_t nowMs) {
    u.clearBuffer();
    u.setDrawColor(1);
    u.setFontMode(1);
    u.setFont(u8g2_font_galmuri7_kr);
    layoutPages(u, d);
    if (page >= layout.pages) page = layout.pages - 1;
    char buf[24];

    switch (idx) {
        case 0: {
            drawBold(u, 0, 11, d.name);
            u.drawHLine(0, 14, 128);
            char el[12];
            formatElapsed(d.elS + (uint32_t)(nowMs - d.rxMs) / 1000, el, sizeof(el));
            u.drawUTF8(0, 25, stateLabel(d.st));
            u.drawUTF8(128 - u.getUTF8Width(el), 25, el);
            snprintf(buf, sizeof(buf), "mode %s", d.mode[0] ? d.mode : "-");
            u.drawUTF8(0, 36, buf);
            snprintf(buf, sizeof(buf), "sub %u  err %u", d.sub, d.err);
            u.drawUTF8(0, 47, buf);
            break;
        }
        case 1:
            u.drawUTF8(0, 9, "RECENT TOOLS");
            for (int k = 0; k < 4; k++) u.drawUTF8(0, 20 + k * 10, d.h[k]);
            break;
        case 2: {
            u.drawUTF8(0, 9, "LAST MESSAGE");
            if (layout.pages > 1) {
                snprintf(buf, sizeof(buf), "%u/%u", page + 1, layout.pages);
                u.drawUTF8(128 - u.getUTF8Width(buf), 9, buf);
            }
            size_t off = layout.start[page];
            for (int k = 0; k < 4; k++) off = drawLine(u, 20 + k * 10, d.msg, off);
            break;
        }
        default: {   // 같은 페이지의 뒤 5줄
            size_t off = skipLines(u, d.msg, layout.start[page], 4);
            for (int k = 0; k < 5; k++) off = drawLine(u, 9 + k * 10, d.msg, off);
        }
    }

    // 아래 라벨 — 화면 2·3 은 페이지를 넘길 수 있을 때만 (BTN3 = 이전 페이지, BTN4 = 다음 페이지)
    static const char* const labels[4] = {"BACK", "CLOSE", "< PREV", "NEXT >"};
    if (idx == 2 && page == 0) return;
    if (idx == 3 && page + 1 >= layout.pages) return;
    const char* label = labels[idx];
    int w = u.getUTF8Width(label) + 10;
    int x = 64 - w / 2;
    u.drawRFrame(x, 51, w, 13, 3);
    u.drawUTF8(x + 5, 61, label);
}

void drawNotice(U8G2& u, const char* text) {
    u.setDrawColor(1);
    u.drawBox(0, 50, 128, 14);
    u.setDrawColor(0);
    u.setFont(u8g2_font_galmuri7_kr);
    drawLine(u, 61, text, 0);
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
    u.setFont(u8g2_font_galmuri7_kr);
    if (idx == 0) {
        u.setFont(u8g2_font_galmuri7_kr);
        drawBold(u, 0, 14, "Agent Monitor");
        u.setFont(u8g2_font_galmuri7_kr);
        u.drawUTF8(0, 28, "v" AM_FW_VERSION);
        u.drawUTF8(0, 46, everConnected ? "PC link lost" : "Waiting for PC");
        uint8_t dots = (nowMs / 400) % 4;
        for (uint8_t k = 0; k < dots; k++) u.drawUTF8(everConnected ? 72 + k * 6 : 84 + k * 6, 46, ".");
    } else if (idx == 1) {
        u.drawUTF8(0, 14, "Run on PC:");
        u.drawUTF8(0, 30, "python3 agent_bridge");
        u.drawUTF8(0, 42, "  .py run");
    }
}

}  // namespace am
