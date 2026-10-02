// worynim@gmail.com
/**
 * @file test_layout.cpp
 * @brief 어절 단위 2줄 레이아웃 네이티브 단위 테스트
 * @details 빌드: g++ -std=c++11 -I.. test_layout.cpp ../layout_engine.cpp ../renderer_geometry.cpp \
 *       ../english_time_core.cpp -o test_layout
 * @note 검증 범위: 모든 가능한 영어 시간 표현이 2줄 안에 올바르게 들어가는지.
 */
#include <cstdio>
#include <cstring>
#include <string>
#include "layout_engine.h"
#include "english_time_core.h"

static int g_pass = 0;
static int g_fail = 0;

static void check(const char* label, bool ok) {
    if (ok) { g_pass++; }
    else { g_fail++; printf("  FAIL  %s\n", label); }
}

/** layoutWrap 결과 → 사람이 읽는 "줄1 / 줄2" 문자열로 변환 */
static std::string render(const char* text) {
    const CellGeometry& g = *geometryForSize(64);
    LayoutChar out[LAYOUT_MAX_CHARS];
    int count = 0;
    layoutWrap(text, (int)strlen(text), g, out, LAYOUT_MAX_CHARS, 128, count);

    std::string lines[2];
    for (int i = 0; i < count; i++) {
        lines[out[i].line] += std::string(out[i].text, out[i].len);
    }
    if (count == 0) return "(empty)";
    if (lines[1].empty()) return lines[0];
    return lines[0] + " / " + lines[1];
}

/** 줄1·줄2의 글자 수가 각각 9 이하인지 */
static bool withinTwoLines(const char* text, int* line0, int* line1) {
    const CellGeometry& g = *geometryForSize(64);
    LayoutChar out[LAYOUT_MAX_CHARS];
    int count = 0;
    bool full = layoutWrap(text, (int)strlen(text), g, out, LAYOUT_MAX_CHARS, 128, count);
    if (line0) *line0 = layoutLineCount(out, count, 0);
    if (line1) *line1 = layoutLineCount(out, count, 1);
    return full;
}

static void testBasicWrap() {
    printf("기본 줄바꿈\n");
    check("\"TWENTY SEVEN\" 2줄",  render("TWENTY SEVEN")       == "TWENTY / SEVEN");
    // [수정할 사항 1] 공백이 있으면 항상 2줄. 9자로 1줄에 들어갔던 표현도 나눈다.
    check("\"FORTY ONE\" 2줄",     render("FORTY ONE")          == "FORTY / ONE");
    check("\"FORTY FIVE\" 2줄",    render("FORTY FIVE")         == "FORTY / FIVE");
    check("\"FIFTY NINE\" 2줄",    render("FIFTY NINE")         == "FIFTY / NINE");
    check("\"TWENTY 3\" 2줄",      render("TWENTY 3")           == "TWENTY / 3");
    check("\"SEVEN\" 1줄",          render("SEVEN")              == "SEVEN");
    check("\"SEVENTEEN\" 1줄",      render("SEVENTEEN")          == "SEVENTEEN");
    check("\"O'CLOCK\" 1줄",        render("O'CLOCK")            == "O'CLOCK");
    check("\"TWENTY THREE\" 2줄",  render("TWENTY THREE")       == "TWENTY / THREE");
    check("\"TWELVE THIRTY\" 2줄",  render("TWELVE THIRTY")      == "TWELVE / THIRTY");
    check("\"THIRTY NINE\" 2줄",    render("THIRTY NINE")        == "THIRTY / NINE"); // 10자 > 9
    check("\"PM\" 1줄",             render("PM")                 == "PM");
    check("\"AM\" 1줄",             render("AM")                 == "AM");
    check("\"WEDNESDAY\" 1줄",      render("WEDNESDAY")          == "WEDNESDAY");
    check("빈 문자열",              render("")                   == "(empty)");
}

static void testLineCapacity() {
    printf("줄 용량\n");
    int l0, l1;
    // 6 + 5 = 11글자 → 2줄
    withinTwoLines("TWENTY SEVEN", &l0, &l1);
    check("TWENTY SEVEN: 6/5", l0 == 6 && l1 == 5);
    // 6 + 4 = 10글자 → 2줄
    withinTwoLines("THIRTY NINE", &l0, &l1);
    check("THIRTY NINE: 6/4", l0 == 6 && l1 == 4);
    // 9자 → 이제서야 2줄 (공백 규칙이 길이보다 우선한다)
    withinTwoLines("FORTY FIVE", &l0, &l1);
    check("FORTY FIVE: 5/4 2줄", l0 == 5 && l1 == 4);
    // 9자 단독
    withinTwoLines("SEVENTEEN", &l0, &l1);
    check("SEVENTEEN: 줄1 9자", l0 == 9 && l1 == 0);
    // 정확히 9자인 2어절
    withinTwoLines("TWENTY 3", &l0, &l1);   // 6 + 1 = 7
    check("TWENTY 3: 6/1 2줄", l0 == 6 && l1 == 1);
}

/**
 * [수정할 사항 2] 1줄일 때는 세로 중앙(y=16), 2줄일 때는 위부터(0, 32).
 * LayoutChar.y로 배정되며, 호출부는 line * LINE_HEIGHT를 직접 계산하지 않는다.
 */
