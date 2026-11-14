// worynim@gmail.com
/**
 * @file setup_view.h
 * @brief 장소 설정 화면 렌더러 — 고치는 화면(미리보기)과 나머지 세 화면(지도/취소·서쪽·동쪽 버튼)
 * @details 화면을 누르면 그 화면의 버튼이 눌리는 보드라, 각 화면에 "누르면 일어나는 일"을 그린다.
 *          U8g2 C API(u8g2_t*)로 그려서 장치와 PC(test/render)에서 같은 코드를 쓴다.
 */
#ifndef SETUP_VIEW_H
#define SETUP_VIEW_H

#include <stdint.h>

#include "clib/u8g2.h"

#include "lang.h"

/** 설정 중 각 화면의 역할 */
enum SetupRole : uint8_t { SETUP_EDIT, SETUP_MAP, SETUP_WEST, SETUP_EAST };

/**
 * 화면 editScreen 을 고치는 동안 화면 screen 의 역할.
 * 고치는 화면을 뺀 나머지를 왼쪽부터 지도(취소) · 서쪽 · 동쪽 으로 배정한다.
 */
SetupRole setupRole(int editScreen, int screen);

/** 고치는 화면에 보여줄 후보 도시의 미리보기 */
struct SetupPreview {
    const char* city;       ///< 후보 도시 이름 (lang 에 맞는 것)
    Lang lang;              ///< 표시 언어
    uint8_t hour, minute;   ///< 그 도시의 현지 시각
    int16_t utcOffsetMin;   ///< UTC 와의 차이(분, 서머타임 반영)
    bool dst;               ///< 지금 서머타임 중
    const char* homeCity;   ///< 기준 도시 이름 (기준 화면을 고치는 중이면 nullptr)
    int16_t homeDiffMin;    ///< 기준 도시와의 시차(분)
    bool frameOn;           ///< 깜빡이는 테두리의 현재 상태
    bool valid;             ///< false = 시각을 아직 못 받음
};

/** "UTC+5:30" 형태로 쓴다 (분이 0이면 "UTC+9") */
void formatUtcOffset(char* out, int size, int offsetMin);

/** 고치는 화면: 도시 이름 · 현재 시각 · UTC 차이 · 기준 도시와의 시차 · "누르면 저장" */
void drawSetupPreview(u8g2_t* u, const SetupPreview& p);

/** 서쪽(east = false)/동쪽 버튼 화면: 큰 화살표와 누르면 갈 도시 */
void drawSetupMove(u8g2_t* u, Lang lang, bool east, const char* nextCity, int nextUtcOffsetMin);

/** 지도·취소 화면: 세계 지도에 후보 도시 위치(mapX 0~127, mapY 0~63)를 표시. markerOn 으로 깜빡인다 */
void drawSetupMap(u8g2_t* u, Lang lang, int mapX, int mapY, bool markerOn);

#endif
