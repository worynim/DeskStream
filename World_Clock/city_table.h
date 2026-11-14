// worynim@gmail.com
/**
 * @file city_table.h
 * @brief 도시 표와 목록 이동 (순수 로직, Arduino 의존 없음)
 * @details 도시는 경도 순서(서→동)로 한 줄이며 양 끝이 이어진다. 화면에서 고른 도시는 목록 순서(index)가 아니라
 *          고정 ID 로 저장한다 — 나중에 도시를 추가·재정렬해도 저장값이 다른 도시로 바뀌지 않는다.
 *          시간대 규칙은 tzdata 2026f 기준이다 (test/run_all.sh 가 시스템 zoneinfo 와 대조한다).
 *          각국 정책이 바뀌면 이 표(펌웨어)를 고쳐야 한다.
 */
#ifndef CITY_TABLE_H
#define CITY_TABLE_H

#include <stdint.h>

#include "tz_calc.h"

struct City {
    uint8_t id;             ///< 저장용 고정 번호 (한번 정하면 바꾸지 않는다, 새 도시는 다음 번호)
    const char* nameKo;     ///< 한글 이름
    const char* nameEn;     ///< 영문 이름
    int16_t lon;            ///< 경도 (도, 동쪽이 +)
    int8_t lat;             ///< 위도 (도, 북쪽이 +)
    uint8_t zone;           ///< 시간대 번호 (cityTz 로 꺼낸다)
};

/** 처음 켰을 때 화면 1~4 의 도시 ID: 서울 · 런던 · 뉴욕 · 시드니 */
extern const uint8_t kDefaultCityIds[4];

int cityCount();

/** 목록 순서 index(0 ~ cityCount()-1)의 도시 */
const City& cityAt(int index);

/** 그 도시의 시간대 규칙 */
const TzInfo& cityTz(const City& city);

/** 고정 ID → 목록 순서. 없는 ID 면 -1 */
int cityIndexOfId(uint8_t id);

/** 서쪽(dir = -1)/동쪽(+1)으로 한 칸. 양 끝은 이어진다 */
int cityStep(int index, int dir);

/** dir 방향으로, 지금(utc) UTC 차이가 index 도시와 다른 첫 도시까지 건너뛴다 */
int cityJumpZone(int index, int dir, int64_t utc);

/** 지도(128x64 등장방형)에서의 x(0~127), y(0~63) */
int cityMapX(const City& city);
int cityMapY(const City& city);

#endif