static void testVerticalCentering() {
    printf("세로 중앙 정렬\n");
    const CellGeometry& g = *geometryForSize(64);
    LayoutChar out[LAYOUT_MAX_CHARS];
    int count = 0;

    // 1줄: 세로 중앙 = (64 - 32) / 2 = 16
    layoutWrap("SEVEN", 5, g, out, LAYOUT_MAX_CHARS, 128, count);
    check("1줄 SEVEN 전부 y=16", count == 5 && out[0].y == 16 && out[4].y == 16);

    // 9자 1줄도 동일 (9자라 가로 중앙만 검사)
    count = 0;
    layoutWrap("SEVENTEEN", 9, g, out, LAYOUT_MAX_CHARS, 128, count);
    check("1줄 SEVENTEEN y=16", count == 9 && out[0].y == 16 && out[8].y == 16);

    // 공백 없는 1줄 (O'CLOCK)
    count = 0;
    layoutWrap("O'CLOCK", 7, g, out, LAYOUT_MAX_CHARS, 128, count);
    check("1줄 O'CLOCK y=16", count == 7 && out[0].y == 16 && out[6].y == 16);

    // 2줄: (64 - 64) / 2 = 0 → 줄0 y=0, 줄1 y=32
    count = 0;
    layoutWrap("TWENTY SEVEN", 12, g, out, LAYOUT_MAX_CHARS, 128, count);
    check("2줄 TWENTY SEVEN y=0", count == 11 && out[0].y == 0 && out[5].y == 0);
    check("2줄 TWENTY SEVEN y=32", out[6].y == 32 && out[10].y == 32);

    // 날짜 + 요일 ("2/10 FRIDAY") — [수정할 사항 3]이 새로 만드는 문자열
    count = 0;
    layoutWrap("2/10 FRIDAY", 11, g, out, LAYOUT_MAX_CHARS, 128, count);
    check("\"2/10 FRIDAY\" 2줄", count == 10);
    check("\"2/10\" y=0", out[0].y == 0 && out[3].y == 0);
    check("FRIDAY y=32", out[4].y == 32 && out[9].y == 32);

    // 숫자 모드 날짜 + 요일숫자 (공백은 그리지 않으므로 4 + 1 = 5)
    count = 0;
    layoutWrap("2/10 5", 6, g, out, LAYOUT_MAX_CHARS, 128, count);
    check("\"2/10 5\" 2줄", count == 5 && out[3].y == 0 && out[4].y == 32);
}

/** 모든 영어 시간 표현이 2줄 × 9자 안에 수용되는가 — english_time와 결합한 전수 검증 */
static void testExhaustiveWithRealTexts() {
    printf("전수 검증: 실제 변환 결과가 2줄에 수용되는가\n");
    char b[ENG_BUF_SIZE];
    int violations = 0, truncated = 0;
    int worstTotal = 0, worstL0 = 0, worstL1 = 0;
    char worstText[64] = "";

    for (int h = 0; h < 24; h++) {
        for (int is24 = 0; is24 < 2; is24++) {
            if (!engtime::hourToWords(h, is24 != 0, b, sizeof(b))) continue;
            int l0, l1;
            bool full = withinTwoLines(b, &l0, &l1);
            if (!full) { truncated++; printf("  FAIL 잘림: 시%d \"%s\"\n", h, b); }
            if (l0 > 9 || l1 > 9) {
                violations++;
                printf("  FAIL 용량초과: 시%d \"%s\" → %d/%d\n", h, b, l0, l1);
            }
            if (l0 + l1 > worstTotal) {
                worstTotal = l0 + l1; worstL0 = l0; worstL1 = l1;
                snprintf(worstText, sizeof(worstText), "%s", b);
            }
        }
    }
    for (int m = 0; m < 60; m++) {
        if (engtime::minuteToWords(m, b, sizeof(b))) {
            int l0, l1;
            bool full = withinTwoLines(b, &l0, &l1);
            if (!full) { truncated++; printf("  FAIL 잘림: 분%d \"%s\"\n", m, b); }
            if (l0 > 9 || l1 > 9) { violations++; printf("  FAIL 용량초과: 분%d \"%s\" → %d/%d\n", m, b, l0, l1); }
            if (l0 + l1 > worstTotal) {
                worstTotal = l0 + l1; worstL0 = l0; worstL1 = l1;
                snprintf(worstText, sizeof(worstText), "%s", b);
            }
        }
        if (engtime::secondToWords(m, b, sizeof(b))) {
            int l0, l1;
            if (!withinTwoLines(b, &l0, &l1)) truncated++;
            if (l0 > 9 || l1 > 9) violations++;
        }
    }
    for (int d = 0; d < 7; d++) {
        if (engtime::dayName(d, b, sizeof(b))) {
            int l0, l1;
            if (!withinTwoLines(b, &l0, &l1)) { truncated++; printf("  FAIL 잘림: 요일%d \"%s\"\n", d, b); }
            if (l0 > 9 || l1 > 9) violations++;
        }
    }
    // [수정할 사항 3] 날짜 + 요일 — 24H 첫 화면이 되는 모든 조합 (월 12 × 일 31 × 요일 7 × 2가지 순서)
    for (int mon = 1; mon <= 12; mon++) {
        for (int day = 1; day <= 31; day++) {
            for (int order = 0; order < 2; order++) {
                for (int d = 0; d < 7; d++) {
                    char date[ENG_BUF_SIZE], dn[ENG_BUF_SIZE];
                    if (!engtime::dateString(mon, day, order != 0, date, sizeof(date))) continue;
                    if (!engtime::dayName(d, dn, sizeof(dn))) continue;
                    std::string combo = std::string(date) + " " + dn;
                    int l0, l1;
                    if (!withinTwoLines(combo.c_str(), &l0, &l1)) { truncated++; printf("  FAIL 잘림: \"%s\"\n", combo.c_str()); }
                    if (l0 > 9 || l1 > 9) { violations++; printf("  FAIL 용량초과: \"%s\"\n", combo.c_str()); }
                }
            }
        }
    }
    printf("  최악 케이스: \"%s\" → %d/%d (합 %d)\n", worstText, worstL0, worstL1, worstTotal);
    check("용량 초과 0건", violations == 0);
    check("잘림 0건", truncated == 0);
}

static void testCoordinates() {
    printf("좌표 배정\n");
    const CellGeometry& g = *geometryForSize(64);
    LayoutChar out[LAYOUT_MAX_CHARS];
    int count = 0;

    // 1줄: "SEVEN" (5자) → (128 - 5*14)/2 = 29
    layoutWrap("SEVEN", 5, g, out, LAYOUT_MAX_CHARS, 128, count);
    check("SEVEN count 5", count == 5);
    check("SEVEN 첫 x == 29 ((128-70)/2)", out[0].x == 29);
    check("SEVEN 마지막 x == 85", out[4].x == 29 + 4 * 14);

    // 2줄: "TWENTY SEVEN" → 줄1 6자 x=7, 줄2 4자 x=36
    count = 0;
    layoutWrap("TWENTY SEVEN", 12, g, out, LAYOUT_MAX_CHARS, 128, count);
    check("TWENTY SEVEN count 11", count == 11);
    check("줄1 첫 x == 22 ((128-84)/2)", out[0].x == 22);
    check("줄1 마지막 x == 92", out[5].x == 22 + 5 * 14);
    check("줄2 첫 x == 29 ((128-70)/2)", out[6].x == 29);
    check("줄2 마지막 x == 85", out[10].x == 29 + 4 * 14);

    // 화면 밖으로 나가지 않는가 (9자 한도)
    count = 0;
    layoutWrap("SEVENTEEN", 9, g, out, LAYOUT_MAX_CHARS, 128, count);
    check("SEVENTEEN 첫 x == 1 ((128-126)/2)", out[0].x == 1);
    check("SEVENTEEN 마지막 x + 14 == 127 (화면 내)", out[8].x + 14 == 127);
}

