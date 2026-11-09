// worynim@gmail.com
/**
 * @file ds_time_sync.h
 * @brief NTP 재동기화 감독 서비스 (시각 누적 오차 방지)
 * @details 부팅 때 한 번 맞춘 시각을 그대로 믿지 않고, 주기 재동기화 · WiFi 재접속 ·
 *          동기 실패 재시도를 스스로 판정해 SNTP를 다시 시작한다. 판정 규칙은
 *          ds_time_sync_policy.h(순수 함수)에 있고, 이 클래스는 I/O(SNTP/WiFi/Serial)만 담당한다.
 *          WiFi 재접속 시도 자체는 ds::WifiService::loop()가 한다.
 *
 * @note 배경 (2026-11-07 사용자 보고 "계속 켜 놓으니까 십여 초 틀어진다"):
 *       ESP32-C3의 시스템 시각은 40MHz XTAL에서 파생된 free-running 카운터다
 *       (전형 ±10~30ppm = 하루 1~3초). 부팅 시 NTP 1회 동기화만 믿으면 오차가 누적된다.
 * @note 출처: Hangeul_Clock/time_sync.* (v2.10.0)
 */
#ifndef DS_TIME_SYNC_H
#define DS_TIME_SYNC_H

#include <Arduino.h>
#include "ds_time_sync_policy.h"

namespace ds {

struct TimeSyncConfig {
    const char* servers[3] = {"kr.pool.ntp.org", "pool.ntp.org", "time.nist.gov"};
    uint32_t sntpPollMs = 3600000UL;   ///< SNTP 내부 재요청 주기 (코어 기본 3시간 → 1시간 안전망)
    /** 감독 정책: 주기 6h · 오래됨 12h · 미동기 재시도 30s · 연속 요청 최소 간격 1분 */
    TimeSyncThresholds thresholds = {21600000UL, 43200000UL, 30000UL, 60000UL};
    /** 적용할 POSIX TZ 문자열을 돌려준다 (검증된 값이어야 한다 — ds::isValidTimezone) */
    const char* (*tzProvider)() = nullptr;
};

class TimeSync {
public:
    /**
     * @brief 감독 초기화 — SNTP 내부 재요청 주기를 낮추고 WiFi 상태 추적을 시작한다
     * @note TZ 적용과 최초 SNTP 시작은 하지 않는다. applyTimezone()이 그 역할이다
     *       (부팅 경로와 TZ 변경 경로가 같은 함수를 쓰도록 한 곳에 둔다).
     */
    void begin(const TimeSyncConfig& cfg);

    /**
     * @brief tzProvider의 POSIX TZ를 적용하고 SNTP를 (재)시작한다 = 즉시 동기화 요청
     * @details configTzTime()이 setenv("TZ") + tzset()까지 해 준다.
     *          configTime(오프셋, ...)은 절대 쓰지 않는다 — 코어가 오프셋을 POSIX TZ로 강제
     *          변환해 어떤 타임존을 골라도 UTC로 표시된다.
     */
    void applyTimezone();

    /** @brief 메인 루프에서 자주 호출한다 (내부에서 250ms로 스로틀) */
    void loop();

    /**
     * @brief SNTP 응답을 받았음을 알린다 — sntp_sync_time() 오버라이드가 호출
     * @note lwIP tcpip 태스크에서 호출되므로 공유 변수만 갱신하고, 로깅·판정은 loop()가 한다.
     */
    void notifySntpResponse(int32_t driftMs);

    bool everSynced() const { return _everSynced; }
    uint32_t syncCount() const { return _syncCount; }
    int32_t lastDriftMs() const { return _lastDriftMs; }
    /** @brief 마지막 성공 동기화 후 경과 초 (성공 기록이 없으면 0xFFFFFFFF) */
    uint32_t syncAgeSec() const;

private:
    void startSntp(const char* reason);
    static const char* actionName(TimeSyncAction action);

    TimeSyncConfig _cfg;
    bool _everSynced = false;      // 이번 부팅에서 한 번이라도 성공했는가
    bool _attempted = false;       // 마지막 시도 시각이 유효한가
    bool _wifiWasUp = false;       // 직전 감시 시점의 WiFi 상태 (엣지 검출)
    bool _networkChanged = false;  // 재접속 후 아직 재동기화 못함
    uint32_t _lastSyncMs = 0;
    uint32_t _lastAttemptMs = 0;
    uint32_t _syncCount = 0;
    uint32_t _lastLoopMs = 0;
    int32_t _lastDriftMs = 0;

    // tcpip 태스크 → 메인 태스크 전달용 (32비트 원자 쓰기, 단일 기록자)
    volatile bool _pendingSync = false;
    volatile int32_t _pendingDriftMs = 0;
    volatile uint32_t _pendingSyncAtMs = 0;
};

}  // namespace ds

#endif
