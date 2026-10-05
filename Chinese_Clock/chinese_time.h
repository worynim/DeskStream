// worynim@gmail.com
/**
 * @file chinese_time.h
 * @brief 시간 → 중국어 텍스트 변환 클래스 (Arduino 래퍼)
 * @details 순수 로직은 chinese_time_core.h 참조. 이 클래스는 String 반환만 담당한다.
 * @note [SYNC] API 형태는 ENG_Clock/english_time.h를 따른다 — 호출부 구조가 흔들리지 않는다.
 *
 * @note **문자판은 uint8_t로 받는다.** config_manager의 NVS 값이 uint8_t이고,
 *       함수가 "0 아니면 번체"를 자동 판단하면 잘못된 값(2, 255)이 조용히 번체로 새어 나온다.
 *       따라서 유효하지 않은 값은 여기서 간체로 되돌린다 — 입력 검증은 이 경계의 책임이다.
 */
#ifndef CHINESE_TIME_H
#define CHINESE_TIME_H

#include <Arduino.h>

class ChineseTimeConverter {
public:
    /** 24시간제 시각 → "上午" (0~11) / "下午" (12~23) */
    static String getDayPart(int hour24, uint8_t script);

    /** 시 → 한자 ("三点" / "兩點"). 12시간제에서 0은 12로 센다. */
    static String getHour(int hour, bool is24h, uint8_t script);

    /** 분 → 한자 ("四十五分" / "整" / "半") */
    static String getMinute(int minute, uint8_t script);

    /** 초 → 한자 ("四十五秒" / "整" / "半") */
    static String getSecond(int second, uint8_t script);

    /**
     * @brief 요일 번호 → "星期X"
     * @param day 0=일요일 ~ 6=토요일 (tm_wday 규약)
     */
    static String getWeekday(int day, uint8_t script);

    /**
     * @brief 시 → 2자리 숫자 + 단위 ("13 时" / "13 時")
     * @details 숫자 모드에서 공백 한 칸을 넣어 어절을 만든다 (ENG판의 "02 H" 규칙 승계).
     *           레이아웃 엔진이 이 공백을 **빈 셀 한 칸**으로 소비한다 (PLAN §6.4).
     */
    static String getNumericHour(int hour, bool is24h, uint8_t script);

    /** 분 → 2자리 숫자 + "分" */
    static String getNumericMinute(int minute, uint8_t script);

    /** 초 → 2자리 숫자 + "秒" */
    static String getNumericSecond(int second, uint8_t script);
};

#endif