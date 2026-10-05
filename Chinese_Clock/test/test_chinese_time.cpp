// worynim@gmail.com
/**
 * @file test_chinese_time.cpp
 * @brief PLAN.md §3 표현 규칙 전수표 검증
 * @details 빌드: g++ -std=c++11 -Wall -Wextra -I.. test_chinese_time.cpp chinese_time_core.cpp -o /tmp/test_chinese_time
 *          실행: /tmp/test_chinese_time
 *
 * @note §3의 표가 이 모듈의 **계약**이다. 사람이 검수하지 않아도 두 벌(펌웨어/웹)이
 *       어긋나면 이 테스트가 잡는다. 따라서 기대 문자열은 코드에서 유도하지 않고
 *       아래 표에 **하드코딩**한다 — 유도하면 같은 오류를 두 벌이 동시에 공유한다.
 *
 * @note 특히 주의할 4가지 (PLAN §2.2):
 *   ② 2시는 `两`(시각에만 적용 — 20~23시의 십의 자리는 `二`가 유지된다)
 *   ③ 10분 미만은 `零` 삽입 — 3:05가 "三点五分"으로 오독되면 안 된다
 *   ④ 30분만 `半` — 31분은 "三十一分"으로 되돌아간다
 *   ⑤ 정각은 `整`
 */
#include <cstdio>
#include <cstring>
#include <string>
#include "chinese_time_core.h"

using namespace chtime;

static int g_pass = 0;
static int g_fail = 0;

static void check(const char* what, const std::string& got, const std::string& want) {
    if (got == want) { g_pass++; return; }
    g_fail++;
    printf("  FAIL  %-34s got \"%s\", want \"%s\"\n", what, got.c_str(), want.c_str());
}

/** 성공 반환과 결과 문자열을 함께 단정한다 */
static void checkCall(const char* what, bool ok, const std::string& got, const std::string& want) {
    if (!ok) {
        g_fail++;
        printf("  FAIL  %-34s rejected (expected success)\n", what);
        return;
    }
    check(what, got, want);
}

/**
 * @brief 실패 케이스 단정 — false를 반환하고 버퍼를 빈 문자열로 남겨야 한다
 * @details "한글판 방어선"(PLAN §6.2): 범위 밖 입력은 조용히 빈 문자열이 된다.
 *         반환값만 거짓이 아니라 **버퍼가 비워졌는지**까지 함께 본다.
 */
static void expectRejected(const char* what, bool ok, const char* buf, size_t cap) {
    if (ok) {
        g_fail++;
        printf("  FAIL  %-34s rejected 입력을 받아들였다\n", what);
        return;
    }
    if (cap > 0 && buf[0] != '\0') {
        g_fail++;
        printf("  FAIL  %-34s 실패 시 버퍼가 비워지지 않았다 (\"%s\")\n", what, buf);
        return;
    }
    g_pass++;
}

static std::string callStr(bool (*fn)(int, Script, char*, size_t), int arg, Script s) {
    char buf[CHT_BUF_SIZE];
    memset(buf, 0xAA, sizeof(buf));
    bool ok = fn(arg, s, buf, sizeof(buf));
    if (!ok) return std::string();   // 실패 시 빈 문자열 계약
    return std::string(buf);
}

// === §4.1 문자집합 — 웹 Font Studio가 업로드할 글리프의 기준 ===
// 간체 34자 + 번체 추가 3자(`點` `時` `兩`). web_pages.h의 CHARS_SIMPLIFIED가
// 이 집합과 정확히 일치해야 하며, firmware_wiring_test.mjs가 정적으로 대조한다.
static const std::string CHARSET = "0123456789零一二三四五六七八九十两点时秒分整半上下午星期日點時兩";

static std::string callHour(int hour, bool is24h, Script s) {
    char buf[CHT_BUF_SIZE];
    memset(buf, 0xAA, sizeof(buf));
    bool ok = hourToChars(hour, is24h, s, buf, sizeof(buf));
    if (!ok) return std::string();
    return std::string(buf);
}

