#ifndef KOREAN_TIME_H
#define KOREAN_TIME_H

#include <Arduino.h>

class KoreanTimeConverter {
public:
    static String getAmPm(int hour) {
        return (hour < 12) ? "오전" : "오후";
    }

    static String getHour(int hour) {
        int h = hour % 12;
        if (h == 0) h = 12;

        const char* hours[] = {"", "한시", "두시", "세시", "네시", "다섯시", "여섯시", "일곱시", "여덟시", "아홉시", "열시", "열한시", "열두시"};
        return String(hours[h]);
    }

    static String convertToKorean(int num, const String& unit) {
        if (num == 0) return "영" + unit;
        
        String result = "";
        int tens = num / 10;
        int ones = num % 10;

        const char* onesStr[] = {"", "일", "이", "삼", "사", "오", "육", "칠", "팔", "구"};
        const char* tensStr[] = {"", "십", "이십", "삼십", "사십", "오십"};

        if (tens > 0) {
            result += tensStr[tens];
        }
        
        result += onesStr[ones];
        result += unit;
        
        return result;
    }

    static String getMinute(int minute) {
        return convertToKorean(minute, "분");
    }

    static String getSecond(int second) {
        return convertToKorean(second, "초");
    }
};

#endif
