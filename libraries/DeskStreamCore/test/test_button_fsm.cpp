// worynim@gmail.com
/**
 * @file test_button_fsm.cpp
 * @brief 버튼 짧게/길게 판정 상태 머신 회귀 테스트
 * @details 실행: bash test/run_all.sh
 * @note 고정하는 것: Hangeul_Clock input_manager(v2.10.0)와 같은 판정 —
 *       길게는 누른 채 1000ms **초과** 시 즉시 1회, 짧게는 50ms **초과** 누름 후 놓을 때,
 *       길게가 난 누름은 놓아도 짧게가 나지 않는다, 바운스는 무시, millis 랩 구간.
 *       + Universal_Timer 자동 반복(300ms 후 60ms 간격, 반복이 시작되면 짧게 없음,
 *         1000ms에 LONG과 REPEAT가 **함께** 난다 — 예전엔 REPEAT가 LONG을 덮어썼다)
 *       + Game_Of_Life 안정 디바운스(20ms)와 누름 즉시(PRESS) 이벤트.
 */
#include <cstdio>
#include "input/ds_button_fsm.h"

using namespace ds;

static int g_pass = 0;
static int g_fail = 0;

static void expectEv(const char* label, uint8_t got, uint8_t want) {
    if (got == want) g_pass++;
    else { g_fail++; printf("  FAIL  [%s] got 0x%02X, want 0x%02X\n", label, got, want); }
}

static const ButtonTiming T;   // 50 / 1000 ms
static const bool UP = true, DOWN = false;

int main() {
    {   // 짧게: 100ms 누름
        ButtonFsm f;
        expectEv("누름 순간 → PRESS", f.update(DOWN, 1000, T), BUTTON_PRESS);
        expectEv("누르는 중", f.update(DOWN, 1050, T), BUTTON_NONE);
        expectEv("100ms 후 놓음 → SHORT", f.update(UP, 1100, T), BUTTON_SHORT);
        expectEv("놓은 뒤 유지", f.update(UP, 1200, T), BUTTON_NONE);
    }
    {   // 디바운스 경계: 정확히 50ms는 무시, 51ms는 SHORT
        ButtonFsm f;
        f.update(DOWN, 0, T);
        expectEv("50ms 누름 → 무시", f.update(UP, 50, T), BUTTON_NONE);
        f.update(DOWN, 100, T);
        expectEv("51ms 누름 → SHORT", f.update(UP, 151, T), BUTTON_SHORT);
    }
    {   // 길게: 1000ms 초과 시 누른 채로 1회, 놓아도 SHORT 없음
        ButtonFsm f;
        f.update(DOWN, 0, T);
        expectEv("1000ms 정확히 → 아직", f.update(DOWN, 1000, T), BUTTON_NONE);
        expectEv("1001ms → LONG", f.update(DOWN, 1001, T), BUTTON_LONG);
        expectEv("계속 누름 → 반복 없음", f.update(DOWN, 3000, T), BUTTON_NONE);
        expectEv("길게 후 놓음 → SHORT 없음", f.update(UP, 3100, T), BUTTON_NONE);
    }
    {   // 누름 바운스: 짧은 튐은 무시되고 안정된 누름만 판정된다
        ButtonFsm f;
        f.update(DOWN, 0, T);
        expectEv("10ms 튐 → 무시", f.update(UP, 10, T), BUTTON_NONE);
        f.update(DOWN, 12, T);
        expectEv("안정된 누름 후 놓음 → SHORT", f.update(UP, 200, T), BUTTON_SHORT);
        f.update(DOWN, 205, T);   // 뗌 바운스
        expectEv("뗌 바운스 → 무시", f.update(UP, 210, T), BUTTON_NONE);
    }
    {   // millis 49.7일 랩 구간
        ButtonFsm f;
        f.update(DOWN, 0xFFFFFF00u, T);
        expectEv("랩 넘어 1001ms → LONG", f.update(DOWN, 0xFFFFFF00u + 1001u, T), BUTTON_LONG);
    }
    {   // 부팅 시 이미 눌린 상태로 시작 — 첫 update가 FALLING으로 잡혀야 한다 (lastHigh 초기값 true)
        ButtonFsm f;
        expectEv("눌린 채 시작 → PRESS", f.update(DOWN, 500, T), BUTTON_PRESS);
        expectEv("곧 놓음 → SHORT", f.update(UP, 600, T), BUTTON_SHORT);
    }

    {   // 자동 반복 (Universal_Timer: 300ms 후 60ms 간격)
        ButtonTiming R; R.debounceMs = 30; R.repeatDelayMs = 300; R.repeatIntervalMs = 60;
        ButtonFsm f;
        f.update(DOWN, 0, R);
        expectEv("299ms → 아직", f.update(DOWN, 299, R), BUTTON_NONE);
        expectEv("300ms → 첫 REPEAT", f.update(DOWN, 300, R), BUTTON_REPEAT);
        expectEv("359ms → 아직", f.update(DOWN, 359, R), BUTTON_NONE);
        expectEv("360ms → REPEAT", f.update(DOWN, 360, R), BUTTON_REPEAT);
        expectEv("반복 후 놓음 → SHORT 없음 (증가 중복 방지)", f.update(UP, 400, R), BUTTON_NONE);

        ButtonFsm g;   // 1000ms에 LONG과 REPEAT가 함께 (예전엔 REPEAT가 LONG을 덮어썼다)
        g.update(DOWN, 0, R);
        g.update(DOWN, 300, R);
        g.update(DOWN, 960, R);
        expectEv("1020ms → LONG | REPEAT 동시", g.update(DOWN, 1020, R), BUTTON_LONG | BUTTON_REPEAT);

        ButtonFsm h;   // 반복 시작 전에 놓으면 SHORT
        h.update(DOWN, 0, R);
        expectEv("200ms 누름 → SHORT", h.update(UP, 200, R), BUTTON_SHORT);
    }
    {   // 안정 디바운스 (Game_Of_Life: 20ms) — 흔들리는 동안은 상태가 바뀌지 않는다
        ButtonTiming S; S.stableMs = 20;
        ButtonFsm f;
        expectEv("첫 LOW → 아직 (안정 대기)", f.update(DOWN, 100, S), BUTTON_NONE);
        expectEv("5ms 뒤 HIGH 튐 → 무시", f.update(UP, 105, S), BUTTON_NONE);
        expectEv("다시 LOW", f.update(DOWN, 107, S), BUTTON_NONE);
        expectEv("20ms 이하 유지 → 아직", f.update(DOWN, 127, S), BUTTON_NONE);
        expectEv("20ms 넘게 유지 → PRESS", f.update(DOWN, 128, S), BUTTON_PRESS);
        expectEv("계속 누름 → 추가 PRESS 없음", f.update(DOWN, 200, S), BUTTON_NONE);
        f.update(UP, 300, S);
        expectEv("놓음 안정 → SHORT", f.update(UP, 321, S), BUTTON_SHORT);
    }

    printf("test_button_fsm: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
