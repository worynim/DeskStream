// worynim@gmail.com
/**
 * @file test_time_sync_policy.cpp
 * @brief NTP 재동기화 시점 · 1초 렌더 트리거 규칙 회귀 테스트
 * @details 실행: bash test/run_all.sh
 *
 * @note 이 테스트가 고정하는 것은 사용자 보고에서 출발한 두 가지다.
 *       1) "계속 켜 놓으면 십여 초씩 틀어진다" → 재동기화가 실제로 도는지 감독하는
 *          정책(주기/오래됨/미동기 재시도/WiFi 복구)이 있어야 한다.
 *       2) "시계가 늦다"는 체감 → 1초 렌더가 millis 주기가 아니라 `tm_sec` 변화로
 *          돌아야 한다. 특히 millis() 49.7일 랩 구간에서 감독이 멈추지 않아야 한다.
 */
#include <cstdio>
#include "net/ds_time_sync_policy.h"

using namespace ds;

static int g_pass = 0;
static int g_fail = 0;

static const char* actionName(TimeSyncAction a) {
    switch (a) {
        case TIME_SYNC_NONE:       return "NONE";
        case TIME_SYNC_RETRY:      return "RETRY";
        case TIME_SYNC_PERIODIC:   return "PERIODIC";
        case TIME_SYNC_STALE:      return "STALE";
        case TIME_SYNC_NETWORK_UP: return "NETWORK_UP";
    }
    return "?";
}

static void expectAction(const char* label, const TimeSyncState& s,
                         const TimeSyncThresholds& t, TimeSyncAction want) {
    const TimeSyncAction got = timeSyncActionDue(s, t);
    if (got == want) {
        g_pass++;
    } else {
        g_fail++;
        printf("  FAIL  [%s] got %s, want %s\n", label, actionName(got), actionName(want));
    }
}

static void expectBool(const char* label, bool got, bool want) {
    if (got == want) {
        g_pass++;
    } else {
        g_fail++;
        printf("  FAIL  [%s] got %s, want %s\n", label, got ? "true" : "false", want ? "true" : "false");
    }
}

static void expectInt(const char* label, int got, int want) {
    if (got == want) {
        g_pass++;
    } else {
        g_fail++;
        printf("  FAIL  [%s] got %d, want %d\n", label, got, want);
    }
}

// 시계 프로젝트 config.h 의 채택값과 같은 값
static const uint32_t PERIODIC_MS = 21600000UL;  // 6시간
static const uint32_t STALE_MS    = 43200000UL;  // 12시간
static const uint32_t RETRY_MS    = 30000UL;     // 30초
static const uint32_t MIN_GAP_MS  = 60000UL;     // 1분
static const TimeSyncThresholds T = {PERIODIC_MS, STALE_MS, RETRY_MS, MIN_GAP_MS};

/** 기본 상태: WiFi 정상, 동기 완료, 방금(0ms) 성공, 시도 0ms */
static TimeSyncState baseState() {
    TimeSyncState s;
    s.wifiUp = true;
    s.everSynced = true;
    s.networkChanged = false;
    s.attempted = true;
    s.nowMs = 0;
    s.lastSyncMs = 0;
    s.lastAttemptMs = 0;
    return s;
}

static void testWifiDown() {
    printf("WiFi가 끊겨 있으면 아무 요청도 하지 않는다 (DNS/요청 낭비 방지)\n");
    TimeSyncState s = baseState();
    s.wifiUp = false;
    s.nowMs = STALE_MS + 1;   // 오래됨·주기 모두 해당하지만 네트워크가 없다
    expectAction("wifi down + stale", s, T, TIME_SYNC_NONE);

    s.networkChanged = true;
    expectAction("wifi down + networkChanged", s, T, TIME_SYNC_NONE);
}

static void testRetry() {
    printf("아직 성공 못 한 상태(부팅 직후)는 30초 간격으로 재시도한다\n");
    TimeSyncState s = baseState();
    s.everSynced = false;
    s.attempted = false;
    s.nowMs = 12345;
    expectAction("미동기 + 시도 기록 없음 → 즉시", s, T, TIME_SYNC_RETRY);

    s.attempted = true;
    s.lastAttemptMs = 0;
    s.nowMs = RETRY_MS - 1;
    expectAction("미동기 + 29.999초 경과 → 대기", s, T, TIME_SYNC_NONE);

    s.nowMs = RETRY_MS;
    expectAction("미동기 + 30초 경과 → 재시도", s, T, TIME_SYNC_RETRY);
}

