// worynim@gmail.com
/**
 * @file tz_util.cpp
 * @brief POSIX TZ 문자열 검증 구현
 */
#include "tz_util.h"
#include <string.h>

bool isValidTimezone(const char* tz) {
    if (tz == NULL) return false;

    size_t len = strlen(tz);
    if (len == 0 || len >= TIMEZONE_MAX_LEN) return false;

    // POSIX 표준 약어는 최소 3자 이상의 알파벳이어야 한다 (예: "KST-9"의 "KST").
    // 이 검사가 없으면 과거 펌웨어 버그로 첫 글자가 유실된 NVS 값("ST-9" 등)까지
    // 수용되어 부팅 시간이 UTC로 표시된다. 최소 3자 검사로 그런 값은 자동으로
    // 기본값(KST-9)으로 되돌아가 자가 치유된다.
    size_t stdChars = 0;
    while (stdChars < len && ((tz[stdChars] >= 'A' && tz[stdChars] <= 'Z') ||
                              (tz[stdChars] >= 'a' && tz[stdChars] <= 'z'))) {
        stdChars++;
    }
    if (stdChars < 3) return false;

    for (size_t i = 0; i < len; i++) {
        char c = tz[i];
        bool ok = (c >= 'A' && c <= 'Z') ||
                  (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') ||
                  c == '+' || c == '-' || c == ':' ||
                  c == ',' || c == '.' || c == '/';
        if (!ok) return false;
    }

    // '/'는 DST 경계 시각 구분자로 필요하다 (예: "M3.5.0/1" = 3월 마지막 일요일 1시).
    // tzdata가 이를 tzfile 경로로 해석할 수 있으므로 경로 이동("..")은 금지한다.
    if (strstr(tz, "..") != NULL) return false;

    return true;
}