static void testEdgeCases() {
    printf("경계/실패 경로\n");
    const CellGeometry& g = *geometryForSize(64);
    LayoutChar out[LAYOUT_MAX_CHARS];
    int count = 0;

    check("빈 문자열 → count 0, 성공",
          layoutWrap("", 0, g, out, LAYOUT_MAX_CHARS, 128, count) && count == 0);
    check("NULL 문자열 → 성공, count 0",
          layoutWrap(NULL, 0, g, out, LAYOUT_MAX_CHARS, 128, count) && count == 0);
    check("out=NULL → false",
          !layoutWrap("X", 1, g, NULL, 10, 128, count));
    check("outCapacity 0 → false",
          !layoutWrap("X", 1, g, out, 0, 128, count));

    // 용량 초과: 18자 배열에 30자 요청 → 잘림으로 false
    const char* long1 = "TWENTY THIRTY SEVEN";
    check("18자 초과 → false",
          !layoutWrap(long1, (int)strlen(long1), g, out, LAYOUT_MAX_CHARS, 128, count));
    check("잘려도 count는 배열 범위 이내", count <= LAYOUT_MAX_CHARS);

    // 공백만 있는 문자열
    count = 0;
    check("공백만 → count 0",
          layoutWrap("   ", 3, g, out, LAYOUT_MAX_CHARS, 128, count) && count == 0);

    // 앞뒤 공백
    count = 0;
    layoutWrap("  SEVEN  ", 9, g, out, LAYOUT_MAX_CHARS, 128, count);
    check("앞뒤 공백 무시", count == 5);

    // 3단어 (17자) → 2줄(18자)에는 들어가지만 어절 경계가 안 맞아 SEVEN이 잘린다
    count = 0;
    check("\"TWENTY THIRTY SEVEN\" 잘림 보고",
          !layoutWrap("TWENTY THIRTY SEVEN", 19, g, out, LAYOUT_MAX_CHARS, 128, count));
    // 어절은 절대 쪼개지지 않는다 — 쪼개면 "THIRTYSEV" 같은 비가독 문자열이 된다
    check("잘려도 각 줄이 완전한 어절", render("TWENTY THIRTY SEVEN") == "TWENTY / THIRTY");

    // 9자를 넘는 단어가 들어오면 분할하지 않고 잘림으로 보고한다.
    // 전부 또는 아무것도 규칙이므로 일부만 그려지지 않는다 (0글자).
    count = 0;
    check("10자 단어 → 분할하지 않음",
          !layoutWrap("ABCDEFGHIJ", 10, g, out, LAYOUT_MAX_CHARS, 128, count));
    check("10자 단어는 일부만 그리지 않음", count == 0);
    // 9자 단어는 정상 표시
    count = 0;
    check("9자 단어는 표시",
          layoutWrap("ABCDEFGHI", 9, g, out, LAYOUT_MAX_CHARS, 128, count) && count == 9);
}

/**
 * [수정할 사항 1 + 버그 재발 방지] singleLine 모드와 어절 사이 공백 셀.
 * 공백은 같은 줄에 이어지는 어절 사이에서만 빈 셀(text=" ") 한 칸이 된다.
 * 줄 경계의 공백은 버려진다 — 아니면 줄 폭이 한 칸 늘어나 중앙이 어긋난다.
 */
static void testSingleLineAndSpaceCell() {
    printf("singleLine / 공백 셀 [수정할 사항 1]\n");
    const CellGeometry& g = *geometryForSize(64);
    LayoutChar out[LAYOUT_MAX_CHARS];
    int count = 0;

    // "02 H": 어절 2개지만 singleLine → 1줄. 셀은 '0','2',' '(빈 셀),'H' 4개.
    count = 0;
    layoutWrap("02 H", 4, g, out, LAYOUT_MAX_CHARS, 128, count, true);
    check("\"02 H\" 1줄 4셀", count == 4);
    check("\"02 H\" 전부 줄 0", out[0].line == 0 && out[3].line == 0);
    check("\"02 H\" 세로 중앙 y=16", out[0].y == 16 && out[3].y == 16);
    check("셀 3은 공백(text=\" \", len 1)", out[2].len == 1 && out[2].text[0] == ' ');
    // 가로 중앙은 공백 셀 포함 폭(4셀=56px) 기준: (128-56)/2 = 36
    check("\"02 H\" 첫 x == 36 (공백 셀 포함 중앙)", out[0].x == 36);
    check("H는 공백 셀 하나를 사이에 둔다 (x 간격 2×14)", out[3].x - out[1].x == 2 * 14);

    // 기본(false)은 여전히 어절 2개 → 2줄 ("02" / "H"), 경계 공백 셀 없음
    count = 0;
    layoutWrap("02 H", 4, g, out, LAYOUT_MAX_CHARS, 128, count);
    check("\"02 H\" 기본값은 2줄 3셀 (공백 셀 없음)", count == 3);
    check("기본값 '02' y=0 / 'H' y=32", out[0].y == 0 && out[2].y == 32);

    // singleLine이라도 어절이 안 들어가면 다음 줄로 내려간다 — 경계 공백 셀은 없다
    count = 0;
    layoutWrap("TWENTY SEVEN", 12, g, out, LAYOUT_MAX_CHARS, 128, count, true);
    check("\"TWENTY SEVEN\" singleLine → 11셀 2줄 (경계 공백 없음)", count == 11);
    check("줄0 6셀 / 줄1 5셀", layoutLineCount(out, count, 0) == 6 && layoutLineCount(out, count, 1) == 5);
    check("줄1 SEVEN 중앙 x == 29 (공백 셀이 폭을 늘리지 않는다)", out[6].x == 29);

    // singleLine에서 현재 줄에 못 들면 빈 다음 줄로 내려간다 (잘리지 않는다)
    count = 0;
    check("\"TWELVE THREE\" singleLine → 완전 배치",
          layoutWrap("TWELVE THREE", 12, g, out, LAYOUT_MAX_CHARS, 128, count, true) && count == 11);

    // 2줄마저 넘치는 어절은 버려진다 (무한 확장 아님)
    count = 0;
    check("\"TWELVE THREE FOUR\" singleLine → 잘림 보고",
          !layoutWrap("TWELVE THREE FOUR", 17, g, out, LAYOUT_MAX_CHARS, 128, count, true));

    // 3어절이 한 줄에 이어지면 어절 사이마다 공백 셀이 들어간다
    count = 0;
    layoutWrap("A B C", 5, g, out, LAYOUT_MAX_CHARS, 128, count, true);
    check("\"A B C\" 1줄 5셀", count == 5);
    check("셀 1·3이 공백", out[1].text[0] == ' ' && out[3].text[0] == ' ');
    check("\"A B C\" 등간격 x = 29,43,57,71,85",
          out[0].x == 29 && out[1].x == 43 && out[2].x == 57 && out[3].x == 71 && out[4].x == 85);

    // 앞 공백은 세지 않는다 (" 02 H" → "02 H"와 동일)
    count = 0;
    layoutWrap(" 02 H", 5, g, out, LAYOUT_MAX_CHARS, 128, count, true);
    check("선두 공백 무시", count == 4 && out[0].text[0] == '0');
}

