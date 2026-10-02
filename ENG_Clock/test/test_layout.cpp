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

int main() {
    printf("=== layout_engine 단위 테스트 ===\n\n");
    testBasicWrap();
    testLineCapacity();
    testVerticalCentering();
    testCoordinates();
    testSingleLineAndSpaceCell();
    testEdgeCases();
    testExhaustiveWithRealTexts();

    printf("\n=== 결과: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
