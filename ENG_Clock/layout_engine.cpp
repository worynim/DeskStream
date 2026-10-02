// worynim@gmail.com
/**
 * @file layout_engine.cpp
 * @brief 어절 단위 2줄 레이아웃 계산 구현
 * @note [SYNC] 원본: Hangeul_Clock/renderer.cpp의 getCharData()
 *       원본은 글자 단위 단순 배치였다. 영어 단어(최대 9자)를 위해 어절 단위로 바꾼다.
 */
#include "layout_engine.h"

/** UTF-8 리딩 바이트로부터 해당 문자의 바이트 길이를 구한다. */
static uint8_t utf8CharLen(const char* p, int remaining) {
    if (remaining <= 0) return 0;
    unsigned char c = (unsigned char)*p;
    uint8_t len = 1;
    if ((c & 0xE0) == 0xC0) len = 2;
    else if ((c & 0xF0) == 0xE0) len = 3;
    else if ((c & 0xF8) == 0xF0) len = 4;
    if (len > (uint8_t)remaining) len = (uint8_t)remaining;   // 잘린 멀티바이트 방지
    return len;
}

bool layoutWrap(const char* text, int textLen,
                const CellGeometry& geom,
                LayoutChar* out, int outCapacity, int screenWidth,
                int& outCount, bool singleLine) {
    outCount = 0;
    if (!out || outCapacity <= 0) return false;
    if (!text || textLen <= 0) return true;

    const int maxPerLine = geom.maxPerLine;
    if (maxPerLine <= 0) return false;

    // --- 1단계: 어절을 줄에 배정 (Greedy) ---
    // lines[]에 각 줄의 글자 수를 쌓는다. 최대 LAYOUT_MAX_LINES 줄.
    int lineCount[LAYOUT_MAX_LINES] = { 0, 0 };
    int lines = 1;
    int droppedChars = 0;   // 2줄 안에 못 들어간 글자 수
    bool seenWord = false;
    bool pendingSpace = false;   // 어절 사이 공백 — 다음 어절이 같은 줄에 올 때만 센다

    int i = 0;
    while (i < textLen) {
        if (text[i] == ' ') {
            // [수정할 사항 1] 공백을 즉시 세지 않는다 — 줄 경계의 공백은 버려지고,
            // 같은 줄에 이어지는 어절 사이의 공백만 빈 셀 한 칸이 된다. 복수 공백은 한 칸으로 합친다.
            pendingSpace = true;
            i++;
            continue;
        }

        int wordChars = 0;
        while (i < textLen && text[i] != ' ') {
            i += utf8CharLen(text + i, textLen - i);
            wordChars++;
        }
        if (wordChars == 0) break;

        // 어절 사이 공백 한 칸. 첫 어절 앞의 공백은 세지 않는다.
        // (pendingSpace를 여기서 소비한다 — 이 어절부터는 새 공백만 pending이 된다)
        int gap = (pendingSpace && seenWord) ? 1 : 0;
        pendingSpace = false;

        // 어절이 2개 이상이면 첫 어절은 줄 0에, 두 번째 어절부터 줄 1로 넘긴다.
        // ("FORTY ONE"은 9자라 예전엔 1줄에 들어갔지만, 요구사항상 항상 2줄로 나눈다)
        // [수정할 사항 1] singleLine=true(숫자 모드 "02 H")이면 이 규칙을 건너뛴다 —
        //   숫자+단위는 한 줄이 의도이며 최대 5자라 항상 들어간다.
        if (seenWord && !singleLine && lines < LAYOUT_MAX_LINES) {
            lines = LAYOUT_MAX_LINES;
            lineCount[lines - 1] = 0;
            gap = 0;   // 줄 경계 → 어절 사이 공백은 버려진다
        }
        seenWord = true;

        if (lineCount[lines - 1] + gap + wordChars <= maxPerLine) {
            lineCount[lines - 1] += gap + wordChars;
            continue;
        }

        // 단어는 **전부 또는 아무것도** — 절대 쪼개지 않는다.
        // 쪼개면 "THIRTYSEV" 같은 비가독 문자열이 되므로, 배치하지 않고 잘림으로 보고한다.
        // 영어 시각 표현은 최장 9자("SEVENTEEN")이므로 maxPerLine 이상인 단어는
        // 실제로 발생하지 않지만, 커스텀 폰트/확장 어휘를 위해 안전하게 처리한다.
        // singleLine 모드에서는 현재 줄에 못 들면 빈 다음 줄로 내려간다.
        if (singleLine && lines < LAYOUT_MAX_LINES) {
            lines++;
            lineCount[lines - 1] = 0;
            gap = 0;   // 줄 경계 → 어절 사이 공백은 버려진다
            if (lineCount[lines - 1] + wordChars <= maxPerLine) {
                lineCount[lines - 1] += wordChars;
                continue;
            }
        }
        droppedChars += wordChars;
        break;   // 3줄로 넘어가지 않는다
    }

    // --- 2단계: 실제 문자 단위 배치 + x/y 좌표 배정 ---
    // 세로 중앙: 1줄이면 (64-32)/2 = 16, 2줄이면 (64-64)/2 = 0.
    const int baseY = (LAYOUT_SCREEN_HEIGHT - lines * LAYOUT_LINE_HEIGHT) / 2;
    int idx = 0;
    int p = 0;
    bool capacityExceeded = false;

    for (int ln = 0; ln < lines; ln++) {
        int need = lineCount[ln];
        int placed = 0;

        while (p < textLen && placed < need) {
            // 줄 경계의 공백은 배치하지 않는다 (1단계에서도 세지 않았다).
            // 줄이 시작되기 전의 공백만 건너뛴다 — 같은 줄 어절 사이 공백은 셀로 배출한다.
            if (placed == 0 && text[p] == ' ') { p++; continue; }
            if (p >= textLen) break;
            if (idx >= outCapacity) { capacityExceeded = true; break; }

            int clen = utf8CharLen(text + p, textLen - p);
            out[idx].text = text + p;
            out[idx].len = (uint8_t)clen;
            out[idx].line = (uint8_t)ln;
            out[idx].x = (int16_t)((screenWidth - need * geom.glyphW) / 2 + placed * geom.glyphW);
            out[idx].y = (int16_t)(baseY + ln * LAYOUT_LINE_HEIGHT);
            idx++;
            placed++;
            p += clen;
        }
        if (capacityExceeded) break;
    }

    outCount = idx;
    // 잘림이 전혀 없어야 완전한 배치로 본다.
    return !capacityExceeded && droppedChars == 0;
}

int layoutLineCount(const LayoutChar* out, int count, uint8_t line) {
    int n = 0;
    for (int i = 0; i < count; i++) {
        if (out[i].line == line) n++;
    }
    return n;
}
