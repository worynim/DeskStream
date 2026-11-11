// worynim@gmail.com
/**
 * @file test_analysis.cpp
 * @brief analysis · traffic · ble_table 호스트 단위 테스트
 * @details 실행 (저장소 루트에서):
 *   g++ -std=c++17 -IWireless_Analyzer Wireless_Analyzer/test/test_analysis.cpp Wireless_Analyzer/analysis.cpp \
 *       Wireless_Analyzer/traffic.cpp Wireless_Analyzer/ble_table.cpp -o /tmp/test_analysis && /tmp/test_analysis
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <initializer_list>

#include "analysis.h"
#include "ble_table.h"

static int fails = 0;
#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            fails++;                                               \
        }                                                          \
    } while (0)

using namespace wa;

static ApInfo makeAp(uint8_t id, uint8_t ch, int8_t rssi, uint32_t seen = 0) {
    ApInfo a = {};
    a.bssid[5] = id;
    snprintf(a.ssid, sizeof(a.ssid), "ap%u", id);
    a.channel = ch;
    a.rssi = rssi;
    a.lastSeenMs = seen;
    return a;
}

static void testUpsertPrune() {
    ApTable t;
    t.upsert(makeAp(1, 6, -50, 0));
    t.upsert(makeAp(2, 1, -70, 0));
    t.upsert(makeAp(1, 6, -40, 1000));   // 같은 BSSID → 갱신
    t.upsert(makeAp(3, 14, -30, 0));     // 채널 범위 밖 → 무시
    t.upsert(makeAp(4, 0, -30, 0));
    CHECK(t.count() == 2);
    CHECK(t.at(0).rssi == -40 && t.at(0).lastSeenMs == 1000);
    t.prune(61000, 60000);               // ap2: 61000 > 60000 → 지움, ap1: 60000 → 유지
    CHECK(t.count() == 1);
    CHECK(t.at(0).bssid[5] == 1);
}

static void testFullTable() {
    ApTable t;
    for (int i = 0; i < kApMax; i++) t.upsert(makeAp((uint8_t)i, 1, (int8_t)(-50 - i)));   // 가장 약한 것 = id 47 (-97)
    CHECK(t.count() == kApMax);
    t.upsert(makeAp(200, 1, -99));   // 가장 약한 것보다 약함 → 무시
    bool found = false;
    for (int i = 0; i < t.count(); i++) found |= t.at(i).bssid[5] == 200;
    CHECK(!found);
    t.upsert(makeAp(201, 1, -60));   // 가장 약한 것을 대신
    found = false;
    bool weakestGone = true;
    for (int i = 0; i < t.count(); i++) {
        found |= t.at(i).bssid[5] == 201;
        if (t.at(i).bssid[5] == 47) weakestGone = false;
    }
    CHECK(found && weakestGone && t.count() == kApMax);
}

static void testSorted() {
    ApTable t;
    t.upsert(makeAp(1, 1, -70));
    t.upsert(makeAp(2, 1, -30));
    t.upsert(makeAp(3, 1, -50));
    t.upsert(makeAp(4, 1, -50));
    uint8_t idx[kApMax];
    CHECK(t.sortedByRssi(idx, kApMax) == 4);
    CHECK(t.at(idx[0]).bssid[5] == 2);
    CHECK(t.at(idx[1]).bssid[5] == 3 && t.at(idx[2]).bssid[5] == 4);   // 같은 RSSI는 들어온 순서
    CHECK(t.at(idx[3]).bssid[5] == 1);
    CHECK(t.sortedByRssi(idx, 2) == 2 && t.at(idx[0]).bssid[5] == 2 && t.at(idx[1]).bssid[5] == 3);
}

static void testRing() {
    RssiRing<3> r;
    CHECK(r.size() == 0);
    r.push(-50);
    r.push(-60);
    CHECK(r.size() == 2 && r.at(0) == -50 && r.at(1) == -60);
    r.push(-70);
    r.push(-80);   // 가득 차면 가장 오래된 것(-50)을 덮어씀
    CHECK(r.size() == 3 && r.at(0) == -60 && r.at(2) == -80);
    r.push(kNoSample);
    RingStats st = ringStats(r, 3);   // -70, -80, 없음
    CHECK(st.count == 2 && st.minDbm == -80 && st.maxDbm == -70 && st.avgDbm == -75.0f);
    st = ringStats(r, 1);
    CHECK(st.count == 0);
    r.clear();
    CHECK(r.size() == 0 && ringStats(r, 3).count == 0);
}

static void testHistory() {
    ApTable t;
    t.upsert(makeAp(1, 6, -50, 1000));
    t.upsert(makeAp(2, 1, -70, 1000));
    t.recordSample(1000, 0);
    t.upsert(makeAp(1, 6, -45, 2000));   // ap2는 이번 스캔에서 못 봄
    t.recordSample(2000, 0);
    const int i1 = t.find(makeAp(1, 0, 0).bssid), i2 = t.find(makeAp(2, 0, 0).bssid);
    CHECK(i1 >= 0 && i2 >= 0 && t.find(makeAp(9, 0, 0).bssid) == -1);
    CHECK(t.history(i1).size() == 2 && t.history(i1).at(0) == -50 && t.history(i1).at(1) == -45);
    CHECK(t.history(i2).size() == 2 && t.history(i2).at(1) == kNoSample);

    t.upsert(makeAp(1, 6, -40, 3000));   // 채널 6만 스캔 → ap2(채널 1)는 기록·정리 대상 아님
    t.recordSample(3000, 6);
    CHECK(t.history(i1).size() == 3 && t.history(i2).size() == 2);
    t.prune(62000, 60000, 6);            // ap2는 61초 지났지만 채널 1이라 유지
    CHECK(t.count() == 2);
    t.prune(62000, 60000);
    CHECK(t.count() == 1 && t.at(0).bssid[5] == 1 && t.history(0).size() == 3);   // 지운 뒤에도 기록이 따라감
}

static void testSparkRange() {
    float lo = -95, hi = -30;
    sparkRange(RingStats{0, 0, 0, 0}, 10, lo, hi);   // 기록 없음 → 그대로
    CHECK(lo == -95 && hi == -30);
    sparkRange(RingStats{5, -80, -50, -60}, 10, lo, hi);
    CHECK(lo == -80 && hi == -50);
    sparkRange(RingStats{5, -64, -60, -62}, 10, lo, hi);   // 폭 4dB → 가운데 -62 기준 10dB
    CHECK(lo == -67 && hi == -57);
}

static void testBeepInterval() {
    CHECK(beepIntervalMs(-95) == 1000 && beepIntervalMs(-90) == 1000);
    CHECK(beepIntervalMs(-30) == 100 && beepIntervalMs(-20) == 100);
    CHECK(beepIntervalMs(-60) == 550);
    CHECK(beepIntervalMs(-50) < beepIntervalMs(-70));
}

static void testOverlap() {
    CHECK(overlapWeight(0) == 1.0f);
    CHECK(fabsf(overlapWeight(1) - 17.0f / 22) < 1e-6f);
    CHECK(overlapWeight(-2) == overlapWeight(2));
    CHECK(overlapWeight(4) > 0);
    CHECK(overlapWeight(5) == 0 && overlapWeight(12) == 0);
}

static float dbmSum(std::initializer_list<float> mws) {
    float s = 0;
    for (float m : mws) s += m;
    return 10 * log10f(s);
}
static float mw(float dbm) { return powf(10, dbm / 10); }

static void testChannelStats() {
    ApTable t;
    ChannelStat st[kMaxChannel + 1];
    computeChannelStats(t, st);
    for (int c = kMinChannel; c <= kMaxChannel; c++)
        CHECK(st[c].apCount == 0 && st[c].adjCount == 0 && st[c].costDbm == kNoSignalDbm);
    CHECK(recommendChannel(st) == 1);   // 모두 비면 1·6·11 중 앞 채널 (밖 채널은 벌점)

    t.upsert(makeAp(1, 6, -40));
    t.upsert(makeAp(2, 6, -40));
    computeChannelStats(t, st);
    CHECK(st[6].apCount == 2 && st[6].adjCount == 0);
    CHECK(st[5].apCount == 0 && st[5].adjCount == 2);
    CHECK(fabsf(st[6].costDbm - dbmSum({mw(-40), mw(-40)})) < 0.01f);   // 같은 채널: 시간 나눠 쓰기 = 신호 그대로
    CHECK(fabsf(st[5].costDbm - dbmSum({2 * kNoiseWeight * overlapWeight(1) * mw(-40)})) < 0.01f);   // 잡음 가중
    CHECK(st[5].costDbm > st[6].costDbm);   // 일부 겹침이 같은 채널보다 나쁘다
    CHECK(st[5].costDbm == st[7].costDbm);
    CHECK(st[1].costDbm == kNoSignalDbm && st[11].costDbm == kNoSignalDbm);   // 5채널 이상 떨어지면 안 겹침
    CHECK(recommendChannel(st) == 1);   // 1·11 동점 → 앞 채널

    t.upsert(makeAp(3, 1, -60));
    computeChannelStats(t, st);
    CHECK(recommendChannel(st) == 11);
}

static void testWeakCoChannel() {
    ApTable t;
    ChannelStat st[kMaxChannel + 1];
    t.upsert(makeAp(1, 6, -85));   // CCA 기준 미만 → 같은 채널이라도 잡음으로 계산
    computeChannelStats(t, st);
    CHECK(st[6].apCount == 1);
    CHECK(fabsf(st[6].costDbm - dbmSum({kNoiseWeight * mw(-85)})) < 0.01f);
}

static void testRecommendGrid() {
    // AP가 모두 1·6·11에 있으면, 사이 채널은 양쪽 잡음을 받아 1·6·11보다 나쁘다 → 1·6·11 중에서 추천
    ApTable t;
    ChannelStat st[kMaxChannel + 1];
    t.upsert(makeAp(1, 1, -50));
    t.upsert(makeAp(2, 6, -50));
    t.upsert(makeAp(3, 6, -50));
    t.upsert(makeAp(4, 11, -50));
    t.upsert(makeAp(5, 11, -50));
    computeChannelStats(t, st);
    CHECK(st[3].costDbm > st[1].costDbm);
    CHECK(recommendChannel(st) == 1);
}

static void testRecommendOffGrid() {
    // AP가 2·6·6·10에 흩어져 있으면 13은 10에서 3칸 떨어진 약한 잡음만 받는다 → 벌점을 더해도 13이 가장 낫다
    ApTable t;
    ChannelStat st[kMaxChannel + 1];
    t.upsert(makeAp(1, 2, -40));
    t.upsert(makeAp(2, 6, -40));
    t.upsert(makeAp(3, 6, -40));
    t.upsert(makeAp(4, 10, -40));
    computeChannelStats(t, st);
    CHECK(st[13].costDbm + kOffGridPenaltyDb < st[11].costDbm);
    CHECK(st[13].costDbm + kOffGridPenaltyDb < st[6].costDbm);
    CHECK(recommendChannel(st) == 13);
}

static void testTrafficUpdate() {
    ChannelTraffic t = {};
    trafficUpdate(t, 20, 150000, 0.5f);   // 처음: 그대로
    CHECK(t.sweeps == 1 && fabsf(t.fps - 133.333f) < 0.01f);
    trafficUpdate(t, 0, 150000, 0.5f);    // 이후: 반씩 섞음
    CHECK(t.sweeps == 2 && fabsf(t.fps - 66.667f) < 0.01f);
    trafficUpdate(t, 1, 0, 0.5f);         // 머문 시간 0 → 무시
    CHECK(t.sweeps == 2);
}

static void testRecommendBusy() {
    // 1·11이 AP 기준으로 같아도 실측 활동량이 많은 채널은 피한다
    ApTable t;
    ChannelStat st[kMaxChannel + 1];
    ChannelTraffic tr[kMaxChannel + 1] = {};
    t.upsert(makeAp(1, 6, -40));
    computeChannelStats(t, st);
    CHECK(recommendChannel(st, tr) == 1);   // 실측 없음(sweeps 0) → 무시
    tr[1] = {300, 1};
    CHECK(recommendChannel(st, tr) == 11);
    CHECK(recommendChannel(st, nullptr) == 1);
    CHECK(busiestChannel(st, nullptr) == 5);   // 옆 채널(5·7)이 잡음 2배라 AP가 있는 6보다 혼잡, 동점이면 앞 채널
    tr[1] = {3000, 1};                         // 활동량 3000/s = +90dB → 가장 혼잡
    CHECK(busiestChannel(st, tr) == 1);
    CHECK(fabsf(channelScore(st, tr, 1) - (kNoSignalDbm + 90)) < 0.01f);
    CHECK(fabsf(channelScore(st, nullptr, 2) - (st[2].costDbm + kOffGridPenaltyDb)) < 0.01f);
}

static BleDevice makeBle(uint8_t id, int8_t rssi, const char* name = "", uint32_t seen = 0) {
    BleDevice d = {};
    d.addr[5] = id;
    d.rssi = rssi;
    d.mfr = kNoMfr;
    copyUtf8(d.name, name, kBleNameLen);
    d.lastSeenMs = seen;
    return d;
}

static void testBleTable() {
    BleTable t;
    t.upsert(makeBle(1, -50, "Phone", 0));
    t.upsert(makeBle(2, -85, "", 0));
    t.upsert(makeBle(1, -55, "", 1000));   // 같은 주소 → 갱신, 이름 없으면 전 이름 유지
    CHECK(t.count() == 2);
    CHECK(t.at(0).rssi == -55 && strcmp(t.at(0).name, "Phone") == 0 && t.at(0).lastSeenMs == 1000);
    t.upsert(makeBle(1, -55, "Watch", 1000));   // 새 이름이 오면 바꿈
    CHECK(strcmp(t.at(0).name, "Watch") == 0);
    t.upsert(makeBle(3, -70));
    BleZones z = t.zones();   // -55 가까움, -70 중간, -85 멂
    CHECK(z.nearCount == 1 && z.midCount == 1 && z.farCount == 1);
    uint8_t idx[kBleMax];
    CHECK(t.sortedByRssi(idx, kBleMax) == 3 && t.at(idx[0]).addr[5] == 1 && t.at(idx[2]).addr[5] == 2);
    t.prune(31000, 30000);   // 1000에 본 기기만 남음
    CHECK(t.count() == 1 && t.at(0).addr[5] == 1);

    BleTable full;
    for (int i = 0; i < kBleMax; i++) full.upsert(makeBle((uint8_t)i, (int8_t)(-40 - i / 2)));
    full.upsert(makeBle(200, -99));   // 가장 약한 것보다 약함 → 무시
    full.upsert(makeBle(201, -45));   // 가장 약한 것을 대신
    bool has200 = false, has201 = false;
    for (int i = 0; i < full.count(); i++) {
        has200 |= full.at(i).addr[5] == 200;
        has201 |= full.at(i).addr[5] == 201;
    }
    CHECK(full.count() == kBleMax && !has200 && has201);
}

static void testCopyUtf8() {
    char buf[8];
    copyUtf8(buf, "abc", sizeof(buf));
    CHECK(strcmp(buf, "abc") == 0);
    copyUtf8(buf, "갤럭시버즈", sizeof(buf));   // 한글 3바이트 × 5 → 7바이트 칸에 2글자(6바이트)
    CHECK(strcmp(buf, "갤럭") == 0);
    copyUtf8(buf, "abcdefghij", sizeof(buf));
    CHECK(strcmp(buf, "abcdefg") == 0);
    CHECK(strcmp(bleMfrName(0x004C), "Apple") == 0 && bleMfrName(0x1234) == nullptr);
}

static void testRssiToPx() {
    CHECK(rssiToPx(-100, -95, -30, 50) == 0);
    CHECK(rssiToPx(-20, -95, -30, 50) == 50);
    CHECK(rssiToPx(-62.5f, -95, -30, 50) == 25);
}

static void testClipLine() {
    int a, b, c, d;
    // P0 경계 테스트의 사선 (0,63)-(511,52): 화면마다 잘린 조각이 경계에서 이어져야 한다
    int prevY = -1;
    for (int s = 0; s < 4; s++) {
        CHECK(clipLineX(0, 63, 511, 52, s * 128, s * 128 + 127, a, b, c, d));
        CHECK(a == s * 128 && c == s * 128 + 127);
        if (s > 0) CHECK(b - prevY >= -1 && b - prevY <= 0);   // 이웃 조각과 1px 이내
        prevY = d;
    }
    CHECK(clipLineX(511, 52, 0, 63, 0, 127, a, b, c, d) && a == 0 && b == 63);   // 끝점 순서가 바뀌어도 같음
    CHECK(!clipLineX(0, 0, 100, 10, 128, 255, a, b, c, d));                      // 범위 밖
    CHECK(clipLineX(130, 0, 130, 20, 128, 255, a, b, c, d) && a == 130 && c == 130 && b == 0 && d == 20);   // 세로선
}

int main() {
    testUpsertPrune();
    testFullTable();
    testSorted();
    testRing();
    testHistory();
    testSparkRange();
    testBeepInterval();
    testOverlap();
    testChannelStats();
    testWeakCoChannel();
    testRecommendGrid();
    testRecommendOffGrid();
    testTrafficUpdate();
    testRecommendBusy();
    testBleTable();
    testCopyUtf8();
    testRssiToPx();
    testClipLine();
    if (fails) {
        printf("%d FAILED\n", fails);
        return 1;
    }
    printf("all passed\n");
    return 0;
}