/**
 * @brief 문자열에서 §4.1 문자집합에 **없는** 글자들을 이어 반환한다
 * @details UTF-8 문자 단위로 검사한다 (한자 3바이트 / ASCII 1바이트).
 *          표현이 올바르지만 폰트에 글리프가 없는 글자도 조용한 실패이므로,
 *          "표현 정확성"과 "글리프 존재성"을 별개로 검증한다.
 */
static std::string charsNotInCharset(const std::string& text) {
    std::string missing;
    for (size_t k = 0; k < text.size(); ) {
        size_t clen = ((unsigned char)text[k] < 0x80) ? 1 : 3;
        if (k + clen > text.size()) break;
        if (CHARSET.find(text.substr(k, clen)) == std::string::npos) missing += text.substr(k, clen);
        k += clen;
    }
    return missing;
}

// === §3.1 / §3.2 시 ===
// 12H(0→12), 24H(0→零) 두 열을 하나의 표로 가진다.
struct HourCase { int hour; const char* h12_s; const char* h12_t; const char* h24_s; const char* h24_t; };

static const HourCase HOUR_CASES[] = {
    // 12H 열 (0시 = 12시)          24H 열 (0시 = 0시)
    {  0, "十二点", "十二點",        "零点",   "零點"   },
    {  1, "一点",   "一點",          "一点",   "一點"   },
    {  2, "两点",   "兩點",          "两点",   "兩點"   },   // 규칙 ②
    {  3, "三点",   "三點",          "三点",   "三點"   },
    {  4, "四点",   "四點",          "四点",   "四點"   },
    {  5, "五点",   "五點",          "五点",   "五點"   },
    {  6, "六点",   "六點",          "六点",   "六點"   },
    {  7, "七点",   "七點",          "七点",   "七點"   },
    {  8, "八点",   "八點",          "八点",   "八點"   },
    {  9, "九点",   "九點",          "九点",   "九點"   },
    { 10, "十点",   "十點",          "十点",   "十點"   },
    { 11, "十一点", "十一點",        "十一点", "十一點" },
    { 12, "十二点", "十二點",        "十二点", "十二點" },
    { 13, "一点",   "一點",          "十三点", "十三點" },
    { 14, "两点",   "兩點",          "十四点", "十四點" },   // 13시는 1시
    { 15, "三点",   "三點",          "十五点", "十五點" },
    { 16, "四点",   "四點",          "十六点", "十六點" },
    { 17, "五点",   "五點",          "十七点", "十七點" },
    { 18, "六点",   "六點",          "十八点", "十八點" },
    { 19, "七点",   "七點",          "十九点", "十九點" },
    { 20, "八点",   "八點",          "二十点", "二十點" },   // 廿이 아니다 (PLAN §3.2)
    { 21, "九点",   "九點",          "二十一点", "二十一點" },
    { 22, "十点",   "十點",          "二十二点", "二十二點" },
    { 23, "十一点", "十一點",        "二十三点", "二十三點" },
};
static const int HOUR_CASE_COUNT = (int)(sizeof(HOUR_CASES) / sizeof(HOUR_CASES[0]));

// === §3.3 분/초 (분/초 단위 문자만 다르다) ===
struct MsCase { int value; const char* text; };

