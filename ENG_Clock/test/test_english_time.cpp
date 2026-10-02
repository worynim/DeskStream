// worynim@gmail.com
/**
 * @file test_english_time.cpp
 * @brief english_time_core 네이티브 단위 테스트
 * @details 빌드: g++ -std=c++11 -I.. test_english_time.cpp ../english_time_core.cpp -o test_english_time
 *          실행: ./test_english_time
 *          (Arduino 헤더 의존성이 없어 네이티브에서 그대로 컴파일된다)
 */
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "english_time_core.h"

static int g_pass = 0;
static int g_fail = 0;

static void expectStr(const char* label, const char* actual, const char* expected) {
    if (strcmp(actual, expected) == 0) {
        g_pass++;
    } else {
        g_fail++;
        printf("  FAIL  %-34s got=\"%s\"  want=\"%s\"\n", label, actual, expected);
    }
}

static void expectBool(const char* label, bool actual, bool expected) {
    if (actual == expected) {
        g_pass++;
    } else {
        g_fail++;
        printf("  FAIL  %-34s got=%s  want=%s\n", label,
               actual ? "true" : "false", expected ? "true" : "false");
    }
}

static void expectInt(const char* label, int actual, int expected) {
    if (actual == expected) {
        g_pass++;
    } else {
        g_fail++;
        printf("  FAIL  %-34s got=%d  want=%d\n", label, actual, expected);
    }
}

/* 1줄 수용 폭 (PLAN §3.1). glyph pitch 14px 기준 128/14 = 9자 */
#define CHARS_PER_LINE 9

// --- 개별 케이스 -------------------------------------------------------

static void testNumberToWords() {
    printf("numberToWords\n");
    char b[ENG_BUF_SIZE];
    engtime::numberToWords(0, b, sizeof(b));   expectStr("0", b, "ZERO");
    engtime::numberToWords(1, b, sizeof(b));   expectStr("1", b, "ONE");
    engtime::numberToWords(9, b, sizeof(b));   expectStr("9", b, "NINE");
    engtime::numberToWords(10, b, sizeof(b));  expectStr("10", b, "TEN");
    engtime::numberToWords(11, b, sizeof(b));  expectStr("11", b, "ELEVEN");
    engtime::numberToWords(12, b, sizeof(b));  expectStr("12", b, "TWELVE");
    engtime::numberToWords(13, b, sizeof(b));  expectStr("13", b, "THIRTEEN");
    engtime::numberToWords(15, b, sizeof(b));  expectStr("15", b, "FIFTEEN");
    engtime::numberToWords(19, b, sizeof(b));  expectStr("19", b, "NINETEEN");
    engtime::numberToWords(20, b, sizeof(b));  expectStr("20", b, "TWENTY");
    engtime::numberToWords(21, b, sizeof(b));  expectStr("21", b, "TWENTY ONE");
    engtime::numberToWords(30, b, sizeof(b));  expectStr("30 (일의 자리 생략)", b, "THIRTY");
    engtime::numberToWords(37, b, sizeof(b));  expectStr("37", b, "THIRTY SEVEN");
    engtime::numberToWords(40, b, sizeof(b));  expectStr("40", b, "FORTY");
    engtime::numberToWords(45, b, sizeof(b));  expectStr("45", b, "FORTY FIVE");
    engtime::numberToWords(50, b, sizeof(b));  expectStr("50", b, "FIFTY");
    engtime::numberToWords(59, b, sizeof(b));  expectStr("59", b, "FIFTY NINE");
}

static void testNumberToWordsBoundaries() {
    printf("numberToWords 경계/실패 경로\n");
    char b[ENG_BUF_SIZE];

    expectBool("-1 거부", engtime::numberToWords(-1, b, sizeof(b)), false);
    expectStr("-1 → 빈 문자열", b, "");
    expectBool("60 거부", engtime::numberToWords(60, b, sizeof(b)), false);
    expectStr("60 → 빈 문자열", b, "");
    expectBool("9999 거부", engtime::numberToWords(9999, b, sizeof(b)), false);

    // 용량 부족 시 실패하고 빈 문자열 (버퍼 오버플로 방지)
    char tiny[3];
    expectBool("용량 3(< 'ONE') 거부", engtime::numberToWords(1, tiny, sizeof(tiny)), false);
    expectStr("용량 부족 → 빈 문자열", tiny, "");

    expectBool("buf=NULL 거부", engtime::numberToWords(1, NULL, 10), false);
    expectBool("cap=0 거부", engtime::numberToWords(1, b, 0), false);
}

