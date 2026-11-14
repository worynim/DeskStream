// worynim@gmail.com
/**
 * @file tz_calc.h
 * @brief UTC 시각 → 시간대별 현지 시각 계산 (순수 로직, Arduino 의존 없음)
 * @details C 라이브러리의 setenv("TZ")+localtime() 은 전역 상태라 네 시간대를 번갈아 쓰기에 맞지 않는다.
 *          여기서는 시간대 규칙을 인자로 받아 계산만 한다 (시스템 TZ 는 건드리지 않는다).
 *          서머타임 규칙은 "몇 월 몇 번째 무슨 요일 몇 시" 형태만 다룬다.
 */
#ifndef TZ_CALC_H
#define TZ_CALC_H

#include <stdint.h>

/** 서머타임 전환 시점 하나: month 월의 week 번째(5 = 마지막) weekday 요일 + dayShift 일, 그날 0시에서 timeMin 분 뒤 */
struct DstEdge {
    uint8_t month;      ///< 1~12
    uint8_t week;       ///< 1~4 = 그 달의 n번째, 5 = 마지막
    uint8_t weekday;    ///< 0(일)~6(토)
    int8_t dayShift;    ///< 요일을 찾은 뒤 더할 날 수 (이스라엘: 마지막 일요일의 이틀 전 금요일 = -2)
    int16_t timeMin;    ///< 0시부터의 분 (1440 = 그날 밤 24시)
    bool timeIsUtc;     ///< true = timeMin 이 UTC 기준 (유럽), false = 전환 직전의 현지 시각 기준
};

/** 서머타임 규칙: 시작·끝 전환 시점. 서머타임 동안은 표준시보다 60분 빠르다 */
struct DstRule {
    DstEdge start;
    DstEdge end;
};

/** 시간대 하나: 표준시의 UTC 차이(분, 동쪽이 +)와 서머타임 규칙(없으면 nullptr) */
struct TzInfo {
    int16_t stdOffsetMin;
    const DstRule* dst;
};

/** 현지 시각 */
struct LocalTime {
    int16_t year;
    uint8_t month;          ///< 1~12
    uint8_t day;            ///< 1~31
    uint8_t hour, minute, second;
    uint8_t weekday;        ///< 0(일)~6(토)
    int16_t offsetMin;      ///< 지금 적용 중인 UTC 차이(분, 서머타임 반영)
    bool dst;               ///< 지금 서머타임 중
    int32_t dayNumber;      ///< 1970-01-01 을 0으로 센 현지 날짜 번호 (두 도시의 날짜 차이 계산용)
};

/** 그레고리력 날짜 → 1970-01-01 부터의 날 수 */
int32_t daysFromCivil(int year, int month, int day);

/** 그 달의 week 번째(5 = 마지막) weekday 요일이 며칠인지 */
int nthWeekdayOfMonth(int year, int month, int week, int weekday);

/** UTC epoch 초에 지금 적용되는 UTC 차이(분). isDst 가 nullptr 가 아니면 서머타임 여부를 돌려준다 */
int utcOffsetMinutes(int64_t utc, const TzInfo& tz, bool* isDst = nullptr);

/** UTC epoch 초 → 그 시간대의 현지 시각 */
LocalTime toLocalTime(int64_t utc, const TzInfo& tz);

#endif
