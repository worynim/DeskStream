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
