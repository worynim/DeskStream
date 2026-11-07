// worynim@gmail.com
/**
 * @file time_sync_policy.h
 * @brief NTP 재동기화 시점 · 1초 렌더 트리거 규칙 (순수 함수 — Arduino 의존 없음)
 * @details 실제 SNTP 호출은 time_sync.cpp가 담당한다. 그중 "언제 다시 동기화할 것인가"와
 *          "언제 1초 렌더를 돌릴 것인가"는 버그를 만들기 쉬운 순수 규칙이므로 여기다
 *          분리해 호스트 단위 테스트(test/test_time_sync_policy.cpp)가 가능하게 한다.
 *
 * @note 왜 이 모듈이 필요한가 (2026-11-07 사용자 보고)
 *       "한글 시계를 계속 켜 놓으니까 시간이 조금씩 틀어져서 십여 초 차이가 난다."
 *       ESP32-C3의 시스템 시각은 40MHz 크리스털에서 파생된 free-running 카운터다
 *       (전형 ±10~30ppm = 하루 1~3초). 부팅 때 NTP로 한 번 맞춘 뒤 재동기화가
 *       실제로 도는지 확인하지 않으면 오차가 그대로 누적되어 수일~수주 뒤
 *       "십여 초"가 된다. 그래서 감독(supervision)을 코드에 명시적으로 둔다.
 *
 * @note 시각 비교는 전부 `uint32_t` 뺄셈이다. millis()가 49.7일마다 0으로 돌아가도
 *       `now - last`는 모듈러 연산으로 올바른 경과 시간이 되므로, 절대 `now > last`로
 *       판정하지 않는다 (그렇게 하면 랩 구간에서 감독이 멈춘다).
 */
#ifndef TIME_SYNC_POLICY_H
#define TIME_SYNC_POLICY_H

#include <stdint.h>

/**
 * @brief 시스템 시각이 유효(NTP 동기 완료)하다고 볼 epoch 초 하한
 * @details 1577836800 = 2020-01-01T00:00:00Z. NTP 동기 전의 시스템 시각은 1970년(0 부근)이므로
 *          이 하한으로 충분히 구분되며, RTC 백업이 없는 이 하드웨어에서는 부팅 직후 항상
 *          무효 상태로 시작한다.
 *          `tm_year`(년도)가 아니라 **epoch 초**를 기준으로 두는 이유: 메인 루프는 매
 *          iteration `time()`만 읽고(비차단·저비용) `localtime_r()`은 실제 렌더가 일어나는
 *          초에만 호출하기 위해서다. 판정 기준은 이 한 곳에 고정한다.
 */
static const int64_t TIME_VALID_EPOCH = 1577836800LL;   // 2020-01-01T00:00:00Z

/** @brief 재동기화를 요청해야 하는 이유 */
enum TimeSyncAction : uint8_t {
    TIME_SYNC_NONE = 0,      // 아직 아님
    TIME_SYNC_RETRY,         // 아직 한 번도 성공 못 함 → 실패 재시도 간격 도래
    TIME_SYNC_PERIODIC,      // 주기(6시간) 도래
    TIME_SYNC_STALE,         // 성공 후 너무 오래됨 → 감독이 끊겼다는 신호
    TIME_SYNC_NETWORK_UP,    // WiFi가 끊겼다가 다시 붙음 → 즉시
};

/** @brief 감독 루프가 넘기는 현재 상태 (모든 시각은 millis() 기준, uint32_t) */
struct TimeSyncState {
    bool wifiUp;          // STA가 AP에 연결되어 있는가
    bool everSynced;      // 이번 부팅에서 NTP 성공 기록이 있는가
    bool networkChanged;  // 재접속 후 아직 재동기화하지 못했는가
    bool attempted;       // 마지막 시도 시각(lastAttemptMs)이 유효한가
    uint32_t nowMs;
    uint32_t lastSyncMs;    // 마지막 성공 시각 (everSynced가 false면 무시)
    uint32_t lastAttemptMs; // 마지막 시도(실패 포함) 시각
};

/** @brief 정책 임계값 (config.h의 매크로를 그대로 넘긴다) */
struct TimeSyncThresholds {
    uint32_t periodicMs;  // 주기 재동기화
    uint32_t staleMs;     // 이만큼 성공이 없으면 즉시
    uint32_t retryMs;     // 미동기 상태 재시도
    uint32_t minGapMs;    // 연속 요청 최소 간격
};

/** @brief 마지막 시도로부터 최소 간격이 지났는가 (시도한 적 없으면 참) */
static inline bool timeSyncMinGapElapsed(const TimeSyncState& s, const TimeSyncThresholds& t) {
    return !s.attempted || (uint32_t)(s.nowMs - s.lastAttemptMs) >= t.minGapMs;
}

