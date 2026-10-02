// worynim@gmail.com
/**
 * @file english_time_core.cpp
 * @brief 시간 → 영어 텍스트 변환 순수 로직 구현
 * @details 영어 숫자는 0~59까지 전부 규칙적이므로 한글판의 고유어/한자어 수사 분기가 불필요하다.
 */
#include "english_time_core.h"
#include <string.h>
#include <stdio.h>

namespace engtime {

// 0~19. 영어는 19까지 개별 단어가 있다 (FIFTEEN 등).
static const char* const SMALL[] = {
    "ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE",
    "TEN", "ELEVEN", "TWELVE", "THIRTEEN", "FOURTEEN", "FIFTEEN",
    "SIXTEEN", "SEVENTEEN", "EIGHTEEN", "NINETEEN"
};
static const int SMALL_COUNT = (int)(sizeof(SMALL) / sizeof(SMALL[0]));

// 20~50. 인덱스 2~5만 사용.
static const char* const TENS[] = { "", "", "TWENTY", "THIRTY", "FORTY", "FIFTY" };
static const int TENS_COUNT = (int)(sizeof(TENS) / sizeof(TENS[0]));

/** 상수 문자열을 호출자 버퍼로 안전 복사. 용량 부족 시 실패. */
static bool copyOut(const char* src, char* buf, size_t cap) {
    if (!buf || cap == 0) return false;
    buf[0] = '\0';
    size_t n = strlen(src);
    if (n >= cap) return false;
    memcpy(buf, src, n + 1);
    return true;
}

bool isValidMinute(int n) { return n >= 0 && n <= 59; }
bool isValidHour12(int n) { return n >= 1 && n <= 12; }
bool isValidHour24(int n) { return n >= 0 && n <= 23; }

bool joinWords(const char* a, const char* b, char* buf, size_t cap) {
    if (!buf || cap == 0) return false;
    buf[0] = '\0';
    if (!a || !b || a[0] == '\0' || b[0] == '\0') return false;
    char tmp[ENG_BUF_SIZE * 2];
    int written = snprintf(tmp, sizeof(tmp), "%s %s", a, b);
    if (written < 0 || (size_t)written >= sizeof(tmp)) return false;
    return copyOut(tmp, buf, cap);
}

bool numberToWords(int n, char* buf, size_t cap) {
    if (!buf || cap == 0) return false;
    buf[0] = '\0';
    if (!isValidMinute(n)) return false;

    if (n < SMALL_COUNT) return copyOut(SMALL[n], buf, cap);

    int tens = n / 10;
    int ones = n % 10;
    if (tens < 0 || tens >= TENS_COUNT) return false;
    if (ones == 0) return copyOut(TENS[tens], buf, cap);   // 30 → "THIRTY"
    return joinWords(TENS[tens], SMALL[ones], buf, cap);   // 37 → "THIRTY SEVEN"
}

bool hourToWords(int hour, bool is24h, char* buf, size_t cap) {
    if (!buf || cap == 0) return false;
    buf[0] = '\0';
    if (!isValidHour24(hour)) return false;

    if (is24h) return numberToWords(hour, buf, cap);   // 0 → "ZERO"

    int h = hour % 12;                                  // 12시간제: 0시 → 12
    if (h == 0) h = 12;
    return numberToWords(h, buf, cap);
}

bool minuteToWords(int minute, char* buf, size_t cap) {
    if (!buf || cap == 0) return false;
    buf[0] = '\0';
    if (!isValidMinute(minute)) return false;
    if (minute == 0) return copyOut("O'CLOCK", buf, cap);
    return numberToWords(minute, buf, cap);
}

bool secondToWords(int second, char* buf, size_t cap) {
    if (!buf || cap == 0) return false;
    buf[0] = '\0';
    if (!isValidMinute(second)) return false;
    if (second == 0) return copyOut("O'CLOCK", buf, cap);
    return numberToWords(second, buf, cap);
}

bool amPm(int hour24, char* buf, size_t cap) {
    if (!buf || cap == 0) return false;
    buf[0] = '\0';
    if (!isValidHour24(hour24)) return false;
    return copyOut((hour24 < 12) ? "AM" : "PM", buf, cap);
}

bool twoDigit(int n, char* buf, size_t cap) {
    if (!buf || cap == 0) return false;
    buf[0] = '\0';
    if (!isValidMinute(n)) return false;
    char tmp[4];
    snprintf(tmp, sizeof(tmp), "%02d", n);
    return copyOut(tmp, buf, cap);
}

// 0=일요일 ~ 6=토요일 (tm_wday 규약). 최장 "WEDNESDAY" = 9자 (1줄 한계와 동일).
static const char* const DAYS[] = {
    "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"
};
static const int DAYS_COUNT = (int)(sizeof(DAYS) / sizeof(DAYS[0]));

bool dayName(int day, char* buf, size_t cap) {
    if (!buf || cap == 0) return false;
    buf[0] = '\0';
    if (day < 0 || day >= DAYS_COUNT) return false;
    return copyOut(DAYS[day], buf, cap);
}

bool dateString(int mon, int mday, bool monthFirst, char* buf, size_t cap) {
    if (!buf || cap == 0) return false;
    buf[0] = '\0';
    // tm_mon+1 / tm_mday 규약 검증. 범위 밖 값은 빈 문자열로 남긴다.
    if (mon < 1 || mon > 12) return false;
    if (mday < 1 || mday > 31) return false;

    char tmp[8];   // 최장 "31/12" = 5자 + NUL
    int written = monthFirst ? snprintf(tmp, sizeof(tmp), "%d/%d", mon, mday)
                             : snprintf(tmp, sizeof(tmp), "%d/%d", mday, mon);
    if (written < 0 || (size_t)written >= sizeof(tmp)) return false;
    return copyOut(tmp, buf, cap);
}

} // namespace engtime
