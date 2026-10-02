// worynim@gmail.com
/**
 * @file english_time.h
 * @brief 시간 → 영어 텍스트 변환 클래스 (Arduino 래퍼)
 * @details 순수 로직은 english_time_core.h 참조. 이 클래스는 String 반환만 담당한다.
 */
#ifndef ENGLISH_TIME_H
#define ENGLISH_TIME_H

#include <Arduino.h>

class EnglishTimeConverter {
public:
    /** 24시간제 시각 → "AM" / "PM" */
    static String getAmPm(int hour);

    /** 시 → 영어 단어 (12/24시간제) */
    static String getHour(int hour, bool is24h = false);

    /** 시 → 2자리 숫자 (12/24시간제) */
    static String getNumericHour(int hour, bool is24h);

    /** 분 → 영어 단어 (0은 "O'CLOCK") */
    static String getMinute(int minute);

    /** 초 → 영어 단어 (0은 "O'CLOCK") */
    static String getSecond(int second);

    /** 일 → 영어 단어 (1~7) */
    static String getDay(int day);

    /** 분 → 2자리 숫자 */
    static String getNumericMinute(int minute);

    /** 초 → 2자리 숫자 */
    static String getNumericSecond(int second);

    /**
     * @brief 날짜 → "M/D" (선행 0 없음)
     * @param mon        월 1~12
     * @param mday       일 1~31
     * @param dateOrder  DATE_ORDER_DAY_MONTH = "2/10", DATE_ORDER_MONTH_DAY = "10/2"
     */
    static String getDate(int mon, int mday, uint8_t dateOrder);
};

#endif
