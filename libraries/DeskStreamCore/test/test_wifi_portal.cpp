// worynim@gmail.com
/**
 * @file test_wifi_portal.cpp
 * @brief WiFi 설정 포털 입력 검증·HTML 이스케이프 테스트
 * @details 실행: bash test/run_all.sh
 * @note 고정하는 것: 주변 SSID에 섞인 <script>·따옴표가 페이지에 그대로 들어가지 않는다,
 *       SSID 1~32바이트·비밀번호 0 또는 8~63자, 제어문자 거부, 버퍼가 모자라면 거부.
 */
#include <cstdio>
#include <cstring>
#include "net/ds_wifi_portal_util.h"

using namespace ds;

static int g_pass = 0, g_fail = 0;
static void check(const char* label, bool ok) {
    if (ok) g_pass++;
    else { g_fail++; printf("  FAIL  %s\n", label); }
}

int main() {
    // --- SSID ---
    check("SSID 일반", wifiSsidValid("MyHome_2.4G"));
    check("SSID 한글(UTF-8)", wifiSsidValid("우리집 와이파이"));
    check("SSID 32바이트 허용", wifiSsidValid("ABCDEFGHIJKLMNOPQRSTUVWXYZ012345"));
    check("SSID 33바이트 거부", !wifiSsidValid("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456"));
    check("SSID 빈 문자열 거부", !wifiSsidValid(""));
    check("SSID NULL 거부", !wifiSsidValid(nullptr));
    check("SSID 개행 거부", !wifiSsidValid("a\nb"));

    // --- 비밀번호 ---
    check("비번 빈 문자열(열린 네트워크) 허용", wifiPassValid(""));
    check("비번 8자 허용", wifiPassValid("12345678"));
    check("비번 7자 거부", !wifiPassValid("1234567"));
    check("비번 63자 허용", wifiPassValid("123456789012345678901234567890123456789012345678901234567890123"));
    check("비번 64자 거부", !wifiPassValid("1234567890123456789012345678901234567890123456789012345678901234"));
    check("비번 제어문자 거부", !wifiPassValid("abcd\tefgh"));

    // --- HTML 이스케이프 ---
    char out[128];
    check("<script> 무력화",
          htmlEscapeTo("<script>alert(1)</script>", out, sizeof(out))
              && strcmp(out, "&lt;script&gt;alert(1)&lt;/script&gt;") == 0);
    check("속성값 탈출 문자 \" ' 무력화",
          htmlEscapeTo("a\"b'c", out, sizeof(out)) && strcmp(out, "a&quot;b&#39;c") == 0);
    check("& 이스케이프", htmlEscapeTo("A&B", out, sizeof(out)) && strcmp(out, "A&amp;B") == 0);
    check("UTF-8 그대로", htmlEscapeTo("한글", out, sizeof(out)) && strcmp(out, "한글") == 0);
    check("빈 문자열", htmlEscapeTo("", out, sizeof(out)) && out[0] == '\0');
    char small[6];
    check("버퍼 정확히 맞음 (\"&amp;\" 5바이트 + NUL)", htmlEscapeTo("&", small, sizeof(small)) && strcmp(small, "&amp;") == 0);
    check("버퍼 모자라면 거부", !htmlEscapeTo("&&", small, sizeof(small)));

    printf("test_wifi_portal: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