/**
 * [간격 확장 §6.16] linePitch() — 글자 수가 적을수록 피치를 넓히되,
 * 잉크 폭에서 멈추고, 9자·작은 폰트에서는 기존 값(14)을 그대로 돌려준다.
 */
static void testLinePitch() {
    printf("피치 계산 (linePitch) [§6.16]\n");
    const CellGeometry& g = *geometryForSize(64);   // glyphW 14, maxPerLine 9

    // 작은 폰트(잉크 11 ≈ 12px) — 요구사항 "폰트 크기가 작다면 늘리지 않아도"
    for (int n = 1; n <= 9; n++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "작은 폰트(ink 11) %d자 → 14", n);
        check(msg, linePitch(n, g, 128, 11, 11) == 14);
    }
    check("잉크 0(캐시 없음) 9자 → 14", linePitch(9, g, 128, 0, 0) == 14);
    check("잉크 0(캐시 없음) 5자 → 14", linePitch(5, g, 128, 0, 0) == 14);
    check("잉크 = glyphW 경계 5자 → 14 (확장 안 함)", linePitch(5, g, 128, 14, 14) == 14);

    // [사용자 지정 사다리] n 글자 줄의 총 폭을 (n+1) 글자분으로 맞춘다.
    //   이 표는 **계산해서** 비교한다 — 하드코딩하면 산식과 표가 따로 놀게 된다.
    //   (9→126/9=14 · 8→126/8=15 · 7→112/7=16 · 6→98/6=16 · 5→84/5=16
    //    4→70/4=17 · 3→56/3=18 · 2→42/2=21)
    for (int n = 1; n <= 9; n++) {
        const int total = (n + 1 > 9) ? 9 * 14 : (n + 1) * 14;
        const int want = (n <= 1) ? 14 : total / n;
        char msg[64];
        snprintf(msg, sizeof(msg), "사다리 %d자 → %d (총폭 %d)", n, want, total);
        // 잉크를 want로 주면 하한(=사다리)과 화면 상한이 모두 걸리지 않아
        // 순수 사다리만 나온다. 0을 주면 inkCap가 폰트 최대로 잡혀 7·8자에서 잘린다.
        check(msg, linePitch(n, g, 128, want, want, want) == want);
    }
    // 9자는 **어떤 잉크에서도** 14여야 한다 (요구사항: 9글자는 그대로).
    {
        bool all14 = true;
        for (int ink = 0; ink <= 43; ink++)
            if (linePitch(9, g, 128, ink, ink, ink) != 14) all14 = false;
        check("9자는 잉크 0~43 전부에서 14 (변경 없음)", all14);
    }
    // 잉크 하단은 **올리기만** 한다 — 이게 "AM/ONE/FORTY 너무 붙어"의 회귀 지점이다.
    check("하단이 사다리보다 크면 올린다 (5자·큰 잉크 → 화면 상한 21)",
          linePitch(5, g, 128, 43, 43, 43) == 21);
    check("사다리는 잉크·하단 없이도 성립한다 (5자 ink 0 → 16)",
          linePitch(5, g, 128, 0, 43, 0) == 16);
    // 글자 수가 줄수록 피치는 줄지 않는다 (요구사항: 조금씩 늘려준다)
    bool monotone = true;
    for (int n = 2; n <= 9; n++) {
        if (linePitch(n, g, 128, 43, 43) < linePitch(n + 1, g, 128, 43, 43)) monotone = false;
    }
    check("ink 43 · 글자 수가 줄수록 피치가 줄지 않는다", monotone);
    check("ink 43 · 9자 → 14 (기존과 동일)", linePitch(9, g, 128, 43, 43) == 14);
    check("ink 43 · 2자 → 화면 상한 43 (사다리 21을 하단이 올림)", linePitch(2, g, 128, 43, 43) == 43);

    // 잉크 하단이 사다리를 **올리는** 구간을 고정한다.
    //   하단이 사다리보다 커야 올림이 일어난다.
    //   ink 30 폰트 · 5자: min(25, 30, 122/4=30) = 25 → 14 + 11·3/5 = 20.
    // [§6.16c] 완가는 100% 벌림(캡·화면 상한 적용 후 spread)에서 **겹침 없는 하단**으로
    //   되돌린다. ink 30 · 5자: 몫 25 → 잉크 캡 30에 걸리지 않음, 화면 상한 24 → spread 24.
    //   하단을 모르면(lineFloor=0) 줄 최대 30을 쓰지만 그것이 spread보다 커서 상한으로 깎여 24.
    // [현재 규칙] 사다리 → (하단으로 올림) → (화면 상한으로 내림).
    //   잉크가 사다리보다 좁으면 간격은 사다리 그대로다 — 잉크는 벌리는 쪽이 아니다.
    //   잉크가 사다리보다 **넓으면** 하한이 올린다. 이게 벌림의 정체다 —
    //   잉크가 좁을 때는 사다리를 그대로 쓴다(아래 두 단정이 그것을 보인다).
    check("ink 30 · 5자 → 하단 30이 사다리를 올려 24",
          linePitch(5, g, 128, 30, 30, 30) == 24);
    check("ink 15 · 5자 → 사다리 16 (잉크가 좁으면 사다리 그대로)",
          linePitch(5, g, 128, 15, 15, 15) == 16);
    // [하한 = 최소 필요한 값이라 **올린다**] — "너무 붙어" 회귀의 핵심.
    //   48px "TWO"(줄 잉크 40, 인접쌍 하단 36): 사다리 18 → 하단 36 → 화면 상한 44 → 36.
    check("하단이 사다리보다 크면 올린다 — ink 40 · 3자 하단 36 → 36",
          linePitch(3, g, 128, 40, 43, 36) == 36);
    //   하단이 사다리보다 작으면 사다리를 따른다 (되돌리지 않는다).
    check("하단이 사다리보다 작으면 사다리 유지 — ink 15 · 5자 → 16",
          linePitch(5, g, 128, 15, 15, 15) == 16);
    //   하단을 모르면(0) 줄 잉크를 쓴다. 그것이 안전한 상계라 간격을 벌리지 않는다.
    check("하단 미지정이면 줄 잉크로 대체 — ink 40 · 3자 → 40",
          linePitch(3, g, 128, 40, 43, 0) == 40);
    //   사다리는 어떤 경우에도 셀 폭(14)을 깎지 않는다.
    check("셀 폭 아래로 내리지 않는다",
          linePitch(1, g, 128, 43, 43) == 14 && linePitch(9, g, 128, 43, 43) == 14 &&
          linePitch(5, g, 128, 11, 11) == 14);

    // [§6.16b] **줄 잉크가 폰트 최대보다 좁을 때** 줄마다 다른 값이 나와야 한다.
    //   48px 폰트(fontInk 43)에서 "SEVEN"은 줄 잉크 24, "TWO"는 32다.
    check("[§6.16b] 줄 잉크로 하한 — fontInk 43 · lineInk 28 · 5자 → 25 (화면 상한)",
          linePitch(5, g, 128, 28, 43, 28) == 25);
    check("[§6.16b] 같은 줄 수에서 줄 잉크가 크면 하한도 커진다",
          linePitch(5, g, 128, 20, 43, 20) < linePitch(5, g, 128, 32, 43, 32));
    //   줄 잉크·하단을 둘 다 모르면(0) **폰트 최대로 대체하지 않는다** —
    //   하한을 지어내면 "W가 넓은 폰트" 한 장에 모든 짧은 줄이 43px까지 밀린다.
    check("[회귀] 잉크를 모르면 하단을 만들지 않는다 (5자 → 사다리 16)",
          linePitch(5, g, 128, 0, 43, 0) == 16);
    //   작은 폰트 판정은 줄 잉크가 아니라 **폰트 최대**로 한다 —
    //   줄 잉크만 좁아도 확장하면 짧은 줄만 흩어져 보인다.
    check("[§6.16b] 작은 폰트는 줄 잉크가 넓어도 확장하지 않는다",
          linePitch(3, g, 128, 40, 12) == 14);

    // 글자 수가 줄수록 피치가 줄지 않는다 (요구사항: 조금씩 늘려준다).
    bool monotoneByInk = true;
    for (int ink = 15; ink <= 48; ink++) {
        for (int n = 2; n <= 9; n++) {
            if (linePitch(n, g, 128, ink, ink) < linePitch(n + 1, g, 128, ink, ink)) monotoneByInk = false;
        }
    }
    check("글자 수가 줄수록 피치가 줄지 않는다 (단조)", monotoneByInk);

    // 줄 폭이 화면(128)을 넘지 않는다 — 단, **배치가 보장되는 범위에서만**.
    //   9자는 요구대로 14로 고정이다. 그러면 잉크 14를 넘는 폰트는 9자가 물리적으로
    //   들어가지 않는다(126px 화면에 9×14=126이 딱이라 여유가 0). 이는 사다리를
    //   바꾸기 전부터 같았고, 고정 자체가 사용자 요구다 — 그래서 여기서 제외한다.
    //   작은 폰트(잉크 ≤ 14)는 모든 글자 수에서 반드시 들어간다.
    for (int ink = 0; ink <= 14; ink++) {
        for (int n = 1; n <= 9; n++) {
            // 진짜 경계는 **잉크 블록 폭** (n−1)·pitch + ink 이다.
            //   pitch·n 으로 재면 셀 폭만큼을 빼먹어 128px 화면을 넘긴 걸 놓친다.
            const int pitch = linePitch(n, g, 128, ink, ink);
            if ((n - 1) * pitch + ink > 128) {
                char msg[64];
                snprintf(msg, sizeof(msg), "ink %d · %d자 줄 폭 ≤ 128", ink, n);
                check(msg, false);
            }
        }
    }
    check("ink 0~14 · 모든 글자 수에서 줄 폭 ≤ 128", true);

    // [§6.16 — 브라우저 픽셀 검증에서 추가한 상한의 계약]
    // 실제 화면에 그려지는 것은 **잉크 블록** `(n−1)·pitch + ink` 이다.
    // 이 값이 128을 넘으면 화면 밖에서 글자가 잘린다(잘림은 겹침보다 나쁘다).
    // 예외는 pitch가 셀 폭(14)에 묶인 경우뿐이다 — 9자는 사용자 요구로 간격을
    // 고정했고, 거기서 ink가 크면 기하학적으로 화면에 들어가지 않는다.
    bool inkFits = true;
    for (int ink = 0; ink <= 48; ink++) {
        for (int n = 2; n <= 9; n++) {
            const int p = linePitch(n, g, 128, ink, ink);
            if (p == 14) continue;                       // 고정 간격 — 요구사항이 우선
            if ((n - 1) * p + ink > 128) inkFits = false;
        }
    }
    check("ink 0~48 · 확장된 줄의 잉크 블록 ≤ 128 (잘림 없음)", inkFits);

    // 상한이 실제로 발동하는 지점: 6자·ink 24 → min(126/6=21, 24, 104/5=20) = 20
    check("화면 적합 상한 발동 — ink 24 · 6자 → 20", linePitch(6, g, 128, 24, 24) == 20);
    // 5자·ink 24: 사다리 16을 하단 24가 올리고, 화면 상한 26에 걸리지 않아 24가 된다.
    // 8자·ink 24는 상한(104/7=14)에 걸려 고정 간격으로 내려온다 (요구사항과 일치)
    check("ink 24 · 8자 → 14 (9자 간격까지 = 상한에 걸림)", linePitch(8, g, 128, 24, 24) == 14);

    // 셀 수 0/음수는 방어 — 예외 없이 1자처럼 취급
    check("0셀 → 14",  linePitch(0, g, 128, 43, 43) == 14);
    check("음수 셀 → 14", linePitch(-3, g, 128, 43, 43) == 14);
}