static void testPeriodicAndStale() {
    printf("동기 후 6시간이면 주기 재동기화, 12시간이면 즉시(감독 실패) 재동기화\n");
    TimeSyncState s = baseState();

    s.nowMs = PERIODIC_MS - 1;
    expectAction("5:59:59.999 → 아직", s, T, TIME_SYNC_NONE);

    s.nowMs = PERIODIC_MS;
    expectAction("6시간 → PERIODIC", s, T, TIME_SYNC_PERIODIC);

    s.lastAttemptMs = 0;               // 방금이 아니라 6시간 전 시도 → minGap 통과
    s.nowMs = STALE_MS - 1;
    expectAction("11:59:59.999 → PERIODIC", s, T, TIME_SYNC_PERIODIC);

    s.nowMs = STALE_MS;
    expectAction("12시간 → STALE(주기보다 우선)", s, T, TIME_SYNC_STALE);
}

static void testNetworkUp() {
    printf("WiFi 재접속은 즉시 재동기화하되, 연속 요청 최소 간격(1분)을 지킨다\n");
    TimeSyncState s = baseState();
    s.networkChanged = true;
    s.nowMs = PERIODIC_MS - 1;          // 주기에는 안 걸리지만 네트워크는 복구됨
    expectAction("재접속 + 1분 경과 → NETWORK_UP", s, T, TIME_SYNC_NETWORK_UP);

    s.lastAttemptMs = 0;
    s.nowMs = MIN_GAP_MS - 1000;        // 59초 전에 시도했다
    expectAction("재접속 + 59초 전 시도 → 대기", s, T, TIME_SYNC_NONE);

    s.nowMs = MIN_GAP_MS;
    expectAction("재접속 + 1분 경과 → NETWORK_UP", s, T, TIME_SYNC_NETWORK_UP);
}

static void testMillisRollover() {
    printf("millis() 49.7일 랩 구간에서도 경과 시간이 올바르게 계산되어야 한다\n");
    TimeSyncState s = baseState();
    // 마지막 성공이 랩 직전(0xFFFFFC18 = -1000ms), 지금은 랩 후 500ms → 경과 1500ms
    s.lastSyncMs = 0xFFFFFC18UL;
    s.attempted = false;
    s.nowMs = 500UL;
    expectAction("랩 직후 → 아직 주기 아님", s, T, TIME_SYNC_NONE);

    // 랩 직전 성공 + 지금 6시간 경과 (모듈러 뺄셈)
    s.lastSyncMs = 0xFFFFFFFFUL;         // 랩 1ms 전
    s.nowMs = PERIODIC_MS - 1;           // 6시간 - 1ms  → 경과 = 6시간
    expectAction("랩을 넘겨 6시간 경과 → PERIODIC", s, T, TIME_SYNC_PERIODIC);
}

static void testClockRender() {
    printf("1초 렌더 트리거: tm_sec 변화가 기준, 시각 무효일 때만 1초 폴백\n");
    expectBool("tm_sec 변화 없음 → 렌더 안 함", clockRenderDue(false, true, false, false), false);
    expectBool("tm_sec 변화 → 렌더", clockRenderDue(false, true, true, false), true);
    expectBool("강제 갱신 → 렌더", clockRenderDue(true, true, false, false), true);
    expectBool("시각 무효 + 폴백 아님 → 렌더 안 함", clockRenderDue(false, false, false, false), false);
    expectBool("시각 무효 + 폴백 도래 → 렌더", clockRenderDue(false, false, false, true), true);

    expectBool("첫 렌더(last=-1) → 항상 참", secondTickDue(-1, 0), true);
    expectBool("같은 초 → 거짓", secondTickDue(5, 5), false);
    expectBool("59→0 랩 → 참", secondTickDue(59, 0), true);
    expectBool("epoch 초 변화 → 참", secondTickDue(1793500000LL, 1793500001LL), true);
}

static void testValidityAndDrift() {
    printf("시각 유효 판정과 드리프트 부호 규약\n");
    expectBool("epoch 0 (1970년) 무효", localTimeIsValid(0), false);
    expectBool("2020-01-01 1초 전 무효", localTimeIsValid(TIME_VALID_EPOCH - 1), false);
    expectBool("2020-01-01T00:00:00Z 유효", localTimeIsValid(TIME_VALID_EPOCH), true);
    expectBool("2026년 시각 유효", localTimeIsValid(1793500000LL), true);

    // 부호 규약: + = 로컬이 서버보다 느렸다(앞으로 당겨짐)
    expectInt("서버가 1.5ms 앞 → +1ms", ntpDriftMs(1001500LL, 1000000LL), 1);
    expectInt("서버가 1.5ms 뒤 → -1ms", ntpDriftMs(998500LL, 1000000LL), -1);
    expectInt("서버가 2.5초 앞 → +2500ms", ntpDriftMs(2500000LL, 0LL), 2500);
}

int main() {
    testWifiDown();
    testRetry();
    testPeriodicAndStale();
    testNetworkUp();
    testMillisRollover();
    testClockRender();
    testValidityAndDrift();

    printf("\n%d passed / %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
