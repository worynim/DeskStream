// worynim@gmail.com
/**
 * @file tz_calc.cpp
 * @brief UTC 시각 → 시간대별 현지 시각 계산 구현
 */
#include "tz_calc.h"

namespace {

const int kDstSavingMin = 60;

/** 음수에서도 내림하는 나눗셈 */
int64_t floorDiv(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }

/** 1970-01-01 부터의 날 수 → 연·월·일 (Howard Hinnant 의 civil_from_days) */
void civilFromDays(int32_t days, int* year, int* month, int* day) {
    const int64_t z = (int64_t)days + 719468;
    const int64_t era = floorDiv(z, 146097);
    const int doe = (int)(z - era * 146097);
    const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int mp = (5 * doy + 2) / 153;
    *day = doy - (153 * mp + 2) / 5 + 1;
    *month = mp < 10 ? mp + 3 : mp - 9;
    *year = (int)(yoe + era * 400) + (*month <= 2 ? 1 : 0);
}

int weekdayOfDays(int32_t days) { return (int)(((days % 7) + 11) % 7); }   // 1970-01-01 = 목요일(4)

int daysInMonth(int year, int month) {
    return daysFromCivil(month == 12 ? year + 1 : year, month == 12 ? 1 : month + 1, 1) - daysFromCivil(year, month, 1);
}

/** 전환 시점의 UTC epoch 초. offsetBeforeMin = 전환 직전에 적용되던 UTC 차이 */
int64_t edgeUtc(const DstEdge& e, int year, int offsetBeforeMin) {
    const int day = nthWeekdayOfMonth(year, e.month, e.week, e.weekday);
    const int64_t wall = ((int64_t)daysFromCivil(year, e.month, day) + e.dayShift) * 86400 + (int64_t)e.timeMin * 60;
    return e.timeIsUtc ? wall : wall - (int64_t)offsetBeforeMin * 60;
}

}  // namespace

int32_t daysFromCivil(int year, int month, int day) {
    year -= month <= 2 ? 1 : 0;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const int yoe = year - era * 400;
    const int doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int nthWeekdayOfMonth(int year, int month, int week, int weekday) {
    const int firstWd = weekdayOfDays(daysFromCivil(year, month, 1));
    int day = 1 + (weekday - firstWd + 7) % 7 + (week - 1) * 7;
    while (day > daysInMonth(year, month)) day -= 7;      // week = 5 → 마지막 주
    return day;
}

int utcOffsetMinutes(int64_t utc, const TzInfo& tz, bool* isDst) {
    bool dst = false;
    if (tz.dst) {
        int year, month, day;
        civilFromDays((int32_t)floorDiv(utc + (int64_t)tz.stdOffsetMin * 60, 86400), &year, &month, &day);
        const int64_t start = edgeUtc(tz.dst->start, year, tz.stdOffsetMin);
        const int64_t end = edgeUtc(tz.dst->end, year, tz.stdOffsetMin + kDstSavingMin);
        // 북반구는 시작 < 끝(한 해 안), 남반구는 끝 < 시작(해를 넘긴다)
        dst = start < end ? (utc >= start && utc < end) : (utc >= start || utc < end);
    }
    if (isDst) *isDst = dst;
    return tz.stdOffsetMin + (dst ? kDstSavingMin : 0);
}

LocalTime toLocalTime(int64_t utc, const TzInfo& tz) {
    LocalTime t;
    t.offsetMin = (int16_t)utcOffsetMinutes(utc, tz, &t.dst);
    const int64_t local = utc + (int64_t)t.offsetMin * 60;
    t.dayNumber = (int32_t)floorDiv(local, 86400);
    const int secOfDay = (int)(local - (int64_t)t.dayNumber * 86400);
    int year, month, day;
    civilFromDays(t.dayNumber, &year, &month, &day);
    t.year = (int16_t)year;
    t.month = (uint8_t)month;
    t.day = (uint8_t)day;
    t.hour = (uint8_t)(secOfDay / 3600);
    t.minute = (uint8_t)(secOfDay / 60 % 60);
    t.second = (uint8_t)(secOfDay % 60);
    t.weekday = (uint8_t)weekdayOfDays(t.dayNumber);
    return t;
}
