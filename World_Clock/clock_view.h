// worynim@gmail.com
/**
 * @file clock_view.h
 * @brief 시계 화면(128x64 한 장) 렌더러 — 도시 하나의 현지 시각
 * @details 큰 AM/PM 글자(AM = 위, PM = 아래) · 낮/밤 아이콘 · 하루 막대로 오전/오후와 낮/밤을 보여준다.
 *          U8g2 C API(u8g2_t*)로 그려서 장치와 PC(test/render)에서 같은 코드를 쓴다.
 *          장치에서는 display.screen(i).getU8g2() 를 넘긴다.
 */
#ifndef CLOCK_VIEW_H
#define CLOCK_VIEW_H

#include <stdint.h>

#include "clib/u8g2.h"

#include "lang.h"

/** 시계 화면 한 장에 그릴 내용 */
struct ClockFace {
    const char* city;   ///< 도시 이름 (UTF-8, lang 에 맞는 것)
    Lang lang;          ///< 요일·날짜 차이 표시 언어
    uint8_t hour;       ///< 현지 시 0~23
    uint8_t minute;     ///< 현지 분 0~59
    uint8_t month;      ///< 현지 월 1~12
    uint8_t day;        ///< 현지 일 1~31
    uint8_t weekday;    ///< 현지 요일 0(일)~6(토)
    int8_t dayDiff;     ///< 기준 도시와의 날짜 차이 (-1, 0, +1)
    bool hour24;        ///< true = 24시 표기
    bool colonOn;       ///< 깜빡이는 ':' 의 현재 상태
    bool valid;         ///< false = 시각을 아직 못 받음 ("--:--")
};

/** 낮(06:00~17:59)이면 true — 해/달 아이콘과 하루 막대가 같은 기준을 쓴다 */
bool isDaytime(uint8_t hour);

/** 하루 막대에서 지금 시각의 x 좌표 (0시 = 0, 24시 직전 = 127) */
int dayBarX(uint8_t hour, uint8_t minute);

/** 해(day = true) 또는 달 아이콘을 중심 (cx, cy), 약 15x15 크기로 그린다 */
void drawDayNightIcon(u8g2_t* u, int cx, int cy, bool day);

/** 버퍼를 지우고 시계 화면을 그린다 (전송은 호출자가 display.push() 로) */
void drawClock(u8g2_t* u, const ClockFace& f);

#endif