/**
 * [간격 확장 §6.16] lineStartX() — 확장이 없으면 기존 셀 중앙 정렬과 **한 픽셀도**
 * 같아야 하고, 확장됐으면 잉크 블록이 화면 중앙에 와야 한다.
 */
static void testLineStartX() {
    printf("줄 시작 x (lineStartX) [§6.16]\n");
    const CellGeometry& g = *geometryForSize(64);

    // 기존 동작 보존: pitch == glyphW 경로
    for (int n = 1; n <= 9; n++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "%d자 셀 중앙 정렬 = (128-%d)/2", n, n * 14);
        check(msg, lineStartX(n, 14, g, 128, 43) == (128 - n * 14) / 2);
    }

    // 확장 시 잉크 블록 중앙 정렬 — 셀 폭 기준으로 하면 (pitch-14)/2 만큼 쏠린다
    // 2자 · ink 24 · pitch 24 → 잉크 블록 [40, 88], 화면 중앙 64
    check("2자 pitch24 ink24 → 45 (잉크 블록 [40,88] 중앙)", lineStartX(2, 24, g, 128, 24) == 45);
    // 5자 · ink 24 · pitch 24 → 잉크 블록 [4, 124]
    check("5자 pitch24 ink24 → 9 (잉크 블록 [4,124] 중앙)", lineStartX(5, 24, g, 128, 24) == 9);
    // 3자 · ink 43 · pitch 42 → 잉크 블록 [-0.5, 126.5]
    check("3자 pitch42 ink43 → 14", lineStartX(3, 42, g, 128, 43) == 14);
    // 셀 중앙 정렬 would've been 1 — 13px 차이. 이 회귀를 못 잡으면 "AM"이 왼쪽에 붙는다.
    check("3자 ink43는 셀 중앙(1)보다 오른쪽 (5px 쏠림 회귀)", lineStartX(3, 42, g, 128, 43) != 1);
}

