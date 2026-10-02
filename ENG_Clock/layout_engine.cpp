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

/**
 * 음수에서도 내림으로 나눈다.
 * @details C++의 정수 나눗셈은 0 쪽으로 버리지만 웹 미러(JS Math.floor)는 내림이라,
 *         잉크 블록이 화면보다 넓어 `screenWidth - inkSpan`이 음수가 되면
 *         두 구현이 1px 어긋난다. 교차 검증(layout_crosscheck)이 정수를 그대로
 *         비교하므로 여기서语义를 맞춘다.
 */
static int floorDiv(int a, int b) {
    int q = a / b;
    if ((a % b) != 0 && ((a < 0) != (b < 0))) q--;
    return q;
}

int measureLineInk(const char* text, int start, int count,
                   InkWidthFn inkFn, void* ctx, int fallback) {
    if (!text || count <= 0) return 0;
    if (!inkFn) return fallback;          // 조회 수단 없으면 폰트 최대 그대로

    // [주의] 공백도 **한 셀**이다 — 어절 사이 공백이 셀 하나로 배치되므로
    //   카운트를 소모해야 한다. 다만 그려지지 않으므로(폴백 폰트도 빈 글리프)
    //   잉크 상한에는 기여하지 않는다.
    const char* stop = text + start;
    for (int i = 0; i < count && *stop; i++) stop += utf8CharLen(stop, 1);

    int maxInk = 0;
    for (const char* p = text + start; p < stop;) {
        if (*p == ' ') { p++; continue; }
        const int len = utf8CharLen(p, (int)(stop - p));
        const uint8_t w = inkFn(ctx, p, (uint8_t)len);
        if (w > maxInk) maxInk = w;
        p += len;
    }
    return maxInk ? maxInk : fallback;   // 공백뿐이거나 전부 조회 실패 → 폰트 최대
}

int measureLineFloor(const char* text, int start, int count,
                     InkWidthFn inkFn, void* ctx, int fallback) {
    if (!text || count <= 0) return 0;
    // 조회 수단이 없으면 줄 최대(=폴백)를 하한으로 쓴다 — 이건 항상 안전한 값이다.
    if (!inkFn) return fallback;

    const char* stop = text + start;
    for (int i = 0; i < count && *stop; i++) stop += utf8CharLen(stop, 1);

    // 잉크는 래스터 안에서 중앙 정렬 → 두 글자의 빈틈 = pitch − (inkA+inkB)/2.
    //   겹치지 않으려면 pitch ≥ (inkA+inkB)/2. 줄의 하한 = 인접 쌍의 최댓값.
    // 공백은 그려지지 않으므로 **쌍 계산에서 건너뛴다** — "A B"의 실제 간격은
    //   A와 B 사이 하나뿐이고, 공백을 사이에 넣으면 하한이 부풀려 간격이 벌어진다.
    int floorInk = 0;
    int prevInk = -1;                       // 직전에 만난 잉크 있는 글자
    bool seen = false;
    for (const char* p = text + start; p < stop;) {
        if (*p == ' ') { p++; continue; }
        const int len = utf8CharLen(p, (int)(stop - p));
        const int w = inkFn(ctx, p, (uint8_t)len);
        if (seen) {
            const int pair = (prevInk + w + 1) / 2;   // 올림 — 정수 픽셀에서 겹침 방지
            if (pair > floorInk) floorInk = pair;
        }
        prevInk = w;
        seen = true;
        p += len;
    }
    return floorInk ? floorInk : fallback;   // 한 글자뿐이거나 전부 조회 실패
}