static void testHourToWords() {
    printf("hourToWords\n");
    char b[ENG_BUF_SIZE];

    // 12시간제: 0시 → TWELVE
    engtime::hourToWords(0, false, b, sizeof(b));   expectStr("12h 0시", b, "TWELVE");
    engtime::hourToWords(1, false, b, sizeof(b));   expectStr("12h 1시", b, "ONE");
    engtime::hourToWords(9, false, b, sizeof(b));   expectStr("12h 9시", b, "NINE");
    engtime::hourToWords(11, false, b, sizeof(b));  expectStr("12h 11시", b, "ELEVEN");
    engtime::hourToWords(12, false, b, sizeof(b));  expectStr("12h 12시", b, "TWELVE");
    engtime::hourToWords(13, false, b, sizeof(b));  expectStr("12h 13시(=1시)", b, "ONE");
    engtime::hourToWords(23, false, b, sizeof(b));  expectStr("12h 23시(=11시)", b, "ELEVEN");

    // 24시간제: 0시 → ZERO
    engtime::hourToWords(0, true, b, sizeof(b));    expectStr("24h 0시", b, "ZERO");
    engtime::hourToWords(9, true, b, sizeof(b));    expectStr("24h 9시", b, "NINE");
    engtime::hourToWords(13, true, b, sizeof(b));   expectStr("24h 13시", b, "THIRTEEN");
    engtime::hourToWords(20, true, b, sizeof(b));   expectStr("24h 20시", b, "TWENTY");
    engtime::hourToWords(23, true, b, sizeof(b));   expectStr("24h 23시", b, "TWENTY THREE");

    expectBool("24h 24시 거부", engtime::hourToWords(24, true, b, sizeof(b)), false);
    expectBool("24h -1시 거부", engtime::hourToWords(-1, true, b, sizeof(b)), false);
}

static void testMinuteSecondWords() {
    printf("minuteToWords / secondToWords\n");
    char b[ENG_BUF_SIZE];

    engtime::minuteToWords(0, b, sizeof(b));   expectStr("분 0 → O'CLOCK", b, "O'CLOCK");
    engtime::minuteToWords(1, b, sizeof(b));   expectStr("분 1", b, "ONE");
    engtime::minuteToWords(30, b, sizeof(b));  expectStr("분 30", b, "THIRTY");
    engtime::minuteToWords(45, b, sizeof(b));  expectStr("분 45", b, "FORTY FIVE");

    engtime::secondToWords(0, b, sizeof(b));   expectStr("초 0 → O'CLOCK", b, "O'CLOCK");
    engtime::secondToWords(59, b, sizeof(b));  expectStr("초 59", b, "FIFTY NINE");

    expectBool("분 60 거부", engtime::minuteToWords(60, b, sizeof(b)), false);
    expectBool("초 60 거부", engtime::secondToWords(60, b, sizeof(b)), false);
    expectBool("초 -1 거부", engtime::secondToWords(-1, b, sizeof(b)), false);
}

static void testAmPm() {
    printf("amPm\n");
    char b[ENG_BUF_SIZE];
    engtime::amPm(0, b, sizeof(b));   expectStr("0시", b, "AM");
    engtime::amPm(11, b, sizeof(b));  expectStr("11시", b, "AM");
    engtime::amPm(12, b, sizeof(b));  expectStr("12시(정오)", b, "PM");
    engtime::amPm(13, b, sizeof(b));  expectStr("13시", b, "PM");
    engtime::amPm(23, b, sizeof(b));  expectStr("23시", b, "PM");
    expectBool("24시 거부", engtime::amPm(24, b, sizeof(b)), false);
}

