// worynim@gmail.com
/**
 * @file ds_time_sync.cpp
 * @brief NTP 재동기화 감독 서비스 구현
 * @details 정책 판정(ds_time_sync_policy.h)에 따라 SNTP를 다시 시작하고, 응답 드리프트를
 *          기록한다. 시각을 적용하는 훅(sntp_sync_time)을 대체해 "얼마나 틀어져 있었는지"를
 *          남긴다 — 누적 오차의 원인이 크리스털인지 네트워크 단절인지 구분하는 진단 근거다.
 */
#include "ds_time_sync.h"

#include <WiFi.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>

namespace ds {

// sntp_sync_time()(tcpip 태스크)이 응답을 넘길 대상 — begin()한 인스턴스 (보드당 1개)
static TimeSync* s_active = nullptr;

void TimeSync::begin(const TimeSyncConfig& cfg) {
    _cfg = cfg;
    s_active = this;
    // lwIP SNTP의 내부 재요청 주기를 낮춰 둔다 (코어 기본 CONFIG_LWIP_SNTP_UPDATE_DELAY = 3시간).
    //   이 감독 루프와 별개로 도는 안전망이다. SNTP 시작 전에 불러도 다음 폴링부터 적용된다.
    esp_sntp_set_sync_interval(_cfg.sntpPollMs);
    _wifiWasUp = (WiFi.status() == WL_CONNECTED);
    Serial.printf("[NTP] Supervisor start: periodic=%luh stale=%luh\n",
                  (unsigned long)(_cfg.thresholds.periodicMs / 3600000UL),
                  (unsigned long)(_cfg.thresholds.staleMs / 3600000UL));
}

void TimeSync::applyTimezone() {
    startSntp("tz");
}

void TimeSync::startSntp(const char* reason) {
    const char* tz = _cfg.tzProvider ? _cfg.tzProvider() : "UTC0";
    // configTzTime() = SNTP 서버 설정 + SNTP 시작 + setenv("TZ") + tzset().
    //   이미 SNTP가 돌고 있으면 내부에서 stop → init 하므로 "즉시 재요청"이 된다.
    configTzTime(tz, _cfg.servers[0], _cfg.servers[1], _cfg.servers[2]);
    _attempted = true;
    _lastAttemptMs = millis();
    Serial.printf("[NTP] resync(%s) TZ=%s\n", reason, tz);
}

void TimeSync::loop() {
    const uint32_t now = millis();
    // WiFi.status()/Serial 부담을 줄이기 위한 스로틀 (재동기화는 시간 단위라 영향 없음)
    if ((uint32_t)(now - _lastLoopMs) < 250u) return;
    _lastLoopMs = now;

    // 1) SNTP 응답 결과 소비 (tcpip 태스크가 남긴 volatile 플래그)
    if (_pendingSync) {
        _pendingSync = false;
        const bool firstSync = !_everSynced;
        const uint32_t gapSec = firstSync
                                    ? (uint32_t)_pendingSyncAtMs / 1000u
                                    : (uint32_t)(_pendingSyncAtMs - _lastSyncMs) / 1000u;
        _everSynced = true;
        _networkChanged = false;
        _syncCount++;
        _lastSyncMs = _pendingSyncAtMs;
        // 부팅 첫 동기화는 비교 대상이 없다(로컬 시계가 1970년) → 드리프트를 0으로 둔다.
        _lastDriftMs = firstSync ? 0 : _pendingDriftMs;
        if (firstSync) {
            Serial.printf("[NTP] sync #%lu (boot, %lus after reset)\n",
                          (unsigned long)_syncCount, (unsigned long)gapSec);
        } else {
            // 부호 규약: +면 보정 전 로컬 시계가 서버보다 느렸다(앞으로 당겨짐).
            Serial.printf("[NTP] sync #%lu drift=%+ldms gap=%lus\n",
                          (unsigned long)_syncCount, (long)_lastDriftMs, (unsigned long)gapSec);
        }
    }

    // 2) WiFi 감시: 끊기면 시각은 free-run이 되므로 복구 즉시 재동기화한다.
    const bool wifiUp = (WiFi.status() == WL_CONNECTED);
    if (wifiUp && !_wifiWasUp) {
        _networkChanged = true;
        Serial.println("[NTP] WiFi up -> resync scheduled");
    } else if (!wifiUp && _wifiWasUp) {
        Serial.println("[NTP] WiFi lost -> clock free-runs until reconnect");
    }
    _wifiWasUp = wifiUp;

    // 3) 정책 판정 → 필요하면 SNTP 재시작
    TimeSyncState s;
    s.wifiUp = wifiUp;
    s.everSynced = _everSynced;
    s.networkChanged = _networkChanged;
    s.attempted = _attempted;
    s.nowMs = now;
    s.lastSyncMs = _lastSyncMs;
    s.lastAttemptMs = _lastAttemptMs;

    const TimeSyncAction action = timeSyncActionDue(s, _cfg.thresholds);
    if (action == TIME_SYNC_NONE) return;
    if (action == TIME_SYNC_NETWORK_UP) _networkChanged = false;
    startSntp(actionName(action));
}

void TimeSync::notifySntpResponse(int32_t driftMs) {
    _pendingDriftMs = driftMs;
    _pendingSyncAtMs = millis();
    _pendingSync = true;
}

uint32_t TimeSync::syncAgeSec() const {
    if (!_everSynced) return 0xFFFFFFFFu;
    return (uint32_t)(millis() - _lastSyncMs) / 1000u;
}

const char* TimeSync::actionName(TimeSyncAction action) {
    switch (action) {
        case TIME_SYNC_RETRY:      return "retry";
        case TIME_SYNC_PERIODIC:   return "periodic";
        case TIME_SYNC_STALE:      return "stale";
        case TIME_SYNC_NETWORK_UP: return "wifi-up";
        default:                   return "none";
    }
}

}  // namespace ds

/**
 * @brief IDF SNTP의 시각 적용 훅을 대체한다 (esp_sntp.c의 weak 심볼)
 * @details ESP-IDF가 공식적으로 허용하는 확장점이다 ("It is possible to replace all SNTP update
 *          functionality by placing a sntp_sync_time() function in the app firmware").
 *          기본 구현의 IMMED 경로만 재현하고, 대신 보정 직전의 드리프트를 측정해 넘긴다.
 * @note 호출 문맥은 lwIP tcpip 태스크다 — 공유 변수만 쓰고 즉시 돌아간다.
 */
extern "C" void sntp_sync_time(struct timeval* tv) {
    struct timeval now;
    gettimeofday(&now, nullptr);
    const int64_t localUs = (int64_t)now.tv_sec * 1000000LL + (int64_t)now.tv_usec;
    const int64_t serverUs = (int64_t)tv->tv_sec * 1000000LL + (int64_t)tv->tv_usec;
    if (ds::s_active) ds::s_active->notifySntpResponse(ds::ntpDriftMs(serverUs, localUs));

    settimeofday(tv, nullptr);                                 // 즉시 보정 (step)
    sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);          // 상태 조회용
}
