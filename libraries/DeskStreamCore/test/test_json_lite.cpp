// worynim@gmail.com
/**
 * @file test_json_lite.cpp
 * @brief 평평한 JSON 키 값 파서 회귀 테스트
 * @details 실행: bash test/run_all.sh
 * @note 고정하는 것:
 *       1) 키 오프셋 버그 회귀 — 예전 문자열 파싱이 키 리터럴 길이를 잘못 더해
 *          "KST-9"가 "ST-9"로 저장됐다.
 *       2) 문자열 값 안의 `\"key\"`, 값 위치의 같은 문자열을 키로 오인하지 않는다.
 *       3) 지원하지 않는 이스케이프·잘림은 **거부**한다 (잘린 값을 설정에 쓰지 않는다).
 */
#include <cstdio>
#include <cstring>
#include "util/ds_json_lite.h"

using namespace ds;

static int g_pass = 0;
static int g_fail = 0;

static void check(const char* label, bool ok) {
    if (ok) g_pass++;
    else { g_fail++; printf("  FAIL  %s\n", label); }
}

static void testInt() {
    long v = 0;
    check("정수", jsonGetInt("{\"anim_mode\":3}", "anim_mode", &v) && v == 3);
    check("공백 허용", jsonGetInt("{ \"anim_mode\" :  12 }", "anim_mode", &v) && v == 12);
    check("음수", jsonGetInt("{\"x\":-5}", "x", &v) && v == -5);
    check("키 없음 → false", !jsonGetInt("{\"a\":1}", "anim_mode", &v));
    check("따옴표 숫자 거부", !jsonGetInt("{\"x\":\"3\"}", "x", &v));
    check("bool 거부", !jsonGetInt("{\"x\":true}", "x", &v));
    check("접두어 키와 구분 (font_slot vs font_slot_x)",
          jsonGetInt("{\"font_slot_x\":9,\"font_slot\":2}", "font_slot", &v) && v == 2);
    check("터무니없는 크기 거부", !jsonGetInt("{\"x\":99999999999}", "x", &v));
}

static void testBool() {
    bool b = false;
    check("true", jsonGetBool("{\"is_flipped\":true}", "is_flipped", &b) && b);
    check("false", jsonGetBool("{\"is_flipped\": false}", "is_flipped", &b) && !b);
    check("숫자 거부", !jsonGetBool("{\"is_flipped\":1}", "is_flipped", &b));
    check("키 없음", !jsonGetBool("{}", "is_flipped", &b));
}

static void testString() {
    char s[48];
    // [회귀] 첫 글자 유실 ("KST-9" → "ST-9")
    check("timezone 첫 글자 보존",
          jsonGetString("{\"font_slot\":1,\"timezone\":\"KST-9\"}", "timezone", s, sizeof(s)) && strcmp(s, "KST-9") == 0);
    check("공백 뒤 값", jsonGetString("{\"timezone\" : \"UTC0\"}", "timezone", s, sizeof(s)) && strcmp(s, "UTC0") == 0);
    check("이스케이프 \\\" 풀기",
          jsonGetString("{\"n\":\"a\\\"b\"}", "n", s, sizeof(s)) && strcmp(s, "a\"b") == 0);
    check("이스케이프 \\\\ \\/ 풀기",
          jsonGetString("{\"n\":\"a\\\\b\\/c\"}", "n", s, sizeof(s)) && strcmp(s, "a\\b/c") == 0);
    check("UTF-8 그대로", jsonGetString("{\"n\":\"한글폰트\"}", "n", s, sizeof(s)) && strcmp(s, "한글폰트") == 0);
    check("빈 문자열", jsonGetString("{\"n\":\"\"}", "n", s, sizeof(s)) && s[0] == '\0');
    check("\\n 이스케이프 거부", !jsonGetString("{\"n\":\"a\\nb\"}", "n", s, sizeof(s)));
    check("닫는 따옴표 없음 거부", !jsonGetString("{\"n\":\"abc", "n", s, sizeof(s)));
    check("숫자 값 거부", !jsonGetString("{\"n\":5}", "n", s, sizeof(s)));

    char small[4];
    check("cap 정확히 맞음 (3자 + NUL)", jsonGetString("{\"n\":\"abc\"}", "n", small, sizeof(small)) && strcmp(small, "abc") == 0);
    check("cap 초과 거부", !jsonGetString("{\"n\":\"abcd\"}", "n", small, sizeof(small)));
}

static void testKeyConfusion() {
    char s[48];
    long v = 0;
    // 값 안에 이스케이프된 "timezone" 이 들어 있어도 키로 보지 않는다
    check("문자열 값 속 \\\"key\\\" 무시",
          jsonGetString("{\"font_name\":\"x\\\"timezone\\\":\\\"EVIL\",\"timezone\":\"KST-9\"}", "timezone", s, sizeof(s))
              && strcmp(s, "KST-9") == 0);
    // 값 위치의 같은 문자열 ("timezone" 이라는 값) 은 키가 아니다
    check("값 위치의 같은 문자열 무시",
          jsonGetString("{\"a\":\"timezone\",\"timezone\":\"UTC0\"}", "timezone", s, sizeof(s)) && strcmp(s, "UTC0") == 0);
    check("NULL 본문", !jsonGetInt(nullptr, "x", &v));
}

int main() {
    testInt();
    testBool();
    testString();
    testKeyConfusion();
    printf("test_json_lite: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
