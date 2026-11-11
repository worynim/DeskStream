// worynim@gmail.com
/**
 * @file analysis.cpp
 * @brief analysis.h 구현
 */
#include "analysis.h"

#include <math.h>
#include <string.h>

namespace wa {

void ApTable::upsert(const ApInfo& ap) {
    if (ap.channel < kMinChannel || ap.channel > kMaxChannel) return;
    const int found = find(ap.bssid);
    if (found >= 0) {
        _e[found].info = ap;
        return;
    }
    int slot = _count;
    if (_count == kApMax) {
        slot = 0;
        for (int i = 1; i < _count; i++)
            if (_e[i].info.rssi < _e[slot].info.rssi) slot = i;
        if (ap.rssi <= _e[slot].info.rssi) return;
    } else {
        _count++;
    }
    _e[slot].info = ap;
    _e[slot].hist.clear();
}

void ApTable::recordSample(uint32_t nowMs, uint8_t onlyChannel) {
    for (int i = 0; i < _count; i++) {
        const ApInfo& ap = _e[i].info;
        if (onlyChannel && ap.channel != onlyChannel) continue;
        _e[i].hist.push(ap.lastSeenMs == nowMs ? ap.rssi : kNoSample);
    }
}

void ApTable::prune(uint32_t nowMs, uint32_t ttlMs, uint8_t onlyChannel) {
    int w = 0;
    for (int i = 0; i < _count; i++) {
        const ApInfo& ap = _e[i].info;
        const bool expired = (!onlyChannel || ap.channel == onlyChannel) && nowMs - ap.lastSeenMs > ttlMs;
        if (!expired) {
            if (w != i) _e[w] = _e[i];
            w++;
        }
    }
    _count = w;
}

int ApTable::find(const uint8_t* bssid) const {
    for (int i = 0; i < _count; i++)
        if (memcmp(_e[i].info.bssid, bssid, 6) == 0) return i;
    return -1;
}

int ApTable::sortedByRssi(uint8_t* out, int maxOut) const {
    uint8_t idx[kApMax];
    for (int i = 0; i < _count; i++) {   // 삽입 정렬 (최대 48개), 같은 RSSI는 먼저 들어온 순서 유지
        int pos = i;
        while (pos > 0 && _e[idx[pos - 1]].info.rssi < _e[i].info.rssi) {
            idx[pos] = idx[pos - 1];
            pos--;
        }
        idx[pos] = (uint8_t)i;
    }
    const int n = _count < maxOut ? _count : maxOut;
    memcpy(out, idx, n);
    return n;
}

float overlapWeight(int distance) {
    if (distance < 0) distance = -distance;
    const float w = (22.0f - 5.0f * distance) / 22.0f;
    return w > 0 ? w : 0;
}

void computeChannelStats(const ApTable& table, ChannelStat* stats) {
    float share[kMaxChannel + 1] = {}, noise[kMaxChannel + 1] = {};
    for (int c = kMinChannel; c <= kMaxChannel; c++) stats[c].apCount = stats[c].adjCount = 0;
    for (int i = 0; i < table.count(); i++) {
        const ApInfo& ap = table.at(i);
        const float p = powf(10.0f, ap.rssi / 10.0f);
        for (int c = kMinChannel; c <= kMaxChannel; c++) {
            const int d = c > ap.channel ? c - ap.channel : ap.channel - c;
            const float w = overlapWeight(d);
            if (w <= 0) continue;
            if (d == 0) stats[c].apCount++;
            else stats[c].adjCount++;
            if (d == 0 && ap.rssi >= kCcaDbm) share[c] += p;
            else noise[c] += p * w;
        }
    }
    for (int c = kMinChannel; c <= kMaxChannel; c++) {
        const float mw = share[c] + kNoiseWeight * noise[c];
        const float dbm = mw > 0 ? 10.0f * log10f(mw) : kNoSignalDbm;
        stats[c].costDbm = dbm > kNoSignalDbm ? dbm : kNoSignalDbm;
    }
}

/** 활동량 벌점 (실측 없으면 0) */
static float busyDb(const ChannelTraffic* traffic, int c) {
    return traffic && traffic[c].sweeps ? traffic[c].fps / 100 * kBusyDbPer100Fps : 0;
}

float channelScore(const ChannelStat* stats, const ChannelTraffic* traffic, int c) {
    return stats[c].costDbm + (isGridChannel(c) ? 0 : kOffGridPenaltyDb) + busyDb(traffic, c);
}

int busiestChannel(const ChannelStat* stats, const ChannelTraffic* traffic) {
    int worst = kMinChannel;
    float worstScore = -1e9f;
    for (int c = kMinChannel; c <= kMaxChannel; c++) {
        const float score = stats[c].costDbm + busyDb(traffic, c);
        if (score > worstScore) {
            worstScore = score;
            worst = c;
        }
    }
    return worst;
}

int recommendChannel(const ChannelStat* stats, const ChannelTraffic* traffic) {
    int best = kMinChannel;
    float bestScore = 1e9f;
    for (int c = kMinChannel; c <= kMaxChannel; c++) {
        const float score = channelScore(stats, traffic, c);
        if (score < bestScore) {
            bestScore = score;
            best = c;
        }
    }
    return best;
}

void sparkRange(const RingStats& st, float minSpan, float& lo, float& hi) {
    if (st.count == 0) return;
    lo = st.minDbm;
    hi = st.maxDbm;
    if (hi - lo < minSpan) {
        const float mid = (lo + hi) / 2;
        lo = mid - minSpan / 2;
        hi = mid + minSpan / 2;
    }
}

uint32_t beepIntervalMs(int rssi) {
    if (rssi <= -90) return 1000;
    if (rssi >= -30) return 100;
    return (uint32_t)(1000 - (rssi + 90) * 900 / 60);
}

int rssiToPx(float dbm, float lo, float hi, int maxPx) {
    if (dbm <= lo) return 0;
    if (dbm >= hi) return maxPx;
    return (int)((dbm - lo) * maxPx / (hi - lo) + 0.5f);
}

/** (x0,y0)-(x1,y1) 직선 위 x의 y — 반올림 (x0 < x1) */
static int lineYAt(int x0, int y0, int x1, int y1, int x) {
    const long num = (long)(y1 - y0) * (x - x0) * 2 / (x1 - x0);
    return y0 + (int)((num + (num >= 0 ? 1 : -1)) / 2);
}

bool clipLineX(int x0, int y0, int x1, int y1, int left, int right, int& cx0, int& cy0, int& cx1, int& cy1) {
    if (x0 > x1) {
        int t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
    }
    if (x1 < left || x0 > right) return false;
    if (x0 == x1) {   // 세로선
        cx0 = cx1 = x0; cy0 = y0; cy1 = y1;
        return true;
    }
    cx0 = x0 < left ? left : x0;
    cx1 = x1 > right ? right : x1;
    cy0 = lineYAt(x0, y0, x1, y1, cx0);
    cy1 = lineYAt(x0, y0, x1, y1, cx1);
    return true;
}

}  // namespace wa
