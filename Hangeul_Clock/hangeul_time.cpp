// worynim@gmail.com
/**
 * @file hangeul_time.cpp
 * @brief 시간-한글 텍스트 변환 로직 클래스 구현
 * @details 24시간제 한자어 수사 적용, 0분/0초 '정각' 처리 등 상세 시간 변환 로직 구현
 */
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

    // [리뷰 §3.1] 여기까지 오는 정상 입력은 두 가지뿐이다 — 12시간제는 이미 %12로 0~11이므로
    //   h==0이면 12로 바뀌었고, 24시간제는 위에서 0과 13+가 처리됐다. 즉 h는 1~12다.
    if (h >= 1 && h <= 12) return String(h_ones[h]) + "시";

    // hour가 0~23을 벗어난 경우의 최후 방어선. h_ones[h]로 인덱싱하면 배열을 넘어가므로
    //   배열 대신 숫자 문자열로 내려보낸다.
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

/**
 * @brief 분 단위 변환: 0분일 때 '정각' 반환
 * @note getSecond()도 0초에 "정각"을 반환한다. 0분0초에 두 화면이 동시에 "정각"이 되지
 *       않도록 Hangeul_Clock.ino가 화면3을 빈 문자열로 덮어쓴다. **이 분기를 제거하면
 *       그쪽 조건도 함께 수정해야 한다** (두 곳이 서로를 전제).
 */
String HangeulTimeConverter::getMinute(int minute) {
    if (minute == 0) return "정각";
    return convertToHangeul(minute, "분");
}

/**
 * @brief 초 단위 변환: 0초일 때 '정각' 반환
 * @note getMinute()의 "정각" 분기와 짝을 이룬다 — Hangeul_Clock.ino 참조.
 */
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
