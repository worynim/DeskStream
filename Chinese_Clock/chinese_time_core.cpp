// worynim@gmail.com
/**
 * @file chinese_time_core.cpp
 * @brief 시간 → 중국어 텍스트 변환 순수 로직 구현
 * @details 규칙의 근거는 PLAN.md §2.2, 정답 표는 §3이다. 이 구현은 §3을 그대로 따른다.
 */
#include "chinese_time_core.h"
#include <string.h>
#include <stdio.h>

namespace chtime {

// 0~9. 10은 별도("十")로 처리한다.
static const char* const DIGITS[] = {
    "零", "一", "二", "三", "四", "五", "六", "七", "八", "九"
};
static const int DIGITS_COUNT = (int)(sizeof(DIGITS) / sizeof(DIGITS[0]));

// 요일 0=일 ~ 6=토. 간체·번체가 같다.
static const char* const WEEKDAY_CHARS[] = {
    "日", "一", "二", "三", "四", "五", "六"
};
static const int WEEKDAY_COUNT = (int)(sizeof(WEEKDAY_CHARS) / sizeof(WEEKDAY_CHARS[0]));

// === 번체 치환표 — 코드의 유일한 분기 지점 (PLAN §2.3) ===
// 전체 문자열을 2벌로 두면 두 벌이 어긋날 수 있으므로, 다른 3글자만 조회한다.
// 웹 Font Studio의 치환 규칙과 1:1이어야 하며, firmware_wiring_test.mjs가 정적으로 대조한다.
static const char* const TRADITIONAL_MAP[][2] = {
    { "点", "點" },   // 시 단위
    { "时", "時" },   // 시각 단위 (숫자 모드)
    { "两", "兩" },   // 2
};

size_t scriptOnlyCount(Script s) {
    (void)s;   // 두 문자판 모두 치환표 크기와 같다 — 어느 쪽이든 3
    return sizeof(TRADITIONAL_MAP) / sizeof(TRADITIONAL_MAP[0]);
}

const char* scriptOnlyGlyph(Script s, size_t i) {
    if (i >= scriptOnlyCount(s)) return nullptr;
    return (s == CHT_SCRIPT_TRADITIONAL) ? TRADITIONAL_MAP[i][1] : TRADITIONAL_MAP[i][0];
}

/**
 * @brief 간체 글자를 문자판에 맞게 변환
 * @details 치환 표에 없는 글자는 그대로 둔다. 나머지 31글자는 두 문자판이 동일하므로
 *          표에 없는 것이 오류가 아니라 정상이다.
 */
static const char* convertChar(const char* simplified, Script s) {
    if (s == CHT_SCRIPT_SIMPLIFIED) return simplified;
    for (size_t i = 0; i < sizeof(TRADITIONAL_MAP) / sizeof(TRADITIONAL_MAP[0]); i++) {
        if (strcmp(TRADITIONAL_MAP[i][0], simplified) == 0) return TRADITIONAL_MAP[i][1];
    }
    return simplified;
}

/** 상수 문자열을 호출자 버퍼로 안전 복사. 용량 부족 시 실패. */
static bool copyOut(const char* src, char* buf, size_t cap) {
    if (!buf || cap == 0) return false;
    buf[0] = '\0';
    size_t n = strlen(src);
    if (n >= cap) return false;
    memcpy(buf, src, n + 1);
    return true;
}

/**
 * @brief 문자열 조각들을 공백 없이 이어 붙인다
 * @param parts 조각 배열, n은 개수. 빈 조각은 그대로 건너뛴다.
 */
static bool joinParts(const char* const* parts, int n, char* buf, size_t cap) {
    if (!buf || cap == 0) return false;
    buf[0] = '\0';
    size_t used = 0;
    for (int i = 0; i < n; i++) {
        if (!parts[i] || parts[i][0] == '\0') continue;
        size_t len = strlen(parts[i]);
        if (used + len + 1 > cap) { buf[0] = '\0'; return false; }
        memcpy(buf + used, parts[i], len);
        used += len;
        buf[used] = '\0';
    }
    return true;
}

bool isValidMinute(int n) { return n >= 0 && n <= 59; }
bool isValidHour12(int n) { return n >= 1 && n <= 12; }
bool isValidHour24(int n) { return n >= 0 && n <= 23; }

/**
 * @brief 0~59를 한자 수사로 조립 (십의 자리 + "十" + 일의 자리)
 * @details 십의 자리가 1이면 "一"을 생략한다 — 15는 "十五"이지 "一十五"가 아니다.
 * @note 0~59의 십의 자리·일의 자리는 간체/번체가 동일하므로 문자판을 받지 않는다.
 *       유일하게 다른 글자 `两`는 시(context)에서만 쓰이고 아래 hourToChars()가 처리한다.
 */
static bool buildNumber(int n, char* buf, size_t cap) {
    if (n < 10) return copyOut(DIGITS[n], buf, cap);

    int tens = n / 10;
    int ones = n % 10;
    if (tens < 1 || tens >= DIGITS_COUNT) { if (buf && cap) buf[0] = '\0'; return false; }

    const char* tenChar = (tens == 1) ? "" : DIGITS[tens];
    const char* oneChar = (ones == 0) ? "" : DIGITS[ones];

    const char* parts[3] = { tenChar, "十", oneChar };
    return joinParts(parts, 3, buf, cap);
}

bool numberToChars(int n, Script s, char* buf, size_t cap) {
    if (!buf || cap == 0) return false;
    buf[0] = '\0';
    if (!isValidMinute(n)) return false;
    (void)s;   // 0~59의 십의/일의 자리는 두 문자판이 동일하다
    return buildNumber(n, buf, cap);
}

bool hourToChars(int hour, bool is24h, Script s, char* buf, size_t cap) {
    if (!buf || cap == 0) return false;
    buf[0] = '\0';
    if (!isValidHour24(hour)) return false;

    // 12시간제에서 0시(=자정)는 12시로 센다.
    int h = hour;
    if (!is24h) { h = hour % 12; if (h == 0) h = 12; }

    // 규칙 ②: **단독 2**는 `二`가 아니라 `两`다 (PLAN §3.1·§3.2).
    //   20~23의 십의 자리는 `二`가 유지된다 ("二十一点", "二十三点" — §3.2).
    char numeral[CHT_BUF_SIZE];
    bool ok;
    if (h == 2) ok = copyOut(convertChar("两", s), numeral, sizeof(numeral));
    else        ok = buildNumber(h, numeral, sizeof(numeral));
    if (!ok) { buf[0] = '\0'; return false; }

    const char* parts[2] = { numeral, convertChar("点", s) };
    return joinParts(parts, 2, buf, cap);
}

/**
 * @brief 분/초 공통 변환 (접미 글자만 다르다 — PLAN 규칙 ⑧)
 * @param suffix "분" 또는 "秒" (두 문자판이 동일하므로 문자판을 받지 않는다)
 */
static bool msToChars(int v, const char* suffix, char* buf, size_t cap) {
    if (!buf || cap == 0) return false;
    buf[0] = '\0';
    if (!isValidMinute(v)) return false;

    // 규칙 ⑤ 정각은 "整". 규칙 ④ 30분은 정확히 "半"만 쓴다 (31분은 "三十一分"으로 되돌아간다).
    if (v == 0) return copyOut("整", buf, cap);
    if (v == 30) return copyOut("半", buf, cap);

    // 규칙 ③: 10분 미만은 십의 자리에 `零`을 넣는다. "三点五分"은 3:5와 3:35로 오독된다.
    const char* lead = (v < 10) ? "零" : "";

    char body[CHT_BUF_SIZE];
    if (!buildNumber(v, body, sizeof(body))) { buf[0] = '\0'; return false; }

    const char* parts[3] = { lead, body, suffix };
    return joinParts(parts, 3, buf, cap);
}

bool minuteToChars(int minute, Script s, char* buf, size_t cap) {
    (void)s;   // "분"은 두 문자판이 동일하다
    return msToChars(minute, "分", buf, cap);
}

bool secondToChars(int second, Script s, char* buf, size_t cap) {
    (void)s;   // "秒"는 두 문자판이 동일하다
    return msToChars(second, "秒", buf, cap);
}

bool weekdayToChars(int day, Script s, char* buf, size_t cap) {
    if (!buf || cap == 0) return false;
    buf[0] = '\0';
    if (day < 0 || day >= WEEKDAY_COUNT) return false;
    (void)s;   // "星期X"의 7글자는 두 문자판이 동일하다

    const char* parts[2] = { "星期", WEEKDAY_CHARS[day] };
    return joinParts(parts, 2, buf, cap);
}

bool dayPartToChars(int hour24, Script s, char* buf, size_t cap) {
    if (!buf || cap == 0) return false;
    buf[0] = '\0';
    if (!isValidHour24(hour24)) return false;
    (void)s;   // "上午"/"下午"는 두 문자판이 동일하다

    // 자정(0시)이 上午인 것은 중국 실무 관행과 같으므로 예외를 두지 않는다 (PLAN §2.2 ⑥).
    return copyOut((hour24 < 12) ? "上午" : "下午", buf, cap);
}

bool twoDigit(int n, char* buf, size_t cap) {
    if (!buf || cap == 0) return false;
    buf[0] = '\0';
    if (!isValidMinute(n)) return false;
    char tmp[4];
    snprintf(tmp, sizeof(tmp), "%02d", n);
    return copyOut(tmp, buf, cap);
}

} // namespace chtime