static const MsCase MS_CASES[] = {
    {  0, "整"     },   // 규칙 ⑤
    {  1, "零一"},   // 규칙 ③ — 1분이 "一分"이 아니라는 점이 핵심
    {  2, "零二"},
    {  3, "零三"},
    {  4, "零四"},
    {  5, "零五"},   // 3:05 케이스의 분 부분
    {  6, "零六"},
    {  7, "零七"},
    {  8, "零八"},
    {  9, "零九"},
    { 10, "十"},
    { 11, "十一"},
    { 12, "十二"},
    { 13, "十三"},
    { 14, "十四"},
    { 15, "十五"},
    { 16, "十六"},
    { 17, "十七"},
    { 18, "十八"},
    { 19, "十九"},
    { 20, "二十"},
    { 21, "二十一"},
    { 22, "二十二"},
    { 23, "二十三"},
    { 24, "二十四"},
    { 25, "二十五"},
    { 26, "二十六"},
    { 27, "二十七"},
    { 28, "二十八"},
    { 29, "二十九"},
    { 30, "半"     },   // 규칙 ④
    { 31, "三十一"},   // 30만 半 — 31부터 다시 규칙적으로
    { 32, "三十二"},
    { 33, "三十三"},
    { 34, "三十四"},
    { 35, "三十五"},
    { 36, "三十六"},
    { 37, "三十七"},
    { 38, "三十八"},
    { 39, "三十九"},
    { 40, "四十"},
    { 41, "四十一"},
    { 42, "四十二"},
    { 43, "四十三"},
    { 44, "四十四"},
    { 45, "四十五"},   // PLAN 제목 예시 — 최장 표현
    { 46, "四十六"},
    { 47, "四十七"},
    { 48, "四十八"},
    { 49, "四十九"},
    { 50, "五十"},
    { 51, "五十一"},
    { 52, "五十二"},
    { 53, "五十三"},
    { 54, "五十四"},
    { 55, "五十五"},
    { 56, "五十六"},
    { 57, "五十七"},
    { 58, "五十八"},
    { 59, "五十九"},
};
static const int MS_CASE_COUNT = (int)(sizeof(MS_CASES) / sizeof(MS_CASES[0]));

// === §3.4 요일 ===
static const char* const WEEKDAYS[7] = {
    "星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"
};