/**
 * [간격 확장 §6.16] layoutWrap()에 잉크 폭을 넘겼을 때의 실제 좌표.
 * 기하(14px 셀)와 126px 목표 폭에서 나온 값이라 손으로 계산해 확인했다.
 */
static void testExpandedCoordinates() {
    printf("확장 후 좌표 배정 [§6.16]\n");
    const CellGeometry& g = *geometryForSize(64);
    LayoutChar out[LAYOUT_MAX_CHARS];
    int count = 0;

    // --- 회귀 0: 9자·작은 폰트는 기존 좌표 그대로 ---
    count = 0;
    layoutWrap("SEVENTEEN", 9, g, out, LAYOUT_MAX_CHARS, 128, count, false, 0);
    check("SEVENTEEN(ink 0) 첫 x == 1", count == 9 && out[0].x == 1);
    check("SEVENTEEN(ink 0) 마지막 x == 113", out[8].x == 113);

    count = 0;
    layoutWrap("SEVENTEEN", 9, g, out, LAYOUT_MAX_CHARS, 128, count, false, 43);
    check("SEVENTEEN(ink 43) 첫 x == 1 (큰 폰트도 9자는 그대로)", count == 9 && out[0].x == 1);
    check("SEVENTEEN(ink 43) 마지막 x == 113", out[8].x == 113);
    check("SEVENTEEN(ink 43) 간격 14 유지", out[1].x - out[0].x == 14);

    count = 0;
    layoutWrap("SEVEN", 5, g, out, LAYOUT_MAX_CHARS, 128, count, false, 11);
    check("SEVEN(ink 11 작은 폰트) 첫 x == 29 (기존)", count == 5 && out[0].x == 29);
    check("SEVEN(ink 11 작은 폰트) 간격 14 유지", out[4].x - out[0].x == 4 * 14);

    // --- 확장: ink 24 (≈26px) ---
    count = 0;
    layoutWrap("SEVEN", 5, g, out, LAYOUT_MAX_CHARS, 128, count, false, 24);
    check("SEVEN(ink 24) 5셀", count == 5);
    // 5자 → 사다리 16을 하단 24가 올려 pitch 24 유지.
    // 잉크 블록 4×24+24 = 120 → (128-120)/2 + 5 = 9
    check("SEVEN(ink 24) 첫 x == 9 (사다리 16을 하단 24가 올림)", out[0].x == 9);
    check("SEVEN(ink 24) 간격 24 (겹침 없는 최솟값)", out[1].x - out[0].x == 24);
    check("SEVEN(ink 24) 마지막 x == 105", out[4].x == 105);
    check("SEVEN(ink 24) 잉크 블록 120 ≤ 126", out[4].x + 24 - 9 <= 126);

    count = 0;
    layoutWrap("TWENTY SEVEN", 12, g, out, LAYOUT_MAX_CHARS, 128, count, false, 24);
    check("TWENTY SEVEN(ink 24) 11셀", count == 11);
    check("줄0(TWENTY 6자) 첫 x == 7",  out[0].x == 7);
    check("줄0 간격 20 (화면 적합 상한)", out[1].x - out[0].x == 20);
    check("줄1(SEVEN 5자) 첫 x == 9",   out[6].x == 9);
    check("줄1 간격 24 (하단이 올림)", out[7].x - out[6].x == 24);
    check("두 줄이 각각 중앙 정렬",      out[0].x != out[6].x);

    count = 0;
    layoutWrap("TWELVE", 6, g, out, LAYOUT_MAX_CHARS, 128, count, false, 24);
    check("TWELVE(ink 24) 6자 첫 x == 7", count == 6 && out[0].x == 7);
    check("TWELVE(ink 24) 간격 20 (화면 적합 상한)", out[1].x - out[0].x == 20);

    count = 0;
    layoutWrap("AM", 2, g, out, LAYOUT_MAX_CHARS, 128, count, false, 24);
    // 2자 → 사다리 21을 하단 24가 올려 24. 잉크 블록 24+24=48 → 40+5=45
    check("AM(ink 24) 2자 첫 x == 45", count == 2 && out[0].x == 45);
    check("AM(ink 24) 간격 24 (126px에 흩어지지 않는다)", out[1].x - out[0].x == 24);

    // --- 최대 폰트 ink 43 (≈48px) ---
    count = 0;
    layoutWrap("AM", 2, g, out, LAYOUT_MAX_CHARS, 128, count, false, 43);
    // 2자 → 잉크 43 상한에 막혀 pitch 43. 잉크 블록 86 → (128-86)/2 + 14 = 35
    check("AM(ink 43) 2자 첫 x == 35", count == 2 && out[0].x == 35);
    check("AM(ink 43) 간격 43 = 화면 상한에서 멈춤",
          out[1].x - out[0].x == 43);

    // --- singleLine(숫자 모드 "02 H")도 같은 규칙 ---
    count = 0;
    layoutWrap("02 H", 4, g, out, LAYOUT_MAX_CHARS, 128, count, true, 24);
    check("\"02 H\"(ink 24) 4셀", count == 4);
    // 4셀 → pitch 24(하단이 올림). 잉크 블록 3×24+24=96 → 16+5=21
    check("\"02 H\"(ink 24) 첫 x == 21 (잉크 블록 96px 중앙)", out[0].x == 21);
    check("\"02 H\"(ink 24) 공백 셀이 간격을 유지", out[2].x - out[1].x == 24);
    check("\"02 H\"(ink 24) H x == 21+3*24", out[3].x == 21 + 3 * 24);
    check("\"02 H\"(ink 24) 줄 폭 96 ≤ 126", out[3].x + 24 - 21 <= 126);

    count = 0;
    layoutWrap("02 H", 4, g, out, LAYOUT_MAX_CHARS, 128, count, true, 0);
    check("\"02 H\"(ink 0) 첫 x == 36 (기존)", out[0].x == 36);
}

