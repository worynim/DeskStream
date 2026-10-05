// worynim@gmail.com
/**
 * @file test_tz_util.cpp
 * @brief tz_util 네이티브 단위 테스트
 * @details 빌드: g++ -std=c++11 -I.. test_tz_util.cpp ../tz_util.cpp -o test_tz_util
 *          실행: ./test_tz_util
 * @note setenv("TZ", ...)로 나가는 값의 검증이므로, 허용되지 않는 문자가
 *       단 하나라도 통과하면 방어 실패다. 화이트리스트 누락 문자를 우선 검증한다.
 */
#include <cstdio>
#include <cstring>
#include "tz_util.h"

static int g_pass = 0;
static int g_fail = 0;

static void expectValid(const char* tz) {
    if (isValidTimezone(tz)) {
        g_pass++;
    } else {
        g_fail++;
        printf("  FAIL  \"%s\" 검증을 통과하지 못함 (허용되어야 함)\n", tz);
    }
}

static void expectInvalid(const char* tz) {
    if (!isValidTimezone(tz)) {
        g_pass++;
    } else {
        g_fail++;
        printf("  FAIL  \"%s\" 검증을 통과함 (거부되어야 함)\n", tz);
    }
}

static void testValidTimezones() {
    printf("수용해야 하는 POSIX TZ 문자열\n");
    // 웹 UI에 노출하는 12개 지역 + 기본값
    expectValid("KST-9");
    expectValid("JST-9");
    expectValid("CST-8");
    expectValid("GMT0BST,M3.5.0/1,M10.5.0");
    expectValid("CET-1CEST,M3.5.0,M10.5.0/3");
    expectValid("EST5EDT,M3.2.0,M11.1.0");
    expectValid("CST6CDT,M3.2.0,M11.1.0");
    expectValid("MST7MDT,M3.2.0,M11.1.0");
    expectValid("PST8PDT,M3.2.0,M11.1.0");
    expectValid("IST-5:30");          // 반시간권 (콜론)
    expectValid("AEST-10AEDT,M10.1.0,M4.1.0/3");
    expectValid("UTC0");
    expectValid("EST5");
    // '/'는 DST 경계 시각 구분자로 필요 (제거하면 유럽·호주 DST가 깨진다)
    expectValid("GMT0BST,M3.5.0/1,M10.5.0");
}

static void testInvalidTimezones() {
    printf("거부해야 하는 문자열\n");
    expectInvalid(NULL);              // NULL
    expectInvalid("");                // 빈 문자열

    // 화이트리스트 밖 문자 (setenv 인젝션 관점)
    expectInvalid("KST-9; rm -rf /");
    expectInvalid("KST-9\nEVIL=1");    // 개행으로 환경변수 주입
    expectInvalid("KST 9");           // 공백
    expectInvalid("KST_9");           // 밑줄
    expectInvalid("KST*9");           // 와일드카드
    expectInvalid("KST$9");           // 변수 확장
    expectInvalid("`id`");            // 백틱
    expectInvalid("$(id)");           // 명령치환
    expectInvalid("KST-9/../etc");    // 경로 이동 (tzfile 경로 주입)
    expectInvalid("AEST-10AEDT/../etc,M10.1.0");  // DST 규칙 부분에 경로 이동
    expectInvalid("KST-9\"quote");    // 따옴표
    expectInvalid("KST-9'quote");     // 작은따옴표
    expectInvalid("KST-9\\backslash");
    expectInvalid("KST-9|pipe");
    expectInvalid("KST-9&ampersand");
    expectInvalid("한글");            // 비ASCII

    // [bug fix 회귀] 과거 펌웨어의 JSON 파싱 오프셋 버그로 NVS에 저장된 유실된 값들.
    //   표준 약어가 3자 미만이 되면 거부되어 부팅 시 기본값(KST-9)으로 자가 치유된다
    //   (POSIX는 표준 약어 3자 이상을 요구한다).
    printf("표준 약어 3자 미만 (과거 버그로 유실된 형태)\n");
    expectInvalid("ST-9");                     // "KST-9"의 첫 글자 유실
    expectInvalid("ST5EDT,M3.2.0,M11.1.0");    // "EST5EDT,..."의 첫 글자 유실
    expectInvalid("T-5:30");                   // "IST-5:30"의 첫 글자 유실
    expectInvalid("C0");                       // "UTC0"의 첫 글자 유실
    expectInvalid("AB-5");                     // 2자 약어 (POSIX 규칙 위반)
}

static void testLengthBoundary() {
    printf("길이 경계 (최대 %d)\n", TIMEZONE_MAX_LEN);

    char exact[TIMEZONE_MAX_LEN + 1];
    char over[TIMEZONE_MAX_LEN + 2];

    // 길이 47 (TIMEZONE_MAX_LEN-1) → 최대 유효
    memset(exact, 'A', TIMEZONE_MAX_LEN - 1);
    exact[TIMEZONE_MAX_LEN - 1] = '\0';
    expectValid(exact);
    printf("  길이 %d (=%d-1) 수용\n", (int)strlen(exact), TIMEZONE_MAX_LEN);

    // 길이 48 (TIMEZONE_MAX_LEN) → 초과, 거부
    memset(over, 'A', TIMEZONE_MAX_LEN);
    over[TIMEZONE_MAX_LEN] = '\0';
    expectInvalid(over);
    printf("  길이 %d (=%d)   거부 (NUL 공간 확보)\n", (int)strlen(over), TIMEZONE_MAX_LEN);

    // 길이 3 ("KST") → 최소 유효 (POSIX 표준 약어 최소 3자)
    expectValid("KST");
    // 길이 1~2는 표준 약어 규칙 위반으로 거부된다
    expectInvalid("K");
    expectInvalid("KC");
}

int main() {
    printf("=== tz_util 단위 테스트 ===\n\n");
    testValidTimezones();
    testInvalidTimezones();
    testLengthBoundary();

    // strlen 기반 검증이므로 NUL 이후 바이트는 보이지 않는다는 것을 명시적으로 고정한다.
    // 웹 파싱은 항상 첫 NUL에서 끊기므로 방어가 성립한다. 이 단정이 깨지면
    // 검증 로직이 strlen 의존을 중단했다는 뜻이므로, 이 경우를 경고로 취급한다.
    char embeddedNul[16];
    memcpy(embeddedNul, "KST-9\0EVIL=1", 12);
    embeddedNul[12] = '\0';
    printf("\n내장 NUL 동작\n");
    if (isValidTimezone(embeddedNul)) {
        g_pass++;
        printf("  OK    \"KST-9\\0EVIL=1\" → strlen이 NUL에서 끊겨 \"KST-9\"로 처리됨\n");
    } else {
        g_fail++;
        printf("  FAIL  내장 NUL 처리 방식이 예상과 다름 — strlen 의존 재확인 필요\n");
    }

    printf("\n=== 결과: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