/**
 * @brief 지금 재동기화해야 하는가, 한다면 어떤 이유인가
 * @details 우선순위: 네트워크 복구 > (미동기) 재시도 > 오래됨 > 주기.
 *          WiFi가 끊겨 있으면 아무것도 하지 않는다 — 요청은 실패하고 DNS만 낭비된다.
 *          재시도(RETRY)는 자기 간격(retryMs)으로만 판정하고, 나머지는 연속 요청
 *          방지를 위해 minGap을 함께 본다 (예: 주기 재동기화 직후 WiFi가 다시 붙어도
 *          1분 안에는 두 번 요청하지 않는다 — 1분 뒤 networkChanged가 남아 있으므로
 *          그때 요청된다).
 */
static inline TimeSyncAction timeSyncActionDue(const TimeSyncState& s, const TimeSyncThresholds& t) {
    if (!s.wifiUp) return TIME_SYNC_NONE;

    TimeSyncAction action = TIME_SYNC_NONE;
    if (s.networkChanged) {
        action = TIME_SYNC_NETWORK_UP;
    } else if (!s.everSynced) {
        if (!s.attempted || (uint32_t)(s.nowMs - s.lastAttemptMs) >= t.retryMs) {
            action = TIME_SYNC_RETRY;
        }
    } else {
        const uint32_t age = (uint32_t)(s.nowMs - s.lastSyncMs);
        if (age >= t.staleMs) {
            action = TIME_SYNC_STALE;
        } else if (age >= t.periodicMs) {
            action = TIME_SYNC_PERIODIC;
        }
    }

    if (action == TIME_SYNC_NONE || action == TIME_SYNC_RETRY) return action;
    if (!timeSyncMinGapElapsed(s, t)) return TIME_SYNC_NONE;
    return action;
}

/**
 * @brief 이번 loop에서 시계를 다시 그려야 하는가
 * @param force          강제 갱신 (버튼/웹 설정 변경)
 * @param timeValid      NTP 동기 완료 여부
 * @param secondChanged  초(epoch 초 또는 `tm_sec`)가 직전 렌더와 다른가 (timeValid일 때만 의미)
 * @param fallbackDue    시각이 무효일 때 1초 폴백 타이머가 찼는가
 *
 * @details **이 함수가 "1초 갱신"의 단일 진실원이다.** 이전 구현은
 *          `millis()` 1000ms 주기로 갱신했는데, 그 주기는 실제 초 경계와 무관해
 *          (1) 화면이 최대 1초 늦게 바뀌고 (2) 주기가 초 경계를 지나치는 위상이
 *          고정되지 않아 매초 렌더 시점이 흔들렸다. 값 자체는 시스템 시각에서
 *          읽으므로 누적 오차는 아니지만, "시계가 늦다"는 체감과 초 표시가
 *          건너뛰거나 겹치는 현상을 만든다. → 초(epoch 초) 변화를 직접 감지한다.
 *
 *          시각이 무효(NTP 미동기)일 때는 보여줄 시각이 없으므로 1초 폴백으로
 *          "Time Sync Error!"를 갱신한다.
 */
static inline bool clockRenderDue(bool force, bool timeValid, bool secondChanged, bool fallbackDue) {
    if (force) return true;
    if (timeValid) return secondChanged;
    return fallbackDue;
}

/** @brief 초가 바뀌었는가 — epoch 초 또는 `tm_sec` 어느 쪽으로 넘겨도 된다 (초기값 -1이면 항상 참) */
static inline bool secondTickDue(int64_t lastSecond, int64_t currentSecond) {
    return lastSecond != currentSecond;
}

/** @brief 시스템 시각(epoch 초)이 유효한가 = NTP 동기 완료 */
static inline bool localTimeIsValid(int64_t epochSec) {
    return epochSec >= TIME_VALID_EPOCH;
}

/**
 * @brief NTP 응답과 로컬 시각의 차이를 ms로 계산한다
 * @param serverUs NTP 서버가 알려준 시각 (µs)
 * @param localUs  응답을 적용하기 직전의 로컬 시각 (µs)
 * @return 양수 = 로컬 시계가 서버보다 **느렸다**(앞으로 당겨짐), 음수 = 빨랐다.
 * @details 부호 규약을 한 곳에 고정한다. 로그에서 부호를 반대로 읽으면
 *          "크리스털이 빠른가 느린가"를 진단할 수 없으므로 테스트로 고정한다.
 */
static inline int32_t ntpDriftMs(int64_t serverUs, int64_t localUs) {
    return (int32_t)((serverUs - localUs) / 1000);
}

#endif
