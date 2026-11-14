// worynim@gmail.com
/**
 * @file tz_dump.cpp
 * @brief 도시마다 UTC 차이를 줄줄이 출력한다 — tz_crosscheck.py 가 시스템 시간대 데이터(zoneinfo)와 대조
 * @details 출력: "<목록 순서> <UTC epoch> <UTC 차이(분)>" — 2026-12-01 부터 3년, 1시간 37분 간격
 *          (표는 지금 이후의 규칙만 담는다 — 2026-11 에 바뀐 밴쿠버·캘거리의 옛 규칙은 대조하지 않는다).
 */
#include <stdio.h>

#include "../city_table.h"

int main() {
    const int64_t begin = (int64_t)daysFromCivil(2026, 12, 1) * 86400;
    const int64_t end = (int64_t)daysFromCivil(2029, 12, 1) * 86400;
    for (int i = 0; i < cityCount(); i++) {
        for (int64_t t = begin; t < end; t += 97 * 60) {
            printf("%d %lld %d\n", i, (long long)t, utcOffsetMinutes(t, cityTz(cityAt(i))));
        }
    }
    return 0;
}
