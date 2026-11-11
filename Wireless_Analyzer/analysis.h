// worynim@gmail.com
/**
 * @file analysis.h
 * @brief 측정값 계산 — AP 테이블·RSSI 히스토리, 채널 혼잡 점수, 추천 채널, 추적 부저 간격, 512px 직선 자르기
 * @details Arduino 의존 없는 순수 로직이라 호스트에서 테스트한다 (test/test_analysis.cpp).
 */
#ifndef WA_ANALYSIS_H
#define WA_ANALYSIS_H

#include <stdint.h>

#include "traffic.h"

namespace wa {

constexpr int kMinChannel = 1;
constexpr int kMaxChannel = 13;
constexpr int kApMax = 48;       ///< AP 테이블 크기 — 가득 차면 가장 약한 AP를 밀어낸다
constexpr int kApHistLen = 60;   ///< AP마다 남기는 RSSI 기록 수 (스캔 1회 = 1개)
constexpr int8_t kNoSample = -128;   ///< 그 스캔에서 안 보임 (그래프에서 끊김)

/** RSSI 기록 링버퍼 — 가득 차면 가장 오래된 것을 덮어쓴다 */
template <int N>
class RssiRing {
public:
    void push(int8_t v) {
        _v[(_head + _len) % N] = v;
        if (_len < N) _len++;
        else _head = (_head + 1) % N;
    }
    void clear() { _head = _len = 0; }
    int size() const { return _len; }
    /** k = 0 이 가장 오래된 기록 */
    int8_t at(int k) const { return _v[(_head + k) % N]; }

private:
    int8_t _v[N];
    int _head = 0, _len = 0;
};

using ApHistory = RssiRing<kApHistLen>;

constexpr int kTrackLen = 190;   ///< 추적 모드 기록 수 (그래프 380px ÷ 2px)
using TrackHistory = RssiRing<kTrackLen>;

/** 최근 lastN개 기록 중 kNoSample을 뺀 최소·최대·평균 */
struct RingStats {
    int count;   ///< 유효 기록 수 (0이면 나머지 값은 의미 없음)
    int8_t minDbm, maxDbm;
    float avgDbm;
};

template <int N>
RingStats ringStats(const RssiRing<N>& r, int lastN) {
    RingStats st = {0, 0, 0, 0};
    long sum = 0;
    const int start = r.size() > lastN ? r.size() - lastN : 0;
    for (int k = start; k < r.size(); k++) {
        const int8_t v = r.at(k);
        if (v == kNoSample) continue;
        if (st.count == 0 || v < st.minDbm) st.minDbm = v;
        if (st.count == 0 || v > st.maxDbm) st.maxDbm = v;
        sum += v;
        st.count++;
    }
    if (st.count) st.avgDbm = (float)sum / st.count;
    return st;
}

/**
 * 스파크라인 세로 범위 — 기록의 최소~최대에 맞추되 폭이 minSpan(dB)보다 좁으면 가운데를 기준으로 넓힌다.
 * 유효 기록이 없으면 lo/hi를 바꾸지 않는다.
 */
void sparkRange(const RingStats& st, float minSpan, float& lo, float& hi);

/** 스캔으로 본 AP 하나 */
struct ApInfo {
    uint8_t bssid[6];
    char ssid[33];          ///< UTF-8, 숨김 SSID면 빈 문자열
    uint8_t channel;        ///< 1~13 (그 밖은 테이블에 넣지 않음)
    int8_t rssi;            ///< 마지막 스캔 RSSI (dBm)
    uint8_t enc;            ///< wifi_auth_mode_t 값 (0 = 열림)
    uint32_t lastSeenMs;
};

/** BSSID로 구분하는 AP 목록 — 스캔마다 흔들리는 AP 수를 흡수하려고 ttl 동안 유지한다 */
class ApTable {
public:
    /** 새로 보거나 갱신. 채널이 범위 밖이면 무시. 가득 차면 더 센 AP만 가장 약한 AP를 대신한다 (새 AP는 기록이 비어 있음) */
    void upsert(const ApInfo& ap);
    /**
     * 스캔 한 번이 끝난 뒤 부른다 — 대상 AP마다 RSSI 기록을 하나 넣는다.
     * 이번 스캔에서 본 AP(lastSeenMs == nowMs)는 RSSI, 못 본 AP는 kNoSample.
     * onlyChannel이 0이 아니면 그 채널 AP만 기록한다 (채널 하나만 스캔한 경우).
     */
    void recordSample(uint32_t nowMs, uint8_t onlyChannel);
    /** now - lastSeenMs > ttlMs 인 AP를 지운다. onlyChannel이 0이 아니면 그 채널 AP만 검사한다 */
    void prune(uint32_t nowMs, uint32_t ttlMs, uint8_t onlyChannel = 0);
    void clear() { _count = 0; }
    int count() const { return _count; }
    const ApInfo& at(int i) const { return _e[i].info; }
    const ApHistory& history(int i) const { return _e[i].hist; }
    /** BSSID로 찾은 인덱스, 없으면 -1 */
    int find(const uint8_t* bssid) const;
    /** RSSI 내림차순 인덱스를 out에 채우고 개수를 돌려준다 */
    int sortedByRssi(uint8_t* out, int maxOut) const;

private:
    struct Entry {
        ApInfo info;
        ApHistory hist;
    };
    Entry _e[kApMax];
    int _count = 0;
};

/**
 * 채널 하나의 통계 — 그 채널에 공유기를 둔다고 할 때의 비용.
 *   같은 채널 AP 중 kCcaDbm 이상: 서로 신호를 알아듣고 번갈아 보낸다 → "시간 나눠 쓰기" 비용 (신호 세기 그대로)
 *   일부 겹치는 AP(거리 1~4)와 kCcaDbm 미만 같은 채널 AP: 알아듣지 못하는 잡음 → 겹침 비율 × kNoiseWeight 배
 * costDbm = 10·log10(두 비용 합, mW). AP가 전혀 안 겹치면 kNoSignalDbm.
 */
struct ChannelStat {
    uint8_t apCount;    ///< 이 채널이 중심인 AP 수
    uint8_t adjCount;   ///< 일부 겹치는(거리 1~4) AP 수
    float costDbm;
};

constexpr float kNoSignalDbm = -100.0f;
constexpr float kCcaDbm = -82.0f;          ///< 20MHz WiFi가 같은 채널 신호를 듣고 전송을 미루는 기준 (802.11 수신 감도)
constexpr float kNoiseWeight = 2.0f;       ///< 잡음 비용 가중 (+3dB) — 충돌로 패킷이 깨져 시간 나눠 쓰기보다 나쁘다
constexpr float kOffGridPenaltyDb = 3.0f;  ///< 1·6·11 밖 채널 벌점 — 양쪽 이웃 그룹 모두에 잡음을 주므로
constexpr float kBusyDbPer100Fps = 3.0f;   ///< 실측 활동량 벌점 — 제어·데이터 100프레임/s마다 +3dB (부하 측정: 평소 20~90, 스트리밍 중 329)

/**
 * 채널 c와 d 사이 겹침 가중치 — 22MHz 폭 신호가 채널 간격(5MHz) × 거리만큼 떨어졌을 때 겹치는 비율.
 * 거리 0/1/2/3/4 → 1.00/0.77/0.55/0.32/0.09, 5 이상 0 (1·6·11이 서로 안 겹치는 이유).
 */
float overlapWeight(int distance);

/** stats[kMinChannel..kMaxChannel]을 채운다 (배열 크기 kMaxChannel + 1, [0]은 쓰지 않음) */
void computeChannelStats(const ApTable& table, ChannelStat* stats);

/** 1·6·11 채널인지 (서로 안 겹치는 채널) */
inline bool isGridChannel(int c) { return c == 1 || c == 6 || c == 11; }

/**
 * 채널 c의 추천 점수(dB, 낮을수록 좋음) = costDbm + (1·6·11 밖이면 kOffGridPenaltyDb) + (실측했으면 활동량/100 × kBusyDbPer100Fps).
 * traffic은 [kMaxChannel + 1] 배열 또는 nullptr(실측 없음).
 */
float channelScore(const ChannelStat* stats, const ChannelTraffic* traffic, int c);

/** 채널 1~13 중 channelScore가 가장 낮은 채널 (같으면 앞 채널) */
int recommendChannel(const ChannelStat* stats, const ChannelTraffic* traffic = nullptr);

/** 채널 1~13 중 비용 + 활동량 벌점이 가장 높은(가장 혼잡한) 채널 (1·6·11 밖 벌점은 넣지 않음, 같으면 앞 채널) */
int busiestChannel(const ChannelStat* stats, const ChannelTraffic* traffic = nullptr);

/** 추적 부저 간격 — 셀수록 짧다. −90dBm 이하 1000ms, −30dBm 이상 100ms, 사이는 선형 */
uint32_t beepIntervalMs(int rssi);

/** RSSI(dBm)를 0~maxPx 높이로 — lo 이하 0, hi 이상 maxPx, 사이는 선형 */
int rssiToPx(float dbm, float lo, float hi, int maxPx);

/**
 * 직선 (x0,y0)-(x1,y1)을 x 범위 [left, right]로 자른다. 범위와 겹치지 않으면 false.
 * 잘린 끝점의 y는 원래 직선에서 반올림해 구하므로 이웃 화면의 조각과 이어진다.
 */
bool clipLineX(int x0, int y0, int x1, int y1, int left, int right, int& cx0, int& cy0, int& cx1, int& cy1);

}  // namespace wa

#endif