int main() {
    // === 1. 시 (24종 × 12H/24H × 2문자판 = 96) ===
    for (int i = 0; i < HOUR_CASE_COUNT; i++) {
        const HourCase& c = HOUR_CASES[i];
        char label[64];

        snprintf(label, sizeof(label), "hour12 %d 간체", c.hour);
        check(label, callHour(c.hour, false, CHT_SCRIPT_SIMPLIFIED), c.h12_s);
        snprintf(label, sizeof(label), "hour12 %d 번체", c.hour);
        check(label, callHour(c.hour, false, CHT_SCRIPT_TRADITIONAL), c.h12_t);
        snprintf(label, sizeof(label), "hour24 %d 간체", c.hour);
        check(label, callHour(c.hour, true, CHT_SCRIPT_SIMPLIFIED), c.h24_s);
        snprintf(label, sizeof(label), "hour24 %d 번체", c.hour);
        check(label, callHour(c.hour, true, CHT_SCRIPT_TRADITIONAL), c.h24_t);
    }

    // === 2. 분/초 (60 × 2 × 2 = 240) ===
    // 분/초 표현은 번체와 동일하다 — `分`/`秒`가 치환 표에 없기 때문이다.
    for (int i = 0; i < MS_CASE_COUNT; i++) {
        const MsCase& c = MS_CASES[i];
        char label[64];

        // 표는 접미가 없는 **본문**만 담는다 (정각 整 / 반 半은 통째로 예외).
        std::string body = c.text;
        bool whole = (body == "整" || body == "半");
        std::string want_min = whole ? body : body + "分";
        std::string want_sec = whole ? body : body + "秒";

        snprintf(label, sizeof(label), "minute %d 간체", c.value);
        check(label, callStr(minuteToChars, c.value, CHT_SCRIPT_SIMPLIFIED), want_min);
        snprintf(label, sizeof(label), "minute %d 번체", c.value);
        check(label, callStr(minuteToChars, c.value, CHT_SCRIPT_TRADITIONAL), want_min);
        snprintf(label, sizeof(label), "second %d 간체", c.value);
        check(label, callStr(secondToChars, c.value, CHT_SCRIPT_SIMPLIFIED), want_sec);
        snprintf(label, sizeof(label), "second %d 번체", c.value);
        check(label, callStr(secondToChars, c.value, CHT_SCRIPT_TRADITIONAL), want_sec);
    }

    // === 3. 요일 (7 × 2 = 14) ===
    for (int d = 0; d < 7; d++) {
        char label[32];
        snprintf(label, sizeof(label), "weekday %d 간체", d);
        check(label, callStr(weekdayToChars, d, CHT_SCRIPT_SIMPLIFIED), WEEKDAYS[d]);
        snprintf(label, sizeof(label), "weekday %d 번체", d);
        check(label, callStr(weekdayToChars, d, CHT_SCRIPT_TRADITIONAL), WEEKDAYS[d]);
    }

    // === 4. 오전/오후 (24 × 2 = 48) ===
    for (int h = 0; h < 24; h++) {
        const char* want = (h < 12) ? "上午" : "下午";
        char label[32];
        snprintf(label, sizeof(label), "dayPart %d 간체", h);
        check(label, callStr(dayPartToChars, h, CHT_SCRIPT_SIMPLIFIED), want);
        snprintf(label, sizeof(label), "dayPart %d 번체", h);
        check(label, callStr(dayPartToChars, h, CHT_SCRIPT_TRADITIONAL), want);
    }

    // === 5. 경계값 — 범위 밖 입력은 빈 문자열 + false (한글판 방어선) ===
    {
        char buf[CHT_BUF_SIZE];
        char label[48];
        const int bad_hours[] = { -1, 24, 25, 100 };
        for (int i = 0; i < 4; i++) {
            memset(buf, 0xAA, sizeof(buf));
            snprintf(label, sizeof(label), "hour24 %d 거부", bad_hours[i]);
            expectRejected(label, hourToChars(bad_hours[i], true, CHT_SCRIPT_SIMPLIFIED, buf, sizeof(buf)), buf, sizeof(buf));
        }
        const int bad_ms[] = { -1, 60, 61, 255 };
        for (int i = 0; i < 4; i++) {
            memset(buf, 0xAA, sizeof(buf));
            snprintf(label, sizeof(label), "minute %d 거부", bad_ms[i]);
            expectRejected(label, minuteToChars(bad_ms[i], CHT_SCRIPT_SIMPLIFIED, buf, sizeof(buf)), buf, sizeof(buf));
            memset(buf, 0xAA, sizeof(buf));
            snprintf(label, sizeof(label), "second %d 거부", bad_ms[i]);
            expectRejected(label, secondToChars(bad_ms[i], CHT_SCRIPT_SIMPLIFIED, buf, sizeof(buf)), buf, sizeof(buf));
        }
        const int bad_days[] = { -1, 7, 8 };
        for (int i = 0; i < 3; i++) {
            memset(buf, 0xAA, sizeof(buf));
            snprintf(label, sizeof(label), "weekday %d 거부", bad_days[i]);
            expectRejected(label, weekdayToChars(bad_days[i], CHT_SCRIPT_SIMPLIFIED, buf, sizeof(buf)), buf, sizeof(buf));
        }
        // dayPart도 24시간 범위를 벗어난 값을 거부해야 한다
        const int bad_dp[] = { -1, 24, 99 };
        for (int i = 0; i < 3; i++) {
            memset(buf, 0xAA, sizeof(buf));
            snprintf(label, sizeof(label), "dayPart %d 거부", bad_dp[i]);
            expectRejected(label, dayPartToChars(bad_dp[i], CHT_SCRIPT_SIMPLIFIED, buf, sizeof(buf)), buf, sizeof(buf));
        }
        // 12시간제는 0을 받아야 한다 (12시로 환산)
        check("hour12 0 → 十二点", callHour(0, false, CHT_SCRIPT_SIMPLIFIED), "十二点");
    }

    // === 6. NULL / 용량 초과 방어 ===
    {
        char buf[CHT_BUF_SIZE];
        expectRejected("NULL buf 거부", hourToChars(3, true, CHT_SCRIPT_SIMPLIFIED, NULL, 10), NULL, 0);
        expectRejected("cap=0 거부", minuteToChars(5, CHT_SCRIPT_SIMPLIFIED, buf, 0), buf, 0);
        // "四十五分" = 12바이트 + NUL = 13바이트가 필요하다
        memset(buf, 0xAA, sizeof(buf));
        expectRejected("cap=12 (부족) 거부", minuteToChars(45, CHT_SCRIPT_SIMPLIFIED, buf, 12), buf, sizeof(buf));
        memset(buf, 0xAA, sizeof(buf));
        checkCall("cap=13 (최소) 수용", minuteToChars(45, CHT_SCRIPT_SIMPLIFIED, buf, 13), std::string(buf), "四十五分");
    }

    // === 7. 번체 치환표 완전성 — 빠진 글자 0건 ===
    // 위험은 "치환이 안 되는 것"이 아니라 "치환 규칙이 3쌍보다 넓어지는 것"이다.
    // 한자가 추가되면 1쌍이 늘어난다 — 그 사실을 §3의 표가 아니라 **테스트가** 먼저 알려야 한다.
    {
        // (a) 간체→번체로 달라지는 글자는 **오직 `点` 하나**뿐이어야 한다 (시 표현 기준).
        //     예외는 `两`→`兩` — 값이 2인 시뿐이다. 12H에서는 14시도 2시다.
        for (int h = 0; h < 24; h++) {
            for (int is24 = 0; is24 < 2; is24++) {
                int h12 = (h % 12 == 0) ? 12 : (h % 12);
                bool isTwoOClock = (is24 ? (h == 2) : (h12 == 2));
                if (isTwoOClock) continue;

                std::string s = callHour(h, is24 == 1, CHT_SCRIPT_SIMPLIFIED);
                std::string t = callHour(h, is24 == 1, CHT_SCRIPT_TRADITIONAL);
                // 마지막 글자(时/點)를 제외한 나머지는 **글자 단위로** 같아야 한다.
                bool same = s.size() == t.size();
                if (same) {
                    for (size_t k = 0; k + 3 <= s.size() - 3; k += 3) {
                        if (s.compare(k, 3, t, k, 3) != 0) { same = false; break; }
                    }
                }
                if (!same) {
                    g_fail++;
                    printf("  FAIL  번체 치환이 과도하다: h=%d is24h=%d \"%s\" → \"%s\"\n",
                           h, is24, s.c_str(), t.c_str());
                }
            }
        }

        // (b) `两`→`兩`가 치환되는 것은 **2시에서만**이다. 20~23시의 십의 자리는 `二`가 유지된다 (PLAN §3.2).
        for (int h = 20; h < 24; h++) {
            std::string t = callHour(h, true, CHT_SCRIPT_TRADITIONAL);
            if (t.find("兩") != std::string::npos) {
                g_fail++;
                printf("  FAIL  20~23시의 십의 자리가 兩로 바뀌었다: h=%d \"%s\"\n", h, t.c_str());
            }
        }

        // (c) 치환 대상이 아닌 표현은 문자판에 무관해야 한다 (分/秒/整/半/上下午/星期X).
        check("45분 문자판 무관", callStr(minuteToChars, 45, CHT_SCRIPT_SIMPLIFIED),
                                      callStr(minuteToChars, 45, CHT_SCRIPT_TRADITIONAL));
        check("45초 문자판 무관", callStr(secondToChars, 45, CHT_SCRIPT_SIMPLIFIED),
                                      callStr(secondToChars, 45, CHT_SCRIPT_TRADITIONAL));
        check("정각 문자판 무관", callStr(minuteToChars, 0, CHT_SCRIPT_SIMPLIFIED),
                                  callStr(minuteToChars, 0, CHT_SCRIPT_TRADITIONAL));
        check("30분(半) 문자판 무관", callStr(minuteToChars, 30, CHT_SCRIPT_SIMPLIFIED),
                                        callStr(minuteToChars, 30, CHT_SCRIPT_TRADITIONAL));
        for (int d = 0; d < 7; d++) {
            check("요일 문자판 무관", callStr(weekdayToChars, d, CHT_SCRIPT_SIMPLIFIED),
                                      callStr(weekdayToChars, d, CHT_SCRIPT_TRADITIONAL));
        }
        for (int h = 0; h < 24; h++) {
            check("오전오후 문자판 무관", callStr(dayPartToChars, h, CHT_SCRIPT_SIMPLIFIED),
                                           callStr(dayPartToChars, h, CHT_SCRIPT_TRADITIONAL));
        }
    }

    // === 8. 표현 길이 상한 — 전 표현이 4자(12바이트) 이내 (PLAN §3.6) ===
    // 이 검사가 깨지면 32px 피치 × 4 = 128px 설계가 파손된다 (ENG판 SEVENTEEN 사례와 같다).
    {
        int maxChars = 0;
        for (int h = 0; h < 24; h++) {
            std::string v = callHour(h, true, CHT_SCRIPT_SIMPLIFIED);
            int n = (int)v.size() / 3;   // 한자는 UTF-8 3바이트
            if (n > maxChars) maxChars = n;
            if (n > 4) { g_fail++; printf("  FAIL  시 표현 4자 초과: h=%d \"%s\"\n", h, v.c_str()); }
            v = callHour(h, false, CHT_SCRIPT_SIMPLIFIED);
            n = (int)v.size() / 3;
            if (n > 4) { g_fail++; printf("  FAIL  12H 시 표현 4자 초과: h=%d \"%s\"\n", h, v.c_str()); }
        }
        for (int m = 0; m < 60; m++) {
            std::string v = callStr(minuteToChars, m, CHT_SCRIPT_SIMPLIFIED);
            int n = (int)v.size() / 3;
            if (n > maxChars) maxChars = n;
            if (n > 4) { g_fail++; printf("  FAIL  분 표현 4자 초과: m=%d \"%s\"\n", m, v.c_str()); }
            v = callStr(secondToChars, m, CHT_SCRIPT_SIMPLIFIED);
            n = (int)v.size() / 3;
            if (n > 4) { g_fail++; printf("  FAIL  초 표현 4자 초과: s=%d \"%s\"\n", m, v.c_str()); }
        }
        for (int d = 0; d < 7; d++) {
            int n = (int)callStr(weekdayToChars, d, CHT_SCRIPT_SIMPLIFIED).size() / 3;
            if (n > maxChars) maxChars = n;
            if (n > 4) { g_fail++; printf("  FAIL  요일 표현 4자 초과: d=%d\n", d); }
        }
        printf("  [info] 관측된 최장 표현 = %d자\n", maxChars);
        check("최장 표현은 4자", std::string(maxChars == 4 ? "4" : "?"), "4");
    }

    // === 9. 숫자 모드 (twoDigit) ===
    {
        const char* want[] = { "00", "01", "09", "10", "30", "45", "59" };
        const int vals[] = { 0, 1, 9, 10, 30, 45, 59 };
        char buf[8];
        for (int i = 0; i < 7; i++) {
            memset(buf, 0xAA, sizeof(buf));
            bool ok = twoDigit(vals[i], buf, sizeof(buf));
            char label[32];
            snprintf(label, sizeof(label), "twoDigit %d", vals[i]);
            checkCall(label, ok, std::string(buf), want[i]);
        }
        memset(buf, 0xAA, sizeof(buf));
        expectRejected("twoDigit 60 거부", twoDigit(60, buf, sizeof(buf)), buf, sizeof(buf));
        memset(buf, 0xAA, sizeof(buf));
        expectRejected("twoDigit -1 거부", twoDigit(-1, buf, sizeof(buf)), buf, sizeof(buf));
    }

    // === 10. 문자집합 포함성 — core가 만드는 모든 글자가 §4.1 집합 안에 있는가 ===
    // 이 검사가 없으면 표현은 맞지만 폰트에 글리프가 없는 글자가 조용히 깨진다.
    // PLAN §4.1이 폰트 업로드 문자집합의 유일한 기준이므로 여기서 닫는다.
    {
        std::string missing;
        for (int i = 0; i < 24; i++)
            for (int is24 = 0; is24 < 2; is24++)
                for (int scr = 0; scr < 2; scr++)
                    missing += charsNotInCharset(callHour(i, is24 == 1, (Script)scr));

        for (int v = 0; v < 60; v++)
            for (int s = 0; s < 2; s++) {
                missing += charsNotInCharset(callStr(minuteToChars, v, (Script)s));
                missing += charsNotInCharset(callStr(secondToChars, v, (Script)s));
            }

        for (int d = 0; d < 7; d++)
            for (int s = 0; s < 2; s++)
                missing += charsNotInCharset(callStr(weekdayToChars, d, (Script)s));

        for (int h = 0; h < 24; h++)
            for (int s = 0; s < 2; s++)
                missing += charsNotInCharset(callStr(dayPartToChars, h, (Script)s));

        check("생성 글자가 §4.1 집합에 모두 포함", missing, "");
    }

    // === 11. 문자판 전용 글자 목록 — DisplayManager의 전환 검증이 쓰는 값 ===
    // setScriptType()은 슬롯을 옮기지 않고 **이 목록**으로 "그 문자판을 그릴 수 있는가"를
    // 판정한다. 목록이 실제 치환표와 어긋나면(중복·누락) 잘못된 전환을 허용하거나,
    // 아무데도 안 쓰는 글자를 요구해 정당한 전환을 막는다. 양쪽 다 조용한 실패다.
    {
        check("간체 전용 글자 수 = 3",
              std::to_string(scriptOnlyCount(CHT_SCRIPT_SIMPLIFIED)), "3");
        check("번체 전용 글자 수 = 3",
              std::to_string(scriptOnlyCount(CHT_SCRIPT_TRADITIONAL)), "3");

        const char* want_s[] = { "点", "时", "两" };
        const char* want_t[] = { "點", "時", "兩" };
        for (size_t i = 0; i < 3; i++) {
            const char* gs = scriptOnlyGlyph(CHT_SCRIPT_SIMPLIFIED, i);
            const char* gt = scriptOnlyGlyph(CHT_SCRIPT_TRADITIONAL, i);
            char label[48];
            snprintf(label, sizeof(label), "간체 전용 %u", (unsigned)i);
            check(label, gs ? std::string(gs) : std::string("(null)"), std::string(want_s[i]));
            snprintf(label, sizeof(label), "번체 전용 %u", (unsigned)i);
            check(label, gt ? std::string(gt) : std::string("(null)"), std::string(want_t[i]));
        }

        // 범위 밖은 nullptr — 없는 글자를 "요구"하지 않도록 (방어선)
        check("범위 밖 nullptr (간체)", std::string(scriptOnlyGlyph(CHT_SCRIPT_SIMPLIFIED, 3) ? "?" : ""), "");
        check("범위 밖 nullptr (번체)", std::string(scriptOnlyGlyph(CHT_SCRIPT_TRADITIONAL, 99) ? "?" : ""), "");

        // 이 목록이 §4.1 업로드 문자집합(37자) 안에 있어야 전환 검사가 잘못 막히지 않는다.
        // Font Studio는 두 문자판의 합집합을 슬롯에 올린다 — 여기서 그 계약이 닫힌다.
        for (int scr = 0; scr < 2; scr++) {
            for (size_t i = 0; i < scriptOnlyCount((Script)scr); i++) {
                const char* g = scriptOnlyGlyph((Script)scr, i);
                if (!g || CHARSET.find(g) == std::string::npos) {
                    g_fail++;
                    printf("  FAIL  전용 글자가 §4.1 집합에 없음: %s\n", g ? g : "(null)");
                } else {
                    g_pass++;
                }
            }
        }
    }

    printf("test_chinese_time: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}