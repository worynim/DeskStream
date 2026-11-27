// worynim@gmail.com
/**
 * @file slot_city.h
 * @brief 화면 칸별 도시와 그 도시로 받는 데이터 (세계 시계·일출일몰·주간 날씨·기온 그래프)
 * @details 한 칸에는 위젯이 하나만 놓이므로 도시는 칸마다 하나를 함께 쓴다.
 *          도시를 고르지 않은 칸은 위젯별 기본 도시(세계 시계 New York, 그 밖 Seoul)를 쓴다.
 *          데이터는 모두 Open-Meteo(API 키 불필요)에서 받는다 — 수집은 data_manager.cpp.
 */
#ifndef SLOT_CITY_H
#define SLOT_CITY_H

#include <Arduino.h>
#include "config.h"

struct SlotCity {
  String name;     // OLED 표시용 도시 이름(영문) — 비어 있으면 위젯 기본 도시
  float lat, lon;  // 도시 좌표
};
extern SlotCity slotCity[SCREEN_SLOTS];

/** 칸 slot에 놓인 위젯 type이 쓸 도시 (고르지 않았으면 기본 도시) */
SlotCity cityFor(int slot, WidgetType type);

// --- 세계 시계: 도시의 현재 UTC 오프셋(서머타임 반영) ---
struct WorldClockTime {
  long offset;   // 초
  bool valid;
};
extern WorldClockTime wcTime[SCREEN_SLOTS];

// --- 일출·일몰 (달 모양은 날짜로 계산) ---
struct SunInfo {
  char rise[6], set[6];   // 현지 "07:22" — 해가 뜨지 않거나 지지 않는 날은 둘 다 "--:--"
  bool valid;
};
extern SunInfo sunInfo[SCREEN_SLOTS];

// --- 주간 날씨 (오늘부터 5일) ---
constexpr int WEEK_DAYS = 5;
struct WeeklyInfo {
  int wday[WEEK_DAYS];   // 요일 (0=일요일)
  int code[WEEK_DAYS];   // WMO 날씨 코드
  int tmax[WEEK_DAYS], tmin[WEEK_DAYS];   // 최고·최저 기온(°C, 반올림)
  bool valid;
};
extern WeeklyInfo weekly[SCREEN_SLOTS];

// --- 기온 그래프 (오늘 0~23시, 도시 현지 시각) — 모두 예보 모델 값 (관측값 아님) ---
struct TempGraph {
  float temp[24];   // 시간별 예보 기온(°C)
  float now;        // 지금 기온(°C) — 15분 간격으로 갱신되는 모델 값
  long offset;      // 도시의 UTC 오프셋(초) — 지금 시각 표시용
  bool valid;
};
extern TempGraph tempGraph[SCREEN_SLOTS];

/** 칸의 도시가 바뀌었을 때 그 칸의 받은 데이터를 지운다 (새 값을 받기 전까지 "--") */
void clearSlotData(int slot);

bool get_world_clock();  // 세계 시계 칸의 오프셋을 모두 받으면 true
bool get_sun();          // 일출·일몰 칸을 모두 받으면 true
bool get_weekly();       // 주간 날씨 칸을 모두 받으면 true
bool get_temp_graph();   // 기온 그래프 칸을 모두 받으면 true

#endif
