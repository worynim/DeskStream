// worynim@gmail.com
/**
 * @file ui.cpp
 * @brief ui.h 구현
 * @details 채널 화면(스펙트럼·혼잡도) 배치 — 채널 c의 중심 x = 32 × c + 48 (채널 1 = 80, 13 = 464).
 *          채널 중심은 화면 경계(128·256·384)에서 16px 이상 떨어져 채널 번호가 두 화면에 쪼개지지 않는다.
 *   y 0~53  : 그래프 영역 (y 53 = 바닥)
 *   y 54    : 축선
 *   y 56~63 : 채널 번호
 * @details AP 목록 — 한 줄 10px × 6줄. 화면 1: 커서·SSID, 화면 2: 채널·RSSI, x 208~503: 최근 60회 RSSI 스파크라인
 *          (줄마다 그 AP의 최소~최대로 확대, 최소 10dB 폭).
 * @details AP 추적 — 화면 1: SSID·채널·현재 RSSI(굵게)·막대·최고/평균·부저, x 132~511: 최근 190회 RSSI 그래프.
 * @details 모드 이름 알림 — 모드를 바꾼 직후 화면 2(x 128~255) 가운데에 테두리 상자로 "2/4 채널 혼잡도"를 잠깐 띄운다.
 *          512px 가운데(x 256)는 화면 경계라 글자가 쪼개지므로 화면 하나 안에 둔다 ("스캔 중" 안내도 같은 이유로 화면 2 가운데).
 * @details BLE 기기 — 화면 1: 기기 수(굵게)·거리 구간별 수, 화면 2: 기기 수 추이(최근 60회),
 *          화면 3·4: 센 순서 6줄 — 이름(화면 3) · 제조사(화면 4) · RSSI. 주소는 표시하지 않는다.
 * @details 요약 — 화면마다 타일 하나: WiFi AP 수 · 추천/가장 혼잡한 채널 · 가장 센 AP · BLE 기기 수.
 * @details 뒤집기 — 패널은 DisplayCore::setFlip(true)로 180° 돌리고, 논리 화면 s를 물리 화면 3−s에 그린다.
 * @details 글꼴은 Agent_Monitor와 같은 갈무리7 하나(영문·한글).
 */
// 갈무리7 한글 글꼴은 64KB 를 넘어 U8g2 가 큰 글꼴 지원을 켜야 한다 — U8g2lib.h 보다 먼저 정의
#ifndef U8G2_USE_LARGE_FONTS
#define U8G2_USE_LARGE_FONTS
#endif
#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "canvas512.h"
#include "config.h"
#include "u8g2_font_galmuri7_kr.h"