static void testTwoDigit() {
    printf("twoDigit\n");
    char b[ENG_BUF_SIZE];
    engtime::twoDigit(0, b, sizeof(b));   expectStr("0", b, "00");
    engtime::twoDigit(5, b, sizeof(b));   expectStr("5", b, "05");
    engtime::twoDigit(23, b, sizeof(b));  expectStr("23", b, "23");
    engtime::twoDigit(59, b, sizeof(b));  expectStr("59", b, "59");
    expectBool("60 거부", engtime::twoDigit(60, b, sizeof(b)), false);
}

static void testJoinWords() {
    printf("joinWords\n");
    char b[ENG_BUF_SIZE];
    expectBool("A + B", engtime::joinWords("THIRTY", "SEVEN", b, sizeof(b)), true);
    expectStr("THIRTY + SEVEN", b, "THIRTY SEVEN");
    expectBool("빈 문자열 거부", engtime::joinWords("", "SEVEN", b, sizeof(b)), false);
    expectBool("NULL 거부", engtime::joinWords(NULL, "SEVEN", b, sizeof(b)), false);
    expectStr("실패 → 빈 문자열", b, "");
}

static void testDayName() {
    printf("dayName\n");
    char b[ENG_BUF_SIZE];
    const char* want[7] = {"SUNDAY","MONDAY","TUESDAY","WEDNESDAY","THURSDAY","FRIDAY","SATURDAY"};
    for (int d = 0; d < 7; d++) {
        engtime::dayName(d, b, sizeof(b));
        char label[32]; snprintf(label, sizeof(label), "day %d", d);
        expectStr(label, b, want[d]);
    }
    expectBool("day -1 거부", engtime::dayName(-1, b, sizeof(b)), false);
    expectBool("day 7 거부", engtime::dayName(7, b, sizeof(b)), false);

    // 최장 요일명 WEDNESDAY(9자)가 1줄 한계와 정확히 일치하는지
    int worst = 0;
    for (int d = 0; d < 7; d++) {
        engtime::dayName(d, b, sizeof(b));
        int len = (int)strlen(b); if (len > worst) worst = len;
    }
    printf("  최장 요일명 = %d자\n", worst);
    expectInt("최장 요일명 9자 = 줄 너비와 동일", worst, CHARS_PER_LINE);
}

/** 어절 단위 줄바꿈 후, 어느 줄도 CHARS_PER_LINE을 넘지 않는지 검사 */
static bool fitsTwoLines(const char* text) {
    int lineLen = 0;
    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] == ' ') { lineLen = 0; continue; }
        lineLen++;
        if (lineLen > CHARS_PER_LINE) return false;
    }
    return true;
}

static void testExhaustiveTwoLineFit() {
    printf("전수 검증: 모든 시각 표현이 2줄 × %d자 안에 수용되는가\n", CHARS_PER_LINE);
    char b[ENG_BUF_SIZE];
    int worst = 0, worstSrc = -1;
    int violations = 0;

    for (int h = 0; h < 24; h++) {
        for (bool is24h : {false, true}) {
            if (engtime::hourToWords(h, is24h, b, sizeof(b))) {
                if (!fitsTwoLines(b)) { violations++; printf("  FAIL 시(h=%d 24h=%d) \"%s\"\n", h, is24h, b); }
                int len = (int)strlen(b); if (len > worst) { worst = len; worstSrc = h; }
            }
        }
    }
    for (int m = 0; m < 60; m++) {
        if (engtime::minuteToWords(m, b, sizeof(b))) {
            if (!fitsTwoLines(b)) { violations++; printf("  FAIL 분(m=%d) \"%s\"\n", m, b); }
            int len = (int)strlen(b); if (len > worst) { worst = len; worstSrc = 1000 + m; }
        }
        if (engtime::secondToWords(m, b, sizeof(b))) {
            if (!fitsTwoLines(b)) { violations++; printf("  FAIL 초(s=%d) \"%s\"\n", m, b); }
        }
    }

    printf("  최장 표현 = %d자 (원본 %d)\n", worst, worstSrc);
    expectInt("2줄 수용 위반 없음", violations, 0);
}

/** 최장 단어 하나만 뽑아 9자 제한을 직접 검증 (SEVENTEEN=9 가 핵심 케이스) */
/**
 * [수정할 사항 3] 날짜 문자열 — 24시간제 첫 화면의 첫 줄로 쓰인다.
 * 일/월 순서는 기본값(사용자 지정 "2/10"), 월/일은 웹에서 선택한다.
 */
