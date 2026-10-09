// worynim@gmail.com
/**
 * @file test_utf8_len.cpp
 * @brief UTF-8 리딩 바이트 판정 회귀 테스트
 * @details 빌드: g++ -std=c++11 -I.. test_utf8_len.cpp -o /tmp/test_utf8_len
 *          실행: /tmp/test_utf8_len
 *
 * @note 재현되는 버그 (Hangeul_Clock/renderer.cpp:getCharData):
 *       3바이트 판정이 `(c & 0xE0) == 0xE0`이었다. 이 조건은 c가 0xE0~0xFF일 때 참이라
 *       **4바이트 리딩 바이트(0xF0~0xF7)를 3으로 잘못 판정**했다. 결과적으로 4바이트
 *       문자가 3바이트로 잘려 깨진 글리프가 화면에 그려졌고, 4바이트 분기까지는 도달하지
 *       못했다. 올바른 마스크는 `(c & 0xF0) == 0xE0` (0xE0~0xEF만 참).
 */
#include <cstdio>
#include "utf8_len.h"

static int g_pass = 0;
static int g_fail = 0;

static void expectLen(const char* label, unsigned char lead, int remaining, uint8_t want) {
    const char s[2] = {(char)lead, 0};
    const uint8_t got = utf8CharLen(s, remaining);
    if (got == want) {
        g_pass++;
    } else {
        g_fail++;
        printf("  FAIL  [%s] lead=0x%02X remaining=%d → got %u, want %u\n",
               label, lead, remaining, got, want);
    }
}

int main() {
    // --- 회귀: 4바이트 리딩 바이트는 반드시 4로 판정되어야 한다 ---
    //   이전 마스크는 여기서 3을 반환했다(0xF0 & 0xE0 == 0xE0).
    expectLen("4바이트 (U+1F300 이모티콘)", 0xF0, 4, 4);
    expectLen("4바이트 (U+1F600)",           0xF1, 4, 4);
    expectLen("4바이트 (U+10FFFF 상한)",     0xF4, 4, 4);
    expectLen("4바이트 리딩 상한 0xF7",      0xF7, 4, 4);

    // --- 3바이트 (한글 음절이 여기 해당) ---
    expectLen("3바이트 한글 '한'", 0xEA, 3, 3);
    expectLen("3바이트 하한 0xE0", 0xE0, 3, 3);
    expectLen("3바이트 상한 0xEF", 0xEF, 3, 3);

    // --- 2바이트 ---
    expectLen("2바이트 (U+00E9)", 0xC3, 2, 2);
    expectLen("2바이트 하한 0xC0", 0xC0, 2, 2);
    expectLen("2바이트 상한 0xDF", 0xDF, 2, 2);

    // --- 1바이트 (ASCII) ---
    expectLen("ASCII 'A'", 'A', 1, 1);
    expectLen("숫자 '7'",  '7', 1, 1);
    expectLen("공백",       ' ', 1, 1);
    expectLen("ASCII 최상한 0x7F", 0x7F, 1, 1);

    // --- 컨티뉴레이션 바이트(0x80~0xBF)가 선행으로 오면 1로 폴백 ---
    //   잘못된 바이트열에서도 전진은 반드시 일어나야 한다(0이면 무한 루프).
    expectLen("컨티뉴레이션 0x80", 0x80, 4, 1);
    expectLen("컨티뉴레이션 0xBF", 0xBF, 4, 1);

    // --- 잘린 멀티바이트: 남은 바이트보다 길게 요구하면 잘린다 ---
    expectLen("잘림: 3바이트 바이트 1개만 남음", 0xEA, 1, 1);
    expectLen("잘림: 4바이트 바이트 2개만 남음", 0xF0, 2, 2);

    // --- remaining <= 0 은 0이 아니라 1 (호출부가 i += len 으로 전진하기 때문) ---
    expectLen("remaining=0 → 무한 루프 방지", 0xEA, 0, 1);
    expectLen("remaining=-1", 0xEA, -1, 1);

    printf("test_utf8_len: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}