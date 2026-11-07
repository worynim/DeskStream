// worynim@gmail.com
/**
 * @file time_sync.h
 * @brief NTP 재동기화 감독 서비스 (시각 누적 오차 방지)
 * @details 부팅 때 한 번 맞춘 시각을 그대로 믿지 않고, 주기 재동기화 · WiFi 재접속 ·
 *          동기 실패 재시도를 스스로 판정해 SNTP를 다시 시작한다. 판정 규칙은
 *          time_sync_policy.h(순수 함수)에 있고, 이 클래스는 I/O(SNTP/WiFi/Serial)만
 *          담당한다.
 * @note [SYNC] 원본: Hangeul_Clock/time_sync.h — 현재 바이트 단위로 동일.
 *
 * @note "왜 필요한가"의 배경과 수치는 config.h의 [2] NTP 절과
 *       time_sync_policy.h의 파일 주석 참조.
 */
#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <Arduino.h>
#include "time_sync_policy.h"

class TimeSyncManager {
public:
    /**
     * @brief 감독 초기화 — SNTP 내부 재요청 주기를 낮추고 WiFi 상태 추적을 시작한다
     * @note TZ 적용과 최초 SNTP 시작은 하지 않는다. `applyTimezone()`이 그 역할이다
     *       (부팅 경로와 웹 타임존 변경 경로가 같은 함수를 쓰도록 한 곳에 둔다).
     */
    void begin();

    /**
     * @brief 설정된 POSIX TZ를 적용하고 SNTP를 (재)시작한다 = 즉시 동기화 요청
     * @details lwIP는 UTC를 내부 보관하고 localtime() 호출 시점에 TZ를 적용한다.
     *          configTzTime()이 setenv("TZ", tz) + tzset()까지 해 준다.
     *          configTime(오프셋, ...)은 절대 쓰지 않는다 — 코어가 오프셋을
     *          POSIX TZ로 강제 변환해 어떤 타임존을 골라도 UTC로 표시된다.
     *          입력 검증은 config_manager(로드)·web_manager(입력)가 이미 수행한다.
     */
    void applyTimezone();

    /** @brief 메인 루프에서 자주 호출한다 (내부에서 250ms로 스로틀). */
    void loop();

    /**
     * @brief SNTP 응답을 받았음을 알린다 — sntp_sync_time() 오버라이드가 호출
     * @param driftMs 보정 전 로컬 시각과 서버 시각의 차이 (양수 = 로컬이 느렸다)
     * @note lwIP tcpip 태스크에서 호출되므로 여기서는 공유 변수만 갱신하고,
     *       로깅·정책 판정은 loop()(메인 태스크)가 한다.
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

    bool _everSynced = false;      // 이번 부팅에서 한 번이라도 성공했는가
    bool _attempted = false;       // 마지막 시도 시각이 유효한가
    bool _wifiWasUp = false;       // 직전 감시 시점의 WiFi 상태 (엣지 검출)
    bool _networkChanged = false;  // 재접속 후 아직 재동기화 못함
    uint32_t _lastSyncMs = 0;
    uint32_t _lastAttemptMs = 0;
    uint32_t _wifiRetryMs = 0;
    uint32_t _syncCount = 0;
    uint32_t _lastLoopMs = 0;
    int32_t _lastDriftMs = 0;

    // tcpip 태스크 → 메인 태스크 전달용 (32비트 원자 쓰기, 단일 기록자)
    volatile bool _pendingSync = false;
    volatile int32_t _pendingDriftMs = 0;
    volatile uint32_t _pendingSyncAtMs = 0;
};

extern TimeSyncManager timeSync;

#endif