/**
 * [§6.16b] measureLineInk() — 줄마다 실제 잉크를 재야 상한이 맞아떨어진다.
 * 펌웨어 Renderer::inkOfTrampoline이 하는 일을 테스트 더블로 흉내낸다.
 */
/**
 * 'A'+i 글자의 잉크를 table[i]로 돌려주는 테스트 더블.
 * @note 표는 `'A'` 기준 **오프셋 배열**이다 — 문자열 리터럴을 넘기면
 *       'A'(65) 인덱스로 벗어나므로 반드시 0부터 시작하는 배열이어야 한다.
 */
static uint8_t fakeInkOf(void* ctx, const char* text, uint8_t len) {
    const uint8_t* table = static_cast<const uint8_t*>(ctx);
    return table[text[0] - 'A'];
}

/** A=20, B=30 인 더블 */
static const uint8_t INK_AB[2] = { 20, 30 };

static void testMeasureLineInk() {
    printf("줄별 잉크 측정 (measureLineInk) [§6.16b]\n");

    const uint8_t* ab = INK_AB;   // A=20, B=30
    check("1자 A → 20", measureLineInk("A", 0, 1, fakeInkOf, (void*)ab, 43) == 20);
    check("2자 AB → 30 (최댓값)", measureLineInk("AB", 0, 2, fakeInkOf, (void*)ab, 43) == 30);

    // 조회 수단이 없으면 폰트 최대 그대로 — 기존 동작 유지(§6.16 하위 호환)
    check("inkFn nullptr → fallback 그대로", measureLineInk("AB", 0, 2, nullptr, nullptr, 43) == 43);
    check("글자 수 0 → 0", measureLineInk("AB", 0, 0, fakeInkOf, (void*)ab, 43) == 0);

    // 공백도 **한 셀**이다 — 카운트를 소모하되 잉크 상한엔 기여하지 않는다.
    //   "A B"(4셀): 공백을 세지 않으면 A,B 뒤를 또 읽어 잘못된 최대가 나온다.
    const uint8_t* wide = INK_AB;
    //   "A B"는 4셀 요청이지만 문자열이 3바이트뿐이라 B까지 읽고 끝난다 → 30.
    //   (A만 읽고 멈추면 20이 되지만, 카운트가 아니라 **문자열 끝**이 경계다)
    check("공백 셀은 카운트를 소모하지만 잉크에 기여하지 않음",
          measureLineInk("A B", 0, 4, fakeInkOf, (void*)wide, 43) == 30);
    check("공백 3칸 뒤의 글자도 읽는다 (4셀 = A,' ',' ',B)",
          measureLineInk("A  B", 0, 4, fakeInkOf, (void*)wide, 43) == 30);

    // 조회 실패(모든 값 0)면 fallback — 캐시에 없는 글자에 대비한다.
    const uint8_t zero[2] = { 0, 0 };
    check("전부 조회 실패(0) → fallback", measureLineInk("AB", 0, 2, fakeInkOf, (void*)zero, 43) == 43);

    // start 오프셋 — 2줄 화면에서 줄 1이 실제로 있는 곳부터 읽어야 한다
    check("start 오프셋 반영 — \"A AB\"의 2~4글자 = AB",
          measureLineInk("A AB", 2, 2, fakeInkOf, (void*)ab, 43) == 30);

    // [핵심 계약] 같은 폰트라도 **줄마다** 다른 상한이 나와야 한다.
    //   48px 폰트에서 "SEVEN"(줄 잉크 28)과 "AMW"(줄 잉크 38)이 같은 값으로 묶이면
    //   "SEVEN"이 필요 이상으로 벌어져 사용자가 지적한 "떨어진 느낌"이 돌아온다.
    const CellGeometry& g = *geometryForSize(64);
    const uint8_t table[23] = { 24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,38 };
    LayoutChar laid[LAYOUT_MAX_CHARS];
    int count = 0;
    layoutWrap("SEVEN", 5, g, laid, LAYOUT_MAX_CHARS, 128, count, false, 43,
               fakeInkOf, (void*)table);
    const int sevenPitch = laid[1].x - laid[0].x;
    check("[핵심] SEVEN(줄 잉크 24) 피치 == 24 (하단=24라 완가 변화 없음)",
          count == 5 && sevenPitch == 24);
    count = 0;
    layoutWrap("AMW", 3, g, laid, LAYOUT_MAX_CHARS, 128, count, false, 43,
               fakeInkOf, (void*)table);
    const int amwPitch = laid[1].x - laid[0].x;
    //   표에서 M=24, W=38 → 인접 평균 (24+38)/2=31 → 하단 31.
    //   사다리 18 → 하단 31 → 화면 상한 (128−38)/2=45 → 최종 31.
    check("[핵심] 줄 잉크 38(W 포함)는 24보다 피치가 넓다",
          amwPitch == 31 && sevenPitch < amwPitch);
    // inkFn 없이 같은 두 줄 → 줄 잉크를 몰라 사다리가 그대로 쓰인다(5자 16, 3자 18).
    count = 0;
    layoutWrap("SEVEN", 5, g, laid, LAYOUT_MAX_CHARS, 128, count, false, 43);
    const int sevenNoFn = laid[1].x - laid[0].x;
    count = 0;
    layoutWrap("AMW", 3, g, laid, LAYOUT_MAX_CHARS, 128, count, false, 43);
    check("[하위 호환] inkFn 없으면 줄이 아니라 폰트 최대로 묶인다",
          sevenNoFn == 21 && laid[1].x - laid[0].x == 42);
}

