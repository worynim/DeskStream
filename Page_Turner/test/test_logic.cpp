// worynim@gmail.com
/**
 * @file test_logic.cpp
 * @brief Page_Turner 순수 로직 호스트 단위 테스트 (모드 표·설정 메뉴·셔터 타이머·절전 정책)
 */
#include <stdio.h>
#include <string.h>

#include "../model.h"
#include "../menu.h"
#include "../power_policy.h"
#include "../shutter_timer.h"
#include "../slots.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static void test_model_defaults() {
    resetToDefaults();
    CHECK(kNumModes == 7);
    CHECK(strcmp(modeAt(0).line1, "YouTube") == 0 && modeAt(0).signal == Signal::Wheel);
    CHECK(valueOf(modeAt(0).settings[0]) == 3 && valueOf(modeAt(0).settings[1]) == 30 && valueOf(modeAt(0).settings[2]) == 1);
    CHECK(valueOf(modeAt(1).settings[0]) == 4);                       // Kakao Webtoon Keys x4
    CHECK(valueOf(modeAt(3).settings[0]) == 10 && valueOf(modeAt(3).settings[1]) == 80);
    CHECK(modeAt(5).signal == Signal::Camera && valueOf(modeAt(5).settings[0]) == 3);
    CHECK(modeAt(2).numSettings == 0 && modeAt(6).signal == Signal::Page);
    CHECK(valueOf(beepSetting()) == 1);
}

static void test_format() {
    resetToDefaults();
    char b[12];
    formatValue(b, sizeof b, modeAt(0).settings[0], 2); CHECK(strcmp(b, "x3") == 0);
    formatValue(b, sizeof b, modeAt(0).settings[1], 1); CHECK(strcmp(b, "30ms") == 0);
    formatValue(b, sizeof b, modeAt(0).settings[2], 1); CHECK(strcmp(b, "Hide") == 0);
    formatValue(b, sizeof b, modeAt(5).settings[0], 1); CHECK(strcmp(b, "3s") == 0);
    formatValue(b, sizeof b, beepSetting(), 0);          CHECK(strcmp(b, "Off") == 0);
}

static void test_menu_save_cancel() {
    resetToDefaults();
    SettingsMenu m;
    m.open(modeAt(0));
    CHECK(m.itemCount() == 4);                // Steps, Gap, Cursor, Beep
    m.inc(); CHECK(m.pending(0) == 3);
    for (int i = 0; i < 20; i++) m.inc();
    CHECK(m.pending(0) == 12);                // 목록 끝(13개)에서 멈춤
    m.cancel();
    CHECK(!m.isOpen() && valueOf(modeAt(0).settings[0]) == 3);
    m.open(modeAt(0));
    m.dec();
    CHECK(!m.next() && !m.next() && !m.next());
    CHECK(m.next());                          // 4번째 next에서 저장
    CHECK(valueOf(modeAt(0).settings[0]) == 2 && !m.isOpen());
}

static void test_menu_beep_only() {
    resetToDefaults();
    SettingsMenu m;
    m.open(modeAt(2));
    CHECK(m.itemCount() == 1 && &m.item(0) == &beepSetting());
    for (int i = 0; i < 3; i++) m.dec();
    CHECK(m.pending(0) == 0);
    CHECK(m.next());
    CHECK(valueOf(beepSetting()) == 0);
}

static void test_timer() {
    ShutterTimer t;
    t.start(1000, 3);
    CHECK(t.running() && t.secondsLeft() == 3);
    CHECK(t.tick(1500) == ShutterTimer::Tick::None);
    CHECK(t.tick(2001) == ShutterTimer::Tick::Second && t.secondsLeft() == 2);
    CHECK(t.tick(3001) == ShutterTimer::Tick::Second && t.secondsLeft() == 1);
    CHECK(t.tick(4000) == ShutterTimer::Tick::Fire && !t.running());
    CHECK(t.tick(5000) == ShutterTimer::Tick::None);
    t.start(0, 5);
    t.cancel();
    CHECK(!t.running() && t.tick(9000) == ShutterTimer::Tick::None);
}

