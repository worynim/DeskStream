// worynim@gmail.com
/**
 * @file chinese_time.cpp
 * @brief 시간 → 중국어 텍스트 변환 (Arduino 래퍼)
 * @details 순수 로직과 분리된 이유: chinese_time_core만 네이티브 g++로 단위 테스트 가능하다.
 *          (AGENTS.md 테스트 규칙 — "새 코드에는 새 테스트")
 */
#include "chinese_time.h"
#include "chinese_time_core.h"
#include "config.h"

namespace {

/**
 * @brief NVS의 uint8_t 값을 유효한 Script로 정규화한다
 * @details 입력 검증은 이 경계에서 한 번만 한다. config_manager도 같은 규칙으로
 *          기본값 복구를 하므로 여기서는 방어적으로 정규화만 수행한다.
 */
chtime::Script toScript(uint8_t s) {
    return (s == SCRIPT_TRADITIONAL) ? chtime::CHT_SCRIPT_TRADITIONAL : chtime::CHT_SCRIPT_SIMPLIFIED;
}

/** 순수 코어를 호출해 String으로 변환. 실패 시 빈 String. */
String wrapUnary(bool (*fn)(int, chtime::Script, char*, size_t), int arg, uint8_t script) {
    char buf[CHT_BUF_SIZE];
    if (!fn(arg, toScript(script), buf, sizeof(buf))) return String("");
    return String(buf);
}

/**
 * @brief 문자판을 받지 않는 단일 인자 변환 함수를 호출해 String으로 변환
 * @details 숫자 모드의 twoDigit()이这条路를 탄다 — `零一二…九`는 두 문자판이
 *           **완전히 동일**하므로 Script 인자를 받을 이유가 없다.
 * @note wrapUnary()와 분리한 이유: 함수 포인터 타입이 다르므로 하나로 합칠 수 없다.
 *       (core의 Script 인자 유무가 함수마다 달라 한 타입으로는 표현되지 않는다)
 */
String wrapPlain(bool (*fn)(int, char*, size_t), int arg) {
    char buf[CHT_BUF_SIZE];
    if (!fn(arg, buf, sizeof(buf))) return String("");
    return String(buf);
}

/**
 * @brief 결과에 단위 접미를 붙인다 (숫자 모드)
 * @param simplifiedUnit  간체 단위 ("时")
 * @param traditionalUnit 번체 단위 ("時") — 두 문자판이 같으면 nullptr
 * @details 접미 사이의 간격 한 칸이 ENG판 "02 H" 규칙의 승계다.
 *           레이아웃 엔진이 이 공백을 빈 셀 한 칸으로 소비한다 (PLAN §6.4).
 */
String withUnit(const String& digits, const char* simplifiedUnit,
                const char* traditionalUnit, uint8_t script) {
    if (digits.length() == 0) return String("");
    const char* unit = simplifiedUnit;
    if (traditionalUnit && toScript(script) == chtime::CHT_SCRIPT_TRADITIONAL) unit = traditionalUnit;
    return digits + " " + unit;
}

} // namespace

String ChineseTimeConverter::getDayPart(int hour24, uint8_t script) {
    return wrapUnary(chtime::dayPartToChars, hour24, script);
}

String ChineseTimeConverter::getHour(int hour, bool is24h, uint8_t script) {
    char buf[CHT_BUF_SIZE];
    if (!chtime::hourToChars(hour, is24h, toScript(script), buf, sizeof(buf))) return String("");
    return String(buf);
}

String ChineseTimeConverter::getMinute(int minute, uint8_t script) {
    return wrapUnary(chtime::minuteToChars, minute, script);
}

String ChineseTimeConverter::getSecond(int second, uint8_t script) {
    return wrapUnary(chtime::secondToChars, second, script);
}

String ChineseTimeConverter::getWeekday(int day, uint8_t script) {
    return wrapUnary(chtime::weekdayToChars, day, script);
}

String ChineseTimeConverter::getNumericHour(int hour, bool is24h, uint8_t script) {
    int h = is24h ? hour : (hour % 12 == 0 ? 12 : hour % 12);
    return withUnit(wrapPlain(chtime::twoDigit, h), "时", "時", script);
}

String ChineseTimeConverter::getNumericMinute(int minute, uint8_t script) {
    return withUnit(wrapPlain(chtime::twoDigit, minute), "分", nullptr, script);
}

String ChineseTimeConverter::getNumericSecond(int second, uint8_t script) {
    return withUnit(wrapPlain(chtime::twoDigit, second), "秒", nullptr, script);
}