/**
 * [§6.16c] measureLineFloor() — 겹침 없는 최소 피치.
 * 잉크가 래스터 안에서 중앙 정렬되므로 하한은 "이웃 두 글자의 평균"이다.
 * 줄 최대 잉크를 하한으로 쓰면 과해져 "SEVEN"이 한 번도 줄지 않는다 — 이게
 * 앞선 두 번의 완화가 무효였던 근본 원인이다.
 */
static void testMeasureLineFloor() {
    printf("겹침 없는 하단 측정 (measureLineFloor) [§6.16c]\n");
    const uint8_t* ab = INK_AB;   // A=20, B=30

    // 인접 평균: (20+30)/2 = 25 → 올림 25
    check("AB → 25 (인접 평균 (20+30)/2)", measureLineFloor("AB", 0, 2, fakeInkOf, (void*)ab, 43) == 25);
    // 두 글자가 같으면 그 폭 그대로 — 줄 최대와 같다
    check("AA → 20 (둘이 같으면 그 폭)", measureLineFloor("AA", 0, 2, fakeInkOf, (void*)ab, 43) == 20);
    // [핵심] 줄 최대(30)보다 **작다** — 이것이 이 함수가存在的 이유
    check("[핵심] AB 하단(25)은 줄 최대(30)보다 작다",
          measureLineFloor("AB", 0, 2, fakeInkOf, (void*)ab, 43)
          < measureLineInk("AB", 0, 2, fakeInkOf, (void*)ab, 43));
    // 인접 쌍의 **최댓값** — 세 글자에서 가운데가 좁으면 양옆의 평균이 더 크다
    check("ABA → 25 (인접 쌍의 최댓값)", measureLineFloor("ABA", 0, 3, fakeInkOf, (void*)ab, 43) == 25);
    check("ABB → 30 (B-B 쌍이 30)", measureLineFloor("ABB", 0, 3, fakeInkOf, (void*)ab, 43) == 30);
    // 공백은 그려지지 않으므로 쌍에서 제외 — "A B"는 A와 B의 인접으로 본다
    check("공백은 쌍에서 제외 — 'A B' → 25", measureLineFloor("A B", 0, 3, fakeInkOf, (void*)ab, 43) == 25);
    // 글자가 하나뿐이면 쌍이 없다 → fallback(항상 안전한 값)
    check("1자 A → fallback 43 (쌍이 없음)", measureLineFloor("A", 0, 1, fakeInkOf, (void*)ab, 43) == 43);
    // 조회 수단 없음 → fallback
    check("inkFn nullptr → fallback", measureLineFloor("AB", 0, 2, nullptr, nullptr, 43) == 43);
    check("글자 수 0 → 0", measureLineFloor("AB", 0, 0, fakeInkOf, (void*)ab, 43) == 0);

    // [하한 계약] 잉크가中央 정렬이므로 이 값 이상이면 실제로 겹치지 않는다.
    //   전수: 어떤 글자 조합에서도 하단을 지키면 인접 쌍이 겹치지 않는다.
    const CellGeometry& g = *geometryForSize(64);
    const uint8_t table[23] = { 24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,38 };
    const char* words[] = { "AB", "AMW", "SEVEN", "TWO", "MONDAY" };
    bool safe = true;
    for (int i = 0; i < 5; i++) {
        const int n = (int)strlen(words[i]);
        const int fl = measureLineFloor(words[i], 0, n, fakeInkOf, (void*)table, 43);
        // 인접 실제 평균의 최댓값을 독립적으로 다시 구해 하단과 일치하는지 본다
        int expect = 0;
        for (int k = 0; k + 1 < n; k++) {
            const int pair = (table[words[i][k] - 'A'] + table[words[i][k + 1] - 'A'] + 1) / 2;
            if (pair > expect) expect = pair;
        }
        if (n < 2) expect = 43;
        if (fl != expect) safe = false;
    }
    check("[하단 계약] 인접 쌍 최댓값과 일치한다", safe);
}

int main() {
    printf("=== layout_engine 단위 테스트 ===\n\n");
    testBasicWrap();
    testLineCapacity();
    testVerticalCentering();
    testCoordinates();
    testSingleLineAndSpaceCell();
    testEdgeCases();
    testLinePitch();
    testLineStartX();
    testExpandedCoordinates();
    testMeasureLineInk();
    testMeasureLineFloor();
    testExhaustiveWithRealTexts();

    printf("\n=== 결과: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
