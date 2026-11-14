// worynim@gmail.com
/**
 * @file misc_view.h
 * @brief 시계·설정 외의 작은 화면 — 안내문, 알림 딱지, 정보 카드
 * @details U8g2 C API(u8g2_t*)로 그려서 장치와 PC(test/render)에서 같은 코드를 쓴다.
 */
#ifndef MISC_VIEW_H
#define MISC_VIEW_H

#include <stdint.h>

#include "clib/u8g2.h"

#include "lang.h"

/** 버퍼를 지우고 글자 줄들을 가운데 맞춤으로 그린다 (최대 5줄, nullptr 인 줄은 빈 줄) */
void drawMessage(u8g2_t* u, const char* const* lines, int count);

/** 이미 그린 시계 화면의 큰 숫자 줄을 지우고 그 자리에 알림 딱지를 그린다 ("24H", "밝기 2/4" 등) */
void drawToast(u8g2_t* u, const char* text);

/** 정보 카드에 그릴 내용 */
struct InfoCard {
    const char* city;       ///< 도시 이름 (lang 에 맞는 것)
    Lang lang;
    int16_t year;
    uint8_t month, day, weekday;
    int16_t utcOffsetMin;   ///< 지금 UTC 차이(분)
    bool dst;               ///< 지금 서머타임 중
    bool isHome;            ///< 기준 도시(화면 1)
    int16_t homeDiffMin;    ///< 기준 도시와의 시차(분)
};

/** 정보 카드: 도시 이름 · 날짜(연도 포함) · UTC 차이 · 기준 도시와의 시차 */
void drawInfoCard(u8g2_t* u, const InfoCard& c);

#endif
