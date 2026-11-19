// worynim@gmail.com
/**
 * @file model.h
 * @brief 모드 표·설정 항목 (하드웨어 무관 — 호스트 테스트 대상, Arduino.h 금지)
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

enum class Unit : uint8_t { Times, Ms, OnOff, Hide, Sec };   // Hide: 1=Hide, 0=Show

// 버튼이 보내는 신호 종류
enum class Signal : uint8_t {
    Volume,   // 볼륨+ / 볼륨- (웹소설 넘김)
    Camera,   // BTN1 볼륨+ 셔터 / BTN2 딜레이 셔터
    Page,     // PageUp / PageDown (PPT)
    Wheel,    // 휠 위 / 아래 — 설정: Steps, Gap, Cursor
    Arrow,    // ↑ / ↓ — 설정: Keys, Gap
};

struct Setting {
    const char* name;    // 짧은 이름 (요약 줄에 표시)
    const char* desc1;   // 설정 화면 설명 2줄
    const char* desc2;
    const uint8_t* values;
    uint8_t numValues;
    Unit unit;
    uint8_t idx;         // 지금 값의 위치
    uint8_t defIdx;      // 기본값의 위치
};

struct Mode {
    const char* line1;   // 앱 이름 (화면 폭 64px이라 두 줄로 나눔)
    const char* line2;
    Signal signal;
    Setting* settings;
    uint8_t numSettings;
};

constexpr int kNumModes = 7;

Mode& modeAt(int i);        // 0..kNumModes-1
Setting& beepSetting();     // 모든 모드 공통

int valueOf(const Setting& s);
uint8_t clampIdx(uint8_t stored, const Setting& s);   // 저장된 위치가 목록 밖이면 기본값 위치
void formatValue(char* buf, size_t n, const Setting& s, uint8_t idx);   // "x3" "30ms" "On" "Hide" "3s"
void resetToDefaults();     // 모든 설정을 기본값으로