int linePitch(int charCount, const CellGeometry& geom, int screenWidth,
              int lineInk, int fontInk, int lineFloor) {
    const int cellW = geom.glyphW;
    if (charCount <= 1) return cellW;         // 셀이 하나면 간격이 드러나지 않는다
    // 작은 폰트는 늘리지 않는다 — 이미 셀 안에 들어 있다 (요구사항).
    if (fontInk <= cellW) return cellW;

    // layoutWrap은 셀 수를 maxPerLine 이하로만 놓지만, 이 함수는 공개 API라 방어한다.
    if (charCount > geom.maxPerLine) charCount = geom.maxPerLine;

    // [사용자 지정 사다리] n 글자 줄의 **총 폭**을 (n+1) 글자분으로 맞춘다.
    //   9자 → 126(변화 없음) · 8자 → 9자폭 126 · 7자 → 8자폭 112 · 6자 → 7자폭 98 …
    //   5자 이하가 25·31·42·63으로 치솟던 것을 16·17·18·21로 눌러 준다.
    const int maxLine = geom.maxPerLine;
    const int total = (charCount + 1 > maxLine) ? maxLine * cellW : (charCount + 1) * cellW;
    int spread = total / charCount;

    // 잉크는 **목표가 아니라 두 개의 제한으로만** 쓴다.
    //   (목표로 썼던 적이 "너무 붙어"의 원인이었다 — 경계에 딱 붙는 간격이 나왔다)
    const int inkCap = (lineInk > cellW) ? lineInk : fontInk;

    // 1) 겹침 방지 — 하단은 '넘지 않을 값'이 아니라 **최소 필요한 값**이라 올린다.
    //    잉크를 모르면 하단을 만들지 않는다. 여기서 폰트 최댓값을 하한으로 쓰면
    //    "W가 넓은 폰트" 한 장Presence에 모든 짧은 줄이 43px까지 밀린다.
    const int floor = (lineFloor > cellW) ? lineFloor
                     : ((lineInk > cellW) ? lineInk : 0);
    if (floor > spread) spread = floor;

    // 2) 화면 밖 잘림 방지 — 잉크 블록 (n−1)·pitch + inkCap 가 화면을 넘으면 줄인다.
    //    잘림은 글자를 지우므로 겹침보다 나쁘다.
    const int fit = (screenWidth - inkCap) / (charCount - 1);
    if (spread > fit) spread = fit;

    if (spread < cellW) spread = cellW;       // 어떤 경우에도 셀 폭 아래로 내리지 않는다
    return spread;
}

int lineStartX(int charCount, int pitch, const CellGeometry& geom,
               int screenWidth, int lineInk) {
    const int cellW = geom.glyphW;
    // 피치가 셀 폭과 같으면(9자, 또는 작은 폰트) 기존 셀 중앙 정렬과 동일하다.
    if (pitch <= cellW) return (screenWidth - charCount * pitch) / 2;

    // 확장된 경우엔 잉크 블록을 화면 중앙에 둔다 — 잉크는 셀의 중앙(x + cellW/2)에
    // 놓이므로(래스터 xOffset = -(drawW-cellW)/2), 셀 폭으로 중앙 정렬하면
    // (pitch - cellW)/2 만큼 왼쪽으로 쏠린다.
    const int inkSpan = (charCount - 1) * pitch + lineInk;
    return floorDiv(screenWidth - inkSpan, 2) + (lineInk - cellW) / 2;
}

bool layoutWrap(const char* text, int textLen,
                const CellGeometry& geom,
                LayoutChar* out, int outCapacity, int screenWidth,
                int& outCount, bool singleLine, int inkWidth,
                InkWidthFn inkFn, void* inkCtx) {
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
        // [사용자 지정 사다리] 글자가 적은 줄일수록 간격이 넓어진다.
        // 9자 · 작은 폰트에서는 linePitch가 glyphW를 그대로 돌려주므로 좌표가 바뀌지 않는다.
        //
        // [§6.16b] 이 줄에 놓일 글자만 훑어 **줄 잉크**를 구한다. p는 이 줄의
        // 첫 위치가 아니라 **직전 줄까지 소비한 뒤의 위치** — 줄 경계의 공백은
        // 아래 배치 루프가 건너뛰므로, 여기서도 똑같이 한 칸 넘겨야 줄이 어긋나지 않는다.
        int lineStart = p;
        if (ln > 0) {
            while (lineStart < textLen && text[lineStart] == ' ') lineStart++;
        }
        const int lineInk = measureLineInk(text, lineStart, need, inkFn, inkCtx, inkWidth);
        // 이 줄의 겹침 없는 최소 피치 — 사다리보다 좁으면 이것까지 올린다.
        const int lineFloor = measureLineFloor(text, lineStart, need, inkFn, inkCtx, lineInk);
        const int pitch = linePitch(need, geom, screenWidth, lineInk, inkWidth, lineFloor);
        const int startX = lineStartX(need, pitch, geom, screenWidth, lineInk);

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
            out[idx].x = (int16_t)(startX + placed * pitch);
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
