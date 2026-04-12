#include "hangeul_time.h"

String HangeulTimeConverter::getAmPm(int hour) {
    return (hour < 12) ? "오전" : "오후";
}

String HangeulTimeConverter::getHour(int hour, bool is24h) {
    int h = is24h ? hour : (hour % 12);
    if (!is24h && h == 0) h = 12;
    if (is24h && h == 0) return "영시";

    // 24시간제이면서 13시 이상인 경우 한자어 수사 적용 (사용자 요청)
    if (is24h && h >= 13) {
        return convertToHangeul(h, "시");
    }

    // 1~12시 구간은 고유어 수사 사용
    const char* h_ones[] = {"", "한", "두", "세", "네", "다섯", "여섯", "일곱", "여덟", "아홉", "열", "열한", "열두"};
    if (h <= 12) return String(h_ones[h]) + "시";
    
    // 24시간제가 아닐 경우 13시 이상은 발생하지 않음 (이미 %12 처리)
    return String(h) + "시";
}

String HangeulTimeConverter::getNumericHour(int hour, bool is24h) {
    int h = is24h ? hour : (hour % 12);
    if (!is24h && h == 0) h = 12;
    char buf[16]; sprintf(buf, "%02d시", h);
    return String(buf);
}

String HangeulTimeConverter::convertToHangeul(int num, const String& unit) {
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

String HangeulTimeConverter::getMinute(int minute) { 
    if (minute == 0) return "정각";
    return convertToHangeul(minute, "분"); 
}

String HangeulTimeConverter::getSecond(int second) { 
    if (second == 0) return "정각";
    return convertToHangeul(second, "초"); 
}
String HangeulTimeConverter::getDay(int day) { return convertToHangeul(day, "일"); }

String HangeulTimeConverter::getNumericMinute(int minute) {
    char buf[16]; sprintf(buf, "%02d분", minute);
    return String(buf);
}

String HangeulTimeConverter::getNumericSecond(int second) {
    char buf[16]; sprintf(buf, "%02d초", second);
    return String(buf);
}

String HangeulTimeConverter::getNumericDay(int day) {
    return String(day) + "일";
}
