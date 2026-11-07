// worynim@gmail.com
/**
 * @file time_sync.cpp
 * @brief NTP 재동기화 감독 서비스 구현
 * @details 정책 판정(time_sync_policy.h)에 따라 SNTP를 다시 시작하고, 응답 드리프트를
 *          기록/로그한다. 시각을 적용하는 훅(sntp_sync_time)을 대체해 "얼마나 틀어져
 *          있었는지"를 남긴다 — 누적 오차의 원인이 크리스털인지 네트워크 단절인지
 *          구분할 수 있는 유일한 진단 근거다.
 *
 * @note [SYNC] 원본: Hangeul_Clock/time_sync.cpp — 현재 바이트 단위로 동일.
 *
 * @note [배경] 2026-11-07 사용자 보고: "계속 켜 놓으니까 십여 초 틀어진다."
 *       ESP32-C3의 시스템 시각은 40MHz XTAL에서 파생된 free-running 카운터다
 *       (전형 ±10~30ppm = 하루 1~3초). 부팅 시 NTP 1회 동기화만 믿으면 오차가
 *       누적되고, 네트워크가 한 번 끊겨 SNTP 재폴링이 멈추면 조용히 free-run이
 *       된다. 그래서 (1) 주기 강제 재동기화 (2) WiFi 복구 시 즉시 재동기화
 *       (3) 성공 감시·실패 재시도 (4) 드리프트 기록을 코드로 명시한다.
 */
#include "time_sync.h"

#include <WiFi.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>

#include "config.h"
#include "config_manager.h"

TimeSyncManager timeSync;

// config.h의 정책 상수를 정책 구조체로 옮긴다 (테스트는 이 구조체를 직접 만든다).
static const TimeSyncThresholds kThresholds = {
    NTP_RESYNC_INTERVAL_MS,   // periodicMs
    NTP_SYNC_STALE_MS,        // staleMs
    NTP_RETRY_INTERVAL_MS,    // retryMs
    NTP_MIN_RESYNC_GAP_MS     // minGapMs
};

void TimeSyncManager::begin() {
    // lwIP SNTP의 내부 재요청 주기를 1시간으로 낮춰 둔다.
    //   - 코어 기본값은 CONFIG_LWIP_SNTP_UPDATE_DELAY = 3시간(10800000ms)이다.
    //   - 이 감독 루프(6시간 주기)와 별개로 도는 안전망이므로 짧게 두는 손해가 없다.
    //   - 이 함수는 다음 폴링부터 적용되며, SNTP 시작 전에 불러도 된다.
    esp_sntp_set_sync_interval(NTP_SNTP_POLL_MS);
    _wifiWasUp = (WiFi.status() == WL_CONNECTED);
    Serial.printf("[NTP] Supervisor start: periodic=%luh stale=%luh\n",
                  (unsigned long)(NTP_RESYNC_INTERVAL_MS / 3600000UL),
                  (unsigned long)(NTP_SYNC_STALE_MS / 3600000UL));
}

void TimeSyncManager::applyTimezone() {
    startSntp("tz");
}