namespace wa {

namespace {

constexpr int kChPx = 32;         // 채널 간격(px)
constexpr int kBaseY = 53;        // 그래프 바닥
constexpr int kAxisY = 54;        // 축선
constexpr int kLabelY = 63;       // 채널 번호 기준선
constexpr int kHillMaxPx = 44;    // 스펙트럼 언덕 최대 높이
constexpr int kHillBaseHalf = 60; // 언덕 바닥 반폭 — 20MHz 신호가 ±2채널에 걸침
constexpr int kHillTopHalf = 28;  // 언덕 꼭대기 반폭
constexpr int kBarMaxPx = 36;     // 혼잡도 막대 최대 높이
constexpr int kCostBarX = -10, kCostBarW = 13;   // 비용 막대: 채널 중심 기준 x, 폭 (채워진 막대)
constexpr int kActBarX = 5, kActBarW = 6;        // 실측 활동량 막대: 채널 중심 기준 x, 폭 (속이 빈 막대)
constexpr float kActFullFps = 400.0f;             // 활동량 막대 꼭대기 (제어·데이터 프레임/s, 부하 측정 최고 329)
constexpr int kSsidLabels = 4;    // 스펙트럼에 SSID를 붙일 센 AP 수
constexpr int kSsidMaxPx = 84;    // SSID 라벨 최대 폭

// AP 목록
constexpr int kListRows = 6;
constexpr int kListRowH = 10;
constexpr int kListSsidX = 8, kListSsidMaxPx = 112;   // 화면 1 안에
constexpr int kListChX = 132, kListRssiRight = 196;    // 화면 2 안에
constexpr int kListSparkX = 208, kListSparkStep = 5;   // 60회 × 5px = 300px
constexpr int kListSparkH = 7;
constexpr float kListSparkMinSpan = 10.0f;   // 스파크라인은 줄마다 최소~최대에 맞춰 확대 (최소 10dB 폭)

// AP 추적
constexpr int kTrackGraphX = 132, kTrackGraphStep = 2;   // 190회 × 2px = 380px → x 132~511
constexpr int kTrackGraphTop = 2, kTrackGraphBottom = 61;
constexpr int kTrackStatsN = 60;   // 최고·평균에 쓰는 최근 기록 수
constexpr int kBoldGap = 2;        // 굵은 글씨 글자 간격 추가(px) — Agent_Monitor와 같음

// 모드 이름 알림
constexpr int kToastX = 132, kToastY = 22, kToastW = 120, kToastH = 20;   // 화면 2 안
const char* const kModeNames[(int)Mode::Count] = {"채널 스펙트럼", "채널 혼잡도", "AP 목록", "AP 추적", "BLE 기기", "요약"};

// 요약 타일 (화면마다 하나)
constexpr int kTilePad = 2, kTileTitleY = 9, kTileRuleY = 12, kTileValueY = 31, kTileSub1Y = 47, kTileSub2Y = 60;

// BLE 기기
constexpr int kBleGraphX = 132, kBleGraphStep = 2;          // 60회 × 2px → x 132~250 (화면 2 안)
constexpr int kBleGraphTop = 12, kBleGraphBottom = 61;
constexpr int kBleNameX = 260, kBleNameMaxPx = 120;         // 화면 3 안
constexpr int kBleMfrX = 388, kBleMfrMaxPx = 90;            // 화면 4 안

int chX(int ch) { return kChPx * ch + 48; }

/** 굵은 글씨 — 글자마다 1px 어긋나게 두 번 그리고 글자 간격을 kBoldGap 만큼 넓힌다 (Agent_Monitor drawBold 방식) */
void textBold(Canvas512& c, int x, int y, const char* text) {
    const size_t n = strlen(text);
    for (size_t i = 0; i < n;) {
        const uint8_t len = ds::utf8CharLen(text + i, (int)(n - i));
        char ch[5] = {};
        memcpy(ch, text + i, len);
        c.text(x, y, ch);
        c.text(x + 1, y, ch);
        x += c.textWidth(ch) + kBoldGap;
        i += len;
    }
}

/**
 * RSSI 기록 그래프 — 가장 최근 기록이 xRight, 한 칸 step px씩 왼쪽으로. yBottom에서 위로 heightPx 높이(lo~hi dBm).
 * kNoSample은 선을 끊고, 앞뒤가 모두 끊긴 기록은 점으로 찍는다.
 */
template <int N>
void drawRssiGraph(Canvas512& c, const RssiRing<N>& r, int xRight, int step, int yBottom, int heightPx, float lo,
                   float hi) {
    int px = 0, py = 0;
    bool havePrev = false;
    for (int k = 0; k < r.size(); k++) {
        const int8_t v = r.at(k);
        const int x = xRight - (r.size() - 1 - k) * step;
        if (v == kNoSample) {
            havePrev = false;
            continue;
        }
        const int y = yBottom - rssiToPx(v, lo, hi, heightPx);
        const bool nextValid = k + 1 < r.size() && r.at(k + 1) != kNoSample;
        if (havePrev) c.line(px, py, x, y);
        else if (!nextValid) c.pixel(x, y);
        px = x; py = y; havePrev = true;
    }
}

/** 목록·추적에서 SSID 표시 문자열 */
const char* ssidOrHidden(const ApInfo& ap) { return ap.ssid[0] ? ap.ssid : "(숨김)"; }

/** 최근 기록이 유효하면 그 RSSI, 아니면 kNoSample */
template <int N>
int8_t lastSample(const RssiRing<N>& r) { return r.size() ? r.at(r.size() - 1) : kNoSample; }

/** src를 maxW 폭에 맞게 글자 단위로 자른다 (UTF-8 글자 중간에서 자르지 않음) */
void fitText(Canvas512& c, const char* src, int maxW, char* out, size_t outSize) {
    strlcpy(out, src, outSize);
    size_t n = strlen(out);
    while (n > 0 && c.textWidth(out) > maxW) {
        do { n--; } while (n > 0 && (out[n] & 0xC0) == 0x80);   // 이어지는 바이트를 건너 글자 시작까지
        out[n] = '\0';
    }
}

/** 가운데 정렬 글자 */
void textCenter(Canvas512& c, int cx, int y, const char* s) { c.text(cx - c.textWidth(s) / 2, y, s); }

/** 채널 축선과 번호. highlight 채널은 반전 */
void drawChannelAxis(Canvas512& c, int highlight) {
    c.hline(0, kAxisY, 512);
    char buf[4];
    for (int ch = kMinChannel; ch <= kMaxChannel; ch++) {
        snprintf(buf, sizeof(buf), "%d", ch);
        if (ch == highlight) {
            c.box(chX(ch) - 8, kAxisY + 1, 16, 64 - kAxisY - 1);
            c.u8g2().setDrawColor(0);
            textCenter(c, chX(ch), kLabelY, buf);
            c.u8g2().setDrawColor(1);
        } else {
            textCenter(c, chX(ch), kLabelY, buf);
        }
    }
}

/** 오른쪽 위 일시정지 표시 */
void drawPaused(Canvas512& c) {
    const char* s = "일시정지";
    const int w = c.textWidth(s) + 4;
    c.box(512 - w, 0, w, 10);
    c.u8g2().setDrawColor(0);
    c.text(512 - w + 2, 8, s);
    c.u8g2().setDrawColor(1);
}

void drawWaiting(Canvas512& c, const char* what = "WiFi 스캔 중...") { textCenter(c, 192, 30, what); }   // 화면 2 가운데

/** 모드 1: AP마다 ±2채널 폭 언덕을 RSSI 높이로 겹쳐 그리고, 센 AP에 SSID를 붙인다 */
void drawSpectrum(Canvas512& c, const ApTable& t, const UiState& st) {
    drawChannelAxis(c, 0);
    char buf[48];
    snprintf(buf, sizeof(buf), "AP %d", t.count());
    c.text(0, 8, buf);
    if (st.scans == 0) return drawWaiting(c);

    uint8_t idx[kApMax];
    const int n = t.sortedByRssi(idx, kApMax);
    for (int k = n - 1; k >= 0; k--) {   // 약한 것부터 — 센 언덕이 위에 보이게
        const ApInfo& ap = t.at(idx[k]);
        const int x = chX(ap.channel);
        const int top = kBaseY - rssiToPx(ap.rssi, WA_RSSI_LO, WA_RSSI_HI, kHillMaxPx);
        c.line(x - kHillBaseHalf, kBaseY, x - kHillTopHalf, top);
        c.line(x - kHillTopHalf, top, x + kHillTopHalf, top);
        c.line(x + kHillTopHalf, top, x + kHillBaseHalf, kBaseY);
    }

    // SSID 라벨: 센 순서로, 이미 놓은 라벨과 겹치면 건너뛴다
    int placed = 0, lx[kSsidLabels], ly[kSsidLabels], lw[kSsidLabels];
    for (int k = 0; k < n && placed < kSsidLabels; k++) {
        const ApInfo& ap = t.at(idx[k]);
        fitText(c, ssidOrHidden(ap), kSsidMaxPx, buf, sizeof(buf));
        const int w = c.textWidth(buf);
        const int top = kBaseY - rssiToPx(ap.rssi, WA_RSSI_LO, WA_RSSI_HI, kHillMaxPx);
        const int y = top - 2 < 8 ? 8 : top - 2;
        int x = chX(ap.channel) - w / 2;
        x = x < 0 ? 0 : (x + w > 512 ? 512 - w : x);
        bool overlap = false;
        for (int j = 0; j < placed; j++)
            overlap |= x < lx[j] + lw[j] + 4 && lx[j] < x + w + 4 && y > ly[j] - 10 && ly[j] > y - 10;
        if (overlap) continue;
        c.text(x, y, buf);
        lx[placed] = x; ly[placed] = y; lw[placed] = w;
        placed++;
    }
}

/**
 * 모드 2: 채널마다 왼쪽 = 비용 막대(그 채널에 둘 때의 혼잡, analysis ChannelStat), 오른쪽 = 실측 활동량 막대(속 빈, 0~400프레임/s),
 * 막대 위에 같은 채널 AP 수(0 포함). 추천 채널은 채널 번호를 반전하고, 왼쪽 위에 이유를 쓴다.
 */
void drawChannels(Canvas512& c, const ChannelStat* stats, const UiState& st) {
    const ChannelTraffic* tr = st.traffic;
    const int rec = st.scans ? recommendChannel(stats, tr) : 0;
    drawChannelAxis(c, rec);
    char buf[64];
    if (st.scans == 0) return drawWaiting(c);
    if (tr[rec].sweeps)
        snprintf(buf, sizeof(buf), "추천 ch%d (같은 채널 %u / 겹침 %u / 활동 %d/s)", rec, stats[rec].apCount,
                 stats[rec].adjCount, (int)(tr[rec].fps + 0.5f));
    else
        snprintf(buf, sizeof(buf), "추천 ch%d (같은 채널 %u / 겹침 %u / 활동 측정 중)", rec, stats[rec].apCount,
                 stats[rec].adjCount);
    c.text(0, 8, buf);
    const char* legend = "왼쪽 비용 / 오른쪽 활동";
    c.text(512 - c.textWidth(legend), 8, legend);

    for (int ch = kMinChannel; ch <= kMaxChannel; ch++) {
        const ChannelStat& s = stats[ch];
        int top = kBaseY + 1;
        if (s.costDbm > kNoSignalDbm) {
            int h = rssiToPx(s.costDbm, WA_RSSI_LO, WA_RSSI_HI, kBarMaxPx);
            if (h < 1) h = 1;   // 신호가 있으면 최소 1px
            c.box(chX(ch) + kCostBarX, kBaseY - h + 1, kCostBarW, h);
            top = kBaseY - h + 1;
        }
        if (tr[ch].sweeps) {
            const float r = tr[ch].fps < kActFullFps ? tr[ch].fps / kActFullFps : 1.0f;
            const int h = (int)(r * kBarMaxPx + 0.5f);
            if (h >= 2) c.frame(chX(ch) + kActBarX, kBaseY - h + 1, kActBarW, h);
            else c.hline(chX(ch) + kActBarX, kBaseY, kActBarW);   // 0~1px는 바닥선으로
            if (kBaseY - h + 1 < top) top = kBaseY - h + 1;
        }
        snprintf(buf, sizeof(buf), "%u", s.apCount);   // 0도 쓴다 — 옆 채널 겹침만으로 생긴 막대임을 보이게
        textCenter(c, chX(ch), top - 2, buf);
    }
}

/** 모드 3: RSSI 순 AP 목록 — 선택 AP에 커서, 줄마다 최근 60회 RSSI 스파크라인 */
void drawList(Canvas512& c, const ApTable& t, const UiState& st) {
    if (st.scans == 0) return drawWaiting(c);
    uint8_t idx[kApMax];
    const int n = t.sortedByRssi(idx, kApMax);
    int selPos = 0;
    for (int k = 0; k < n; k++)
        if (st.sel && memcmp(t.at(idx[k]).bssid, st.sel->bssid, 6) == 0) selPos = k;
    const int first = selPos < kListRows ? 0 : selPos - kListRows + 1;   // 커서가 보이게 스크롤

    char buf[48];
    for (int r = 0; r < kListRows && first + r < n; r++) {
        const int k = first + r;
        const ApInfo& ap = t.at(idx[k]);
        const int y = 8 + r * kListRowH;
        if (k == selPos) c.text(0, y, ">");
        fitText(c, ssidOrHidden(ap), kListSsidMaxPx, buf, sizeof(buf));
        c.text(kListSsidX, y, buf);
        snprintf(buf, sizeof(buf), "ch%u", ap.channel);
        c.text(kListChX, y, buf);
        snprintf(buf, sizeof(buf), "%d", ap.rssi);
        c.text(kListRssiRight - c.textWidth(buf), y, buf);
        const ApHistory& h = t.history(idx[k]);
        float lo = WA_RSSI_LO, hi = WA_RSSI_HI;
        sparkRange(ringStats(h, kApHistLen), kListSparkMinSpan, lo, hi);
        drawRssiGraph(c, h, kListSparkX + (kApHistLen - 1) * kListSparkStep, kListSparkStep, y, kListSparkH, lo, hi);
    }
}

/** 모드 4: 선택 AP 하나의 RSSI를 크게 + 최근 190회 그래프 */
void drawTrack(Canvas512& c, const UiState& st) {
    if (!st.sel) return textCenter(c, 256, 30, "AP 없음");
    const ApInfo& ap = *st.sel;
    char buf[48];
    fitText(c, ssidOrHidden(ap), 124, buf, sizeof(buf));
    c.text(0, 8, buf);
    snprintf(buf, sizeof(buf), "ch%u", ap.channel);
    c.text(0, 19, buf);

    const int8_t now = lastSample(*st.track);
    if (now == kNoSample) {
        textBold(c, 0, 33, "신호 없음");
    } else {
        snprintf(buf, sizeof(buf), "%d dBm", now);
        textBold(c, 0, 33, buf);
        c.frame(0, 37, 124, 6);
        c.box(1, 38, rssiToPx(now, WA_RSSI_LO, WA_RSSI_HI, 122), 4);
    }
    const RingStats rs = ringStats(*st.track, kTrackStatsN);
    if (rs.count) {
        snprintf(buf, sizeof(buf), "최고 %d 평균 %d", rs.maxDbm, (int)(rs.avgDbm - 0.5f));
        c.text(0, 52, buf);
    }
    c.text(0, 62, st.buzzer ? "부저 켜짐" : "부저 꺼짐");

    // 그래프: −40·−60·−80dBm 점선과 눈금
    const int h = kTrackGraphBottom - kTrackGraphTop;
    for (int dbm = -40; dbm >= -80; dbm -= 20) {
        const int y = kTrackGraphBottom - rssiToPx(dbm, WA_RSSI_LO, WA_RSSI_HI, h);
        for (int x = kTrackGraphX + 18; x < 512; x += 6) c.pixel(x, y);
        snprintf(buf, sizeof(buf), "%d", dbm);
        c.text(kTrackGraphX, y + 3, buf);
    }
    drawRssiGraph(c, *st.track, 511, kTrackGraphStep, kTrackGraphBottom, h, WA_RSSI_LO, WA_RSSI_HI);
}

/** 모드 이름 알림 — 아래 그림을 지우고 테두리 상자 안에 "번호/전체 이름" */
void drawModeToast(Canvas512& c, Mode mode) {
    char buf[40];
    snprintf(buf, sizeof(buf), "%d/%d %s", (int)mode + 1, (int)Mode::Count, kModeNames[(int)mode]);
    c.u8g2().setDrawColor(0);
    c.box(kToastX, kToastY, kToastW, kToastH);
    c.u8g2().setDrawColor(1);
    c.frame(kToastX, kToastY, kToastW, kToastH);
    textCenter(c, kToastX + kToastW / 2, kToastY + 14, buf);
}

/** 모드 5: 근처 BLE 기기 수·거리 구간·추이와 센 기기 6개(이름·제조사·RSSI) */
void drawBle(Canvas512& c, const UiState& st) {
    if (st.bleScans == 0) return drawWaiting(c, "BLE 스캔 중...");
    const BleTable& t = *st.ble;
    char buf[48];
    snprintf(buf, sizeof(buf), "BLE %d대", t.count());
    textBold(c, 0, 10, buf);
    const BleZones z = t.zones();
    snprintf(buf, sizeof(buf), "가까움 %d", z.nearCount);
    c.text(0, 25, buf);
    snprintf(buf, sizeof(buf), "중간 %d", z.midCount);
    c.text(0, 37, buf);
    snprintf(buf, sizeof(buf), "멂 %d", z.farCount);
    c.text(0, 49, buf);
    c.text(0, 62, "최근 30초 기준");

    // 추이 — 세로 범위 0~최대(5 단위로 올림)
    const RingStats rs = ringStats(*st.bleHist, kBleHistLen);
    const int top = rs.count ? ((rs.maxDbm + 4) / 5) * 5 : 5;
    snprintf(buf, sizeof(buf), "추이 (최대 %d)", top < 5 ? 5 : top);
    c.text(kBleGraphX, 8, buf);
    c.hline(kBleGraphX, kBleGraphBottom, kBleHistLen * kBleGraphStep);
    drawRssiGraph(c, *st.bleHist, kBleGraphX + (kBleHistLen - 1) * kBleGraphStep, kBleGraphStep, kBleGraphBottom,
                  kBleGraphBottom - kBleGraphTop, 0, top < 5 ? 5 : top);

    // 센 순서 6줄
    uint8_t idx[kBleMax];
    const int n = t.sortedByRssi(idx, kListRows);
    for (int r = 0; r < n; r++) {
        const BleDevice& d = t.at(idx[r]);
        const int y = 8 + r * kListRowH;
        fitText(c, d.name[0] ? d.name : "-", kBleNameMaxPx, buf, sizeof(buf));
        c.text(kBleNameX, y, buf);
        const char* mfr = bleMfrName(d.mfr);
        if (mfr) fitText(c, mfr, kBleMfrMaxPx, buf, sizeof(buf));
        else if (d.mfr == kNoMfr) strlcpy(buf, "-", sizeof(buf));
        else snprintf(buf, sizeof(buf), "0x%04X", d.mfr);
        c.text(kBleMfrX, y, buf);
        snprintf(buf, sizeof(buf), "%d", d.rssi);
        c.text(511 - c.textWidth(buf), y, buf);
    }
}

/** 요약 타일 하나 — 제목, 구분선, 굵은 값, 보조 두 줄 (screen 번째 화면 안) */
void drawTile(Canvas512& c, int screen, const char* title, const char* value, const char* sub1, const char* sub2) {
    const int x = screen * ds::kScreenWidth + kTilePad, w = ds::kScreenWidth - kTilePad * 2;
    char buf[48];
    c.text(x, kTileTitleY, title);
    c.hline(x, kTileRuleY, w);
    textBold(c, x, kTileValueY, value);
    if (sub1) {
        fitText(c, sub1, w, buf, sizeof(buf));
        c.text(x, kTileSub1Y, buf);
    }
    if (sub2) {
        fitText(c, sub2, w, buf, sizeof(buf));
        c.text(x, kTileSub2Y, buf);
    }
}

/** 모드 6: 요약 — WiFi AP 수 · 추천 채널 · 가장 센 AP · BLE 기기 수 */
void drawSummary(Canvas512& c, const ApTable& t, const ChannelStat* stats, const UiState& st) {
    char v[48], s1[48], s2[48];
    if (st.scans == 0) {
        drawTile(c, 0, "WiFi AP", "-", "스캔 중...", nullptr);
        drawTile(c, 1, "추천 채널", "-", nullptr, nullptr);
        drawTile(c, 2, "가장 센 AP", "-", nullptr, nullptr);
    } else {
        int used = 0;
        for (int ch = kMinChannel; ch <= kMaxChannel; ch++) used += stats[ch].apCount ? 1 : 0;
        snprintf(v, sizeof(v), "%d개", t.count());
        snprintf(s1, sizeof(s1), "사용 채널 %d/13", used);
        drawTile(c, 0, "WiFi AP", v, s1, "최근 60초 기준");

        const int rec = recommendChannel(stats, st.traffic), busy = busiestChannel(stats, st.traffic);
        snprintf(v, sizeof(v), "ch%d", rec);
        snprintf(s1, sizeof(s1), "가장 혼잡 ch%d", busy);
        drawTile(c, 1, "추천 채널", v, s1, st.traffic[rec].sweeps ? "활동량 반영" : "활동량 측정 중");

        uint8_t idx[1];
        if (t.sortedByRssi(idx, 1)) {
            const ApInfo& ap = t.at(idx[0]);
            fitText(c, ssidOrHidden(ap), 90, v, sizeof(v));   // 굵게 그리면 글자마다 넓어져 여유를 둔다
            snprintf(s1, sizeof(s1), "ch%u  %d dBm", ap.channel, ap.rssi);
            drawTile(c, 2, "가장 센 AP", v, s1, nullptr);
        } else {
            drawTile(c, 2, "가장 센 AP", "-", nullptr, nullptr);
        }
    }
    if (st.bleScans == 0) {
        drawTile(c, 3, "BLE 기기", "-", "스캔 중...", nullptr);
    } else {
        const BleZones z = st.ble->zones();
        snprintf(v, sizeof(v), "%d대", st.ble->count());
        snprintf(s1, sizeof(s1), "가까움 %d", z.nearCount);
        snprintf(s2, sizeof(s2), "중간 %d  멂 %d", z.midCount, z.farCount);
        drawTile(c, 3, "BLE 기기", v, s1, s2);
    }
}

/** 논리 화면 s를 그릴 물리 화면 — 뒤집혔으면 순서가 반대 */
U8G2& physical(ds::DisplayCore& display, int s, bool flipped) {
    return display.screen(flipped ? ds::kNumScreens - 1 - s : s);
}

}  // namespace

void uiDrawBoot(ds::DisplayCore& display, const char* version, bool flipped) {
    char ver[16];
    snprintf(ver, sizeof(ver), "v%s", version);
    for (int s = 0; s < ds::kNumScreens; s++) {
        U8G2& u = physical(display, s, flipped);
        u.clearBuffer();
        u.setFont(u8g2_font_galmuri7_kr);
        Canvas512 c(u, s);
        const int cx = s * ds::kScreenWidth + ds::kScreenWidth / 2;   // 화면 가운데 — 글자가 경계에 걸리지 않게
        switch (s) {
            case 0: textCenter(c, cx, 36, "Wireless"); break;
            case 1: textCenter(c, cx, 36, "Analyzer"); break;
            case 2:
                textCenter(c, cx, 29, "2.4GHz 무선");
                textCenter(c, cx, 43, "환경 분석기");
                break;
            default:
                textCenter(c, cx, 29, "WiFi / BLE");
                textCenter(c, cx, 43, ver);
                break;
        }
    }
    display.push();
}

void uiDraw(ds::DisplayCore& display, const ApTable& table, const ChannelStat* stats, const UiState& st) {
    for (int s = 0; s < ds::kNumScreens; s++) {
        U8G2& u = physical(display, s, st.flipped);
        u.clearBuffer();
        u.setFont(u8g2_font_galmuri7_kr);
        u.setDrawColor(1);
        Canvas512 c(u, s);
        switch (st.mode) {
            case Mode::Spectrum: drawSpectrum(c, table, st); break;
            case Mode::Channels: drawChannels(c, stats, st); break;
            case Mode::List: drawList(c, table, st); break;
            case Mode::Track: drawTrack(c, st); break;
            case Mode::Ble: drawBle(c, st); break;
            case Mode::Summary: drawSummary(c, table, stats, st); break;
            default: break;
        }
        if (st.paused) drawPaused(c);
        if (st.modeToast) drawModeToast(c, st.mode);
    }
    display.push();
}

}  // namespace wa
