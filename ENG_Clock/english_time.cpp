// worynim@gmail.com
/**
 * @file english_time.cpp
 * @brief 시간 → 영어 텍스트 변환 (Arduino 래퍼)
 * @details 순수 로직과 분리된 이유: english_time_core만 네이티브 g++로 단위 테스트 가능하다.
 */
#include "english_time.h"
#include "english_time_core.h"
#include "config.h"

namespace {

/** (int → char*) 순수 코어를 호출해 String으로 변환. 실패 시 빈 String. */
String wrapUnary(bool (*fn)(int, char*, size_t), int arg) {
    char buf[ENG_BUF_SIZE];
    if (!fn(arg, buf, sizeof(buf))) return String("");
    return String(buf);
}

String wrapHour(int hour, bool is24h) {
    char buf[ENG_BUF_SIZE];
    if (!engtime::hourToWords(hour, is24h, buf, sizeof(buf))) return String("");
    return String(buf);
}

} // namespace

String EnglishTimeConverter::getAmPm(int hour) {
    return wrapUnary(engtime::amPm, hour);
}

String EnglishTimeConverter::getHour(int hour, bool is24h) {
    return wrapHour(hour, is24h);
}

String EnglishTimeConverter::getNumericHour(int hour, bool is24h) {
    int h = is24h ? hour : (hour % 12 == 0 ? 12 : hour % 12);
    return wrapUnary(engtime::twoDigit, h);
}

String EnglishTimeConverter::getMinute(int minute) {
    return wrapUnary(engtime::minuteToWords, minute);
}

String EnglishTimeConverter::getSecond(int second) {
    return wrapUnary(engtime::secondToWords, second);
}

String EnglishTimeConverter::getDay(int day) {
    return wrapUnary(engtime::dayName, day);
}

String EnglishTimeConverter::getNumericMinute(int minute) {
    return wrapUnary(engtime::twoDigit, minute);
}

String EnglishTimeConverter::getNumericSecond(int second) {
    return wrapUnary(engtime::twoDigit, second);
}

String EnglishTimeConverter::getDate(int mon, int mday, uint8_t dateOrder) {
    char buf[ENG_BUF_SIZE];
    bool monthFirst = (dateOrder == DATE_ORDER_MONTH_DAY);
    if (!engtime::dateString(mon, mday, monthFirst, buf, sizeof(buf))) return String("");
    return String(buf);
}