void TimeSyncManager::startSntp(const char* reason) {
    const char* tz = configManager.get().timezone;
    // configTzTime() = SNTP 서버 설정 + SNTP 시작 + setenv("TZ") + tzset().
    //   이미 SNTP가 돌고 있으면 내부에서 stop → init 하므로 "즉시 재요청"이 된다.
    //   (idf/lwIP: sntp_init()이 첫 요청을 바로 보낸다 — 최대 5초 난수 지연 후)
    configTzTime(tz, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
    _attempted = true;
    _lastAttemptMs = millis();
    Serial.printf("[NTP] resync(%s) TZ=%s\n", reason, tz);
}

void TimeSyncManager::loop() {
    const uint32_t now = millis();
    // WiFi.status()/Serial 부담을 줄이기 위한 스로틀. 250ms면 초 단위 정확도에
    //   영향이 없고(재동기화는 시간 단위), 응답 로깅도 체감 지연이 없다.
    if ((uint32_t)(now - _lastLoopMs) < 250u) return;
    _lastLoopMs = now;

    // 1) SNTP 응답 결과 소비 (tcpip 태스크가 남긴 volatile 플래그)
    if (_pendingSync) {
        _pendingSync = false;
        const bool firstSync = !_everSynced;
        // 직전 성공 이후 경과 시간 — 재동기화 주기가 실제로 지켜졌는지 확인하는 값.
        const uint32_t gapSec = firstSync
                                    ? (uint32_t)_pendingSyncAtMs / 1000u
                                    : (uint32_t)(_pendingSyncAtMs - _lastSyncMs) / 1000u;
        _everSynced = true;
        _networkChanged = false;
        _syncCount++;
        _lastSyncMs = _pendingSyncAtMs;
        // 부팅 첫 동기화는 비교 대상이 없다(로컬 시계가 1970년) → 드리프트를 0으로 둔다.
        //   이 값을 그대로 로그에 찍으면 "수십억 ms 틀렸다"는 무의미한 줄이 남는다.
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
    //    자동 재접속(코어 기본 true)이 실패한 경우를 대비해 직접 재접속도 시도한다.
    const bool wifiUp = (WiFi.status() == WL_CONNECTED);
    if (wifiUp && !_wifiWasUp) {
        _networkChanged = true;
        Serial.println("[NTP] WiFi up -> resync scheduled");
    } else if (!wifiUp && _wifiWasUp) {
        Serial.println("[NTP] WiFi lost -> clock free-runs until reconnect");
    }
    if (!wifiUp && (uint32_t)(now - _wifiRetryMs) >= NTP_WIFI_RETRY_MS) {
        _wifiRetryMs = now;
        WiFi.reconnect();
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

    const TimeSyncAction action = timeSyncActionDue(s, kThresholds);
    if (action == TIME_SYNC_NONE) return;
    if (action == TIME_SYNC_NETWORK_UP) _networkChanged = false;
    startSntp(actionName(action));
}

void TimeSyncManager::notifySntpResponse(int32_t driftMs) {
    _pendingDriftMs = driftMs;
    _pendingSyncAtMs = millis();
    _pendingSync = true;
}

uint32_t TimeSyncManager::syncAgeSec() const {
    if (!_everSynced) return 0xFFFFFFFFu;
    return (uint32_t)(millis() - _lastSyncMs) / 1000u;
}

const char* TimeSyncManager::actionName(TimeSyncAction action) {
    switch (action) {
        case TIME_SYNC_RETRY:      return "retry";
        case TIME_SYNC_PERIODIC:   return "periodic";
        case TIME_SYNC_STALE:      return "stale";
        case TIME_SYNC_NETWORK_UP: return "wifi-up";
        default:                   return "none";
    }
}

/**
 * @brief IDF SNTP의 시각 적용 훅을 대체한다 (esp_sntp.c의 weak 심볼)
 * @details ESP-IDF 문서가 공식적으로 허용하는 확장점이다:
 *          "This function is a weak-linked function. It is possible to replace all SNTP
 *           update functionality by placing a sntp_sync_time() function in the app firmware."
 *          기본 구현은 sync mode(IMMED/SMOOTH)에 따라 settimeofday/adjtime을 부르는데,
 *          이 펌웨어는 그 모드를 바꾸지 않으므로 IMMED 경로만 재현하고, 대신
 *          보정 직전의 드리프트를 측정해 로그로 남긴다.
 *
 * @note 호출 문맥은 lwIP tcpip 태스크다. 여기서 Serial/정책 판정을 하면 tcpip 스택을
 *       붙잡을 수 있으므로 공유 변수만 쓰고 즉시 돌아간다.
 */
extern "C" void sntp_sync_time(struct timeval* tv) {
    struct timeval now;
    gettimeofday(&now, nullptr);
    const int64_t localUs = (int64_t)now.tv_sec * 1000000LL + (int64_t)now.tv_usec;
    const int64_t serverUs = (int64_t)tv->tv_sec * 1000000LL + (int64_t)tv->tv_usec;
    timeSync.notifySntpResponse(ntpDriftMs(serverUs, localUs));

    settimeofday(tv, nullptr);                                 // 즉시 보정 (step)
    sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);          // 상태 조회용
}
