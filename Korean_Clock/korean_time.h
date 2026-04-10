#ifndef KOREAN_TIME_H
#define KOREAN_TIME_H

#include <Arduino.h>

class KoreanTimeConverter {
public:
    static String getAmPm(int hour) {
        return (hour < 12) ? "오전" : "오후";
    }

    static String getHour(int hour, bool is24h = false) {
        int h = is24h ? hour : (hour % 12);
        if (!is24h && h == 0) h = 12;
        if (is24h && h == 0) return "영시";

        const char* h_ones[] = {"", "한", "두", "세", "네", "다섯", "여섯", "일곱", "여덟", "아홉", "열", "열한", "열두"};
        
        if (h <= 12) return String(h_ones[h]) + "시";
        else if (h < 20) return "열" + String(h_ones[h-10]) + "시";
        else if (h == 20) return "스무시";
        else return "스물" + String(h_ones[h-20]) + "시";
    }

    static String getNumericHour(int hour, bool is24h) {
        int h = is24h ? hour : (hour % 12);
        if (!is24h && h == 0) h = 12;
        char buf[8]; sprintf(buf, "%02d시", h);
        return String(buf);
    }

    static String convertToKorean(int num, const String& unit) {
        if (num == 0) return "영" + unit;
        String result = "";
        int tens = num / 10;
        int ones = num % 10;
        const char* onesStr[] = {"", "일", "이", "삼", "사", "오", "육", "칠", "팔", "구"};
        const char* tensStr[] = {"", "십", "이십", "삼십", "사십", "오십"};
        if (tens > 0) result += tensStr[tens];
        result += onesStr[ones];
        result += unit;
        return result;
    }

    static String getMinute(int minute) { return convertToKorean(minute, "분"); }
    static String getSecond(int second) { return convertToKorean(second, "초"); }
    static String getDay(int day) { return convertToKorean(day, "일"); }

    static String getNumericMinute(int minute) {
        char buf[8]; sprintf(buf, "%02d분", minute);
        return String(buf);
    }

    static String getNumericSecond(int second) {
        char buf[8]; sprintf(buf, "%02d초", second);
        return String(buf);
    }

    static String getNumericDay(int day) {
        return String(day) + "일";
    }
};

#endif
