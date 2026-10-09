// worynim@gmail.com
/**
 * @file hangeul_time.h
 * @brief 시간-한글 텍스트 변환 로직 클래스 정의
 * @details NTP 시간을 한글 수사(고유어/한자어) 및 숫자 모드로 변환하는 유틸리티 관리
 */
#ifndef HANGEUL_TIME_H
#define HANGEUL_TIME_H

#include <Arduino.h>

class HangeulTimeConverter {
public:
    static String getAmPm(int hour);
    static String getHour(int hour, bool is24h = false);
    static String getNumericHour(int hour, bool is24h);
    static String convertToHangeul(int num, const String& unit);
    static String getMinute(int minute);
    static String getSecond(int second);
    static String getDay(int day);
    static String getNumericMinute(int minute);
    static String getNumericSecond(int second);
    static String getNumericDay(int day);
};

#endif