static void test_timer_wrap() {
    ShutterTimer t;
    t.start(0xFFFFFC18u, 2);   // millis 랩 1초 전에 시작
    CHECK(t.tick(0x000003E8u) == ShutterTimer::Tick::Fire);
}

static void test_power() {
    CHECK(powerStage(59999, 0) == PowerStage::Active);
    CHECK(powerStage(60000, 0) == PowerStage::ScreenOff);
    CHECK(powerStage(299999, 0) == PowerStage::ScreenOff);
    CHECK(powerStage(300000, 0) == PowerStage::Sleep);
}

static void test_power_wrap() {
    CHECK(powerStage(1000, 0xFFFFFC18u) == PowerStage::Active);   // 2초 경과, 랩 걸침
}

static void test_model_clamp() {
    resetToDefaults();
    CHECK(clampIdx(5, modeAt(0).settings[0]) == 5);
    CHECK(clampIdx(13, modeAt(0).settings[0]) == 2);   // Steps 13개 → 13은 범위 밖 → 기본 x3 위치
    CHECK(clampIdx(255, beepSetting()) == 1);
}

static const char* A = "aa:aa:aa:aa:aa:aa";
static const char* B = "bb:bb:bb:bb:bb:bb";
static const char* C = "cc:cc:cc:cc:cc:cc";

static void test_slots_assign_find_clear() {
    SlotTable t;
    CHECK(t.isEmpty(0) && t.isEmpty(1) && t.isEmpty(2));
    CHECK(t.find(A) == -1 && t.firstEmpty() == 0);
    t.assign(1, A);
    CHECK(!t.isEmpty(1) && t.find(A) == 1 && strcmp(t.addr(1), A) == 0);
    CHECK(t.firstEmpty() == 0);
    t.assign(2, A);                       // 같은 기기를 다른 슬롯에 넣으면 이전 슬롯은 비운다
    CHECK(t.find(A) == 2 && t.isEmpty(1));
    t.clear(2);
    CHECK(t.isEmpty(2) && t.find(A) == -1);
}

static void test_slots_adopt() {
    SlotTable t;
    t.assign(0, A);
    t.adopt(A);                           // 이미 있으면 그대로
    t.adopt(B);
    t.adopt(C);
    CHECK(t.find(A) == 0 && t.find(B) == 1 && t.find(C) == 2);
    t.adopt("dd:dd:dd:dd:dd:dd");         // 빈 슬롯이 없으면 넣지 않는다
    CHECK(t.find("dd:dd:dd:dd:dd:dd") == -1 && t.firstEmpty() == -1);
}

static void test_slots_case_insensitive() {
    SlotTable t;
    t.assign(0, "AB:CD:EF:01:23:45");
    CHECK(t.find("ab:cd:ef:01:23:45") == 0);
}

static void test_slots_return_target() {
    SlotTable t;
    t.assign(0, A);
    t.assign(1, B);
    CHECK(t.returnTarget(2, 0) == 0);     // 빈 슬롯 3을 새로 받는 중: 쓰던 슬롯 1로
    CHECK(t.returnTarget(0, 0) == 1);     // 쓰던 슬롯 1 자체를 비우는 중: 다른 페어링 슬롯 2로
    t.clear(1);
    CHECK(t.returnTarget(0, 0) == -1);    // 남은 페어링 기기가 없으면 돌아갈 곳 없음
    CHECK(t.returnTarget(1, -1) == 0);    // 기억한 슬롯이 없어도 다른 페어링 슬롯으로
}

int main() {
    test_slots_return_target();
    test_slots_assign_find_clear();
    test_slots_adopt();
    test_slots_case_insensitive();
    test_model_defaults();
    test_model_clamp();
    test_format();
    test_menu_save_cancel();
    test_menu_beep_only();
    test_timer();
    test_timer_wrap();
    test_power();
    test_power_wrap();
    printf("test_logic: %s\n", failures ? "FAILED" : "ALL PASS");
    return failures ? 1 : 0;
}
