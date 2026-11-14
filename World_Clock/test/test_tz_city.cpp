// worynim@gmail.com
/**
 * @file test_tz_city.cpp
 * @brief tz_calc · city_table 호스트 단위 테스트
 */
#include <stdio.h>
#include <string.h>

#include "../city_table.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static int64_t utcAt(int y, int mo, int d, int h, int mi) { return (int64_t)daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60; }

static const City& byName(const char* ko) {
    for (int i = 0; i < cityCount(); i++) {
        if (strcmp(cityAt(i).nameKo, ko) == 0) return cityAt(i);
    }
    printf("FAIL no city %s\n", ko);
    failures++;
    return cityAt(0);
}

static LocalTime at(const char* ko, int64_t utc) { return toLocalTime(utc, cityTz(byName(ko))); }

int main() {
    // 날짜 계산
    CHECK(daysFromCivil(1970, 1, 1) == 0);
    CHECK(daysFromCivil(2026, 11, 15) == 20772);
    CHECK(nthWeekdayOfMonth(2026, 3, 2, 0) == 8);       // 2026년 3월 둘째 일요일
    CHECK(nthWeekdayOfMonth(2026, 3, 5, 0) == 29);      // 마지막 일요일
    CHECK(nthWeekdayOfMonth(2026, 2, 5, 6) == 28);      // 2월 마지막 토요일

    // 고정 시간대 · 요일 · 날짜 넘어감: 2026-11-13(금) 14:41 UTC
    const int64_t t = utcAt(2026, 11, 13, 14, 41);
    LocalTime seoul = at("서울", t), sydney = at("시드니", t), ny = at("뉴욕", t), honolulu = at("호놀룰루", t);
    CHECK(seoul.hour == 23 && seoul.minute == 41 && seoul.day == 13 && seoul.weekday == 5 && !seoul.dst);
    CHECK(sydney.hour == 1 && sydney.day == 14 && sydney.weekday == 6 && sydney.dst && sydney.offsetMin == 660);
    CHECK(sydney.dayNumber - seoul.dayNumber == 1);
    CHECK(ny.hour == 9 && !ny.dst && ny.offsetMin == -300);
    CHECK(honolulu.hour == 4 && honolulu.dayNumber == seoul.dayNumber);

    // 30분·45분 시간대, 날짜 변경선 양쪽
    CHECK(at("뉴델리", t).hour == 20 && at("뉴델리", t).minute == 11);
    CHECK(at("카트만두", t).hour == 20 && at("카트만두", t).minute == 26);
    CHECK(at("키리티마티", t).dayNumber - at("호놀룰루", t).dayNumber == 1);   // 같은 경도, 하루 차이

    // 미국: 2026-03-08 02:00 EST(07:00 UTC)에 시작, 2026-11-01 02:00 EDT(06:00 UTC)에 끝
    CHECK(!at("뉴욕", utcAt(2026, 3, 8, 6, 59)).dst);
    CHECK(at("뉴욕", utcAt(2026, 3, 8, 7, 0)).dst && at("뉴욕", utcAt(2026, 3, 8, 7, 0)).hour == 3);
    CHECK(at("뉴욕", utcAt(2026, 11, 1, 5, 59)).dst);
    CHECK(!at("뉴욕", utcAt(2026, 11, 1, 6, 0)).dst && at("뉴욕", utcAt(2026, 11, 1, 6, 0)).hour == 1);

    // 유럽: 2026-03-29 01:00 UTC 시작, 2026-10-25 01:00 UTC 끝 — 시간대가 달라도 같은 순간
    CHECK(!at("런던", utcAt(2026, 3, 29, 0, 59)).dst && at("런던", utcAt(2026, 3, 29, 1, 0)).dst);
    CHECK(!at("헬싱키", utcAt(2026, 3, 29, 0, 59)).dst && at("헬싱키", utcAt(2026, 3, 29, 1, 0)).hour == 4);
    CHECK(at("파리", utcAt(2026, 10, 25, 0, 59)).dst && !at("파리", utcAt(2026, 10, 25, 1, 0)).dst);

    // 남반구: 해를 넘기는 서머타임 (1월은 서머타임, 7월은 아님)
    CHECK(at("시드니", utcAt(2027, 1, 1, 0, 0)).dst && !at("시드니", utcAt(2026, 7, 1, 0, 0)).dst);
    CHECK(at("오클랜드", utcAt(2027, 1, 1, 0, 0)).offsetMin == 780);
    CHECK(!at("브리즈번", utcAt(2027, 1, 1, 0, 0)).dst);

    // 도시 표: ID 중복 없음, 경도 순서, 기본 도시가 있음
    bool seen[256] = {false};
    for (int i = 0; i < cityCount(); i++) {
        CHECK(!seen[cityAt(i).id]);
        seen[cityAt(i).id] = true;
        CHECK(cityIndexOfId(cityAt(i).id) == i);
        if (i > 0) CHECK(cityAt(i - 1).lon <= cityAt(i).lon);
        CHECK(cityMapX(cityAt(i)) >= 0 && cityMapX(cityAt(i)) < 128 && cityMapY(cityAt(i)) >= 0 && cityMapY(cityAt(i)) < 64);
    }
    CHECK(cityIndexOfId(255) == -1);
    CHECK(strcmp(cityAt(cityIndexOfId(kDefaultCityIds[0])).nameKo, "서울") == 0);
    CHECK(strcmp(cityAt(cityIndexOfId(kDefaultCityIds[3])).nameEn, "Sydney") == 0);

    // 목록 이동: 양 끝이 이어지고, 시간대 건너뛰기는 UTC 차이가 달라지는 첫 도시에서 멈춘다
    CHECK(cityStep(0, -1) == cityCount() - 1 && cityStep(cityCount() - 1, +1) == 0);
    const int seoulIdx = cityIndexOfId(kDefaultCityIds[0]);
    const int east = cityJumpZone(seoulIdx, +1, t), west = cityJumpZone(seoulIdx, -1, t);
    CHECK(strcmp(cityAt(east).nameKo, "다윈") == 0);        // 서울(+9) → 부산(+9)은 건너뛰고 다윈(+9:30)
    CHECK(strcmp(cityAt(west).nameKo, "타이베이") == 0);

    printf("test_tz_city: %s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