static void testDateString() {
    printf("날짜 문자열 (일/월 ↔ 월/일)\n");
    char b[ENG_BUF_SIZE];

    expectBool("2026-10-02 일/월 성공", engtime::dateString(10, 2, false, b, sizeof(b)), true);
    expectStr("2026-10-02 일/월 = 2/10", b, "2/10");
    expectBool("2026-10-02 월/일 성공", engtime::dateString(10, 2, true, b, sizeof(b)), true);
    expectStr("2026-10-02 월/일 = 10/2", b, "10/2");

    // 선행 0 없음
    expectBool("2026-01-09", engtime::dateString(1, 9, false, b, sizeof(b)), true);
    expectStr("2026-01-09 일/월 = 9/1", b, "9/1");
    expectBool("2026-01-09 월/일", engtime::dateString(1, 9, true, b, sizeof(b)), true);
    expectStr("2026-01-09 월/일 = 1/9", b, "1/9");

    // 최장: 두 자리씩 (5자 + NUL)
    expectStr("최장 31/12", [&]{ engtime::dateString(12, 31, false, b, sizeof(b)); return b; }(), "31/12");
    expectStr("최장 12/31", [&]{ engtime::dateString(12, 31, true, b, sizeof(b)); return b; }(), "12/31");
    expectInt("최장 5자 = 줄 9자 이내", (int)strlen("31/12"), 5);

    // 경계: 월 1~12, 일 1~31
    expectStr("1/1", [&]{ engtime::dateString(1, 1, false, b, sizeof(b)); return b; }(), "1/1");
    expectStr("12/1", [&]{ engtime::dateString(12, 1, true, b, sizeof(b)); return b; }(), "12/1");

    // 범위 밖 입력은 빈 문자열 (tm_mon+1 규약 위반 방지)
    expectBool("월 0 거부",  engtime::dateString(0, 10, false, b, sizeof(b)), false);
    expectStr("월 0 → 빈 문자열", b, "");
    expectBool("월 13 거부", engtime::dateString(13, 10, false, b, sizeof(b)), false);
    expectStr("월 13 → 빈 문자열", b, "");
    expectBool("일 0 거부",  engtime::dateString(10, 0, false, b, sizeof(b)), false);
    expectStr("일 0 → 빈 문자열", b, "");
    expectBool("일 32 거부", engtime::dateString(10, 32, false, b, sizeof(b)), false);
    expectStr("일 32 → 빈 문자열", b, "");
    expectBool("음수 거부",   engtime::dateString(-1, -1, false, b, sizeof(b)), false);

    // 용량 부족 실패 경로
    char tiny[3];
    expectBool("용량 3B는 실패", engtime::dateString(10, 2, false, tiny, sizeof(tiny)), false);
    expectStr("용량 실패 → 빈 문자열", tiny, "");
}

static void testLongestSingleWord() {
    printf("최장 단어 검증\n");
    char b[ENG_BUF_SIZE];
    int worstWord = 0; char worst[ENG_BUF_SIZE] = {0};
    for (int n = 0; n < 60; n++) {
        if (!engtime::numberToWords(n, b, sizeof(b))) continue;
        // 공백 기준 최장 토큰
        const char* p = b;
        while (*p) {
            const char* sp = strchr(p, ' ');
            int len = sp ? (int)(sp - p) : (int)strlen(p);
            if (len > worstWord) { worstWord = len; strncpy(worst, p, len); worst[len] = '\0'; }
            if (!sp) break;
            p = sp + 1;
        }
    }
    printf("  최장 단어 = \"%s\" (%d자)\n", worst, worstWord);
    expectStr("최장 단어는 SEVENTEEN", worst, "SEVENTEEN");
    expectInt("최장 단어 9자 = 줄 너비와 동일", worstWord, CHARS_PER_LINE);
}

int main() {
    printf("=== english_time_core 단위 테스트 ===\n\n");
    testNumberToWords();
    testNumberToWordsBoundaries();
    testHourToWords();
    testMinuteSecondWords();
    testAmPm();
    testTwoDigit();
    testJoinWords();
    testDayName();
    testDateString();
    testExhaustiveTwoLineFit();
    testLongestSingleWord();

    printf("\n=== 결과: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
