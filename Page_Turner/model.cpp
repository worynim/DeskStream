// worynim@gmail.com
/**
 * @file model.cpp
 * @brief 모드 표와 설정값 후보 — 기본값은 Android 실기 시험 결과 (2026-11-20, DESIGN.md §4)
 */
#include "model.h"

#include <stdio.h>

static const uint8_t kStepValues[] = {1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 30};
static const uint8_t kKeyValues[] = {1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 15, 20};
static const uint8_t kGapValues[] = {10, 30, 50, 80, 120, 160, 200, 250};
static const uint8_t kTimerValues[] = {2, 3, 5, 10};
static const uint8_t kOnOffValues[] = {0, 1};

#define SETTING(name, d1, d2, arr, unit, def) {name, d1, d2, arr, sizeof(arr), unit, def, def}
#define STEPS(def)  SETTING("Steps", "Wheel steps", "per press", kStepValues, Unit::Times, def)
#define KEYS(def)   SETTING("Keys", "Arrow keys", "per press", kKeyValues, Unit::Times, def)
#define GAP(def)    SETTING("Gap", "Time between", "steps", kGapValues, Unit::Ms, def)
#define CURSOR(def) SETTING("Cursor", "Move cursor", "to the edge", kOnOffValues, Unit::Hide, def)

static Setting youtubeSet[] = {STEPS(2), GAP(1), CURSOR(1)};      // x3, 30ms, Hide
static Setting kakaoToonSet[] = {KEYS(3), GAP(1)};                // x4, 30ms
static Setting seriesToonSet[] = {STEPS(7), GAP(3), CURSOR(1)};   // x10, 80ms, Hide
static Setting cameraSet[] = {SETTING("Timer", "BTN2 shutter", "delay", kTimerValues, Unit::Sec, 1)};   // 3s
static Setting beep = SETTING("Beep", "Button", "sound", kOnOffValues, Unit::OnOff, 1);                 // On

static Mode modes[kNumModes] = {
    {"YouTube", "",        Signal::Wheel,  youtubeSet,    3},
    {"Kakao",   "Webtoon", Signal::Arrow,  kakaoToonSet,  2},
    {"Kakao",   "Novel",   Signal::Volume, nullptr,       0},
    {"Series",  "Webtoon", Signal::Wheel,  seriesToonSet, 3},
    {"Series",  "Novel",   Signal::Volume, nullptr,       0},
    {"Camera",  "",        Signal::Camera, cameraSet,     1},
    {"PPT",     "",        Signal::Page,   nullptr,       0},
};

Mode& modeAt(int i) { return modes[i]; }
Setting& beepSetting() { return beep; }

int valueOf(const Setting& s) { return s.values[s.idx]; }

uint8_t clampIdx(uint8_t stored, const Setting& s) { return stored < s.numValues ? stored : s.defIdx; }

void formatValue(char* buf, size_t n, const Setting& s, uint8_t idx) {
    int v = s.values[idx];
    switch (s.unit) {
        case Unit::Times: snprintf(buf, n, "x%d", v); break;
        case Unit::Ms:    snprintf(buf, n, "%dms", v); break;
        case Unit::Sec:   snprintf(buf, n, "%ds", v); break;
        case Unit::Hide:  snprintf(buf, n, "%s", v ? "Hide" : "Show"); break;
        case Unit::OnOff: snprintf(buf, n, "%s", v ? "On" : "Off"); break;
    }
}

void resetToDefaults() {
    for (Mode& m : modes) {
        for (int i = 0; i < m.numSettings; i++) m.settings[i].idx = m.settings[i].defIdx;
    }
    beep.idx = beep.defIdx;
}
