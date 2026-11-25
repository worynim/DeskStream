// worynim@gmail.com
/**
 * @file world_clock.h
 * @brief 세계 시계 데이터 선언
 * @details 화면 배치 칸마다 도시를 따로 둔다 (세계 시계를 여러 칸에 놓으면 칸마다 다른 도시).
 *          도시의 현재 UTC 오프셋(서머타임 반영)을 Open-Meteo에서 받아(응답 약 200B, API 키 불필요)
 *          기기 시각에 더해 현지 시각을 그린다. 저장된 도시가 없는 칸은 뉴욕이다.
 */
#ifndef WORLD_CLOCK_H
#define WORLD_CLOCK_H

#include <Arduino.h>
#include "config.h"

constexpr int WC_SLOTS = MAX_DATA_PAGE * 4;   // 화면 배치 칸 수 (SCREEN_MAP과 같은 번호)

struct WorldClockCity {
  String name;     // OLED 표시용 도시 이름(영문)
  float lat, lon;  // 도시 좌표
  long offset;     // 현재 UTC 오프셋(초)
  bool valid;      // offset을 받았는가
};

extern WorldClockCity wc[WC_SLOTS];

/** 칸 i를 기본 도시(뉴욕)로 — 저장된 도시가 없을 때 */
void wcSetDefault(int i);

bool get_world_clock();  // 세계 시계가 놓인 칸의 오프셋을 모두 받으면 true

#endif
