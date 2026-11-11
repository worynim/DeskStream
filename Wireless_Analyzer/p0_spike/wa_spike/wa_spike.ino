// Wireless_Analyzer P0 스파이크 — 측정값 확인용 임시 스케치 (본 펌웨어 아님)
//
// 부팅 후 시험 T1~T4를 차례로 실행하고 결과를 시리얼(115200)에 "[T1] ..." 형식으로 출력한다.
// 이후에는 T5(WiFi 스캔 → 패킷 집계 → BLE 스캔 순환)를 계속 돌리며 주기별 소요 시간과 힙을 출력한다.
//   T1 액티브 스캔 / T2 패시브 스캔 — 소요 시간, AP 수, 채널 12·13 검출
//   T3 프로미스큐어스 채널 순회 — 채널별 프레임 수(관리/제어/데이터), 평균 RSSI
//   T4 BLE 패시브 스캔(NimBLE) — 고유 주소 수, 소요 시간
//   화면: Agent_Monitor와 같은 방식 — 화면별 U8g2 버퍼에 갈무리7 글꼴(drawUTF8)로 그리고 display.push()
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <NimBLEDevice.h>
// 갈무리7 한글 글꼴은 64KB 를 넘어 U8g2 가 큰 글꼴 지원을 켜야 한다 — U8g2lib.h(DeskStreamCore.h) 보다 먼저 정의
#ifndef U8G2_USE_LARGE_FONTS
#define U8G2_USE_LARGE_FONTS
#endif
#include <DeskStreamCore.h>
#include "u8g2_font_galmuri7_kr.h"

static ds::DisplayCore display;

// ---------------------------------------------------------------- 화면
/** 512px 폭 가상 좌표로 줄을 그린다 — 화면 i에는 x를 -128*i 만큼 옮겨 그리고 넘치는 부분은 U8g2가 잘라낸다 */
static void drawLines(const String* lines, int n) {
    for (int s = 0; s < ds::kNumScreens; s++) {
        U8G2& u = display.screen(s);
        u.clearBuffer();
        u.setFont(u8g2_font_galmuri7_kr);
        for (int i = 0; i < n && i < 6; i++) u.drawUTF8(-s * ds::kScreenWidth, 9 + i * 10, lines[i].c_str());
    }
    display.push();
}

/** (x0,x1) 사이 x에서 직선의 y — 반올림 */
static int lineYAt(int x0, int y0, int x1, int y1, int x) {
    if (x1 == x0) return y0;
    const int num = (y1 - y0) * (x - x0) * 2 / (x1 - x0);
    return y0 + (num + (num >= 0 ? 1 : -1)) / 2;
}

/**
 * 512px 가상 좌표 직선을 화면 s에 그린다.
 * U8g2 좌표는 부호 없는 16비트라 음수 x를 넘기면 drawLine이 양 끝을 잘못 비교해 엉뚱하게 그린다
 * (글자·가로선은 잘리기만 해서 괜찮다). 그래서 화면 s의 x 범위로 먼저 잘라 0~127 좌표만 넘긴다.
 */
static void drawLine512(U8G2& u, int s, int x0, int y0, int x1, int y1) {
    if (x0 > x1) { std::swap(x0, x1); std::swap(y0, y1); }
    const int left = s * ds::kScreenWidth, right = left + ds::kScreenWidth - 1;
    if (x1 < left || x0 > right) return;
    const int cx0 = x0 < left ? left : x0, cx1 = x1 > right ? right : x1;
    u.drawLine(cx0 - left, lineYAt(x0, y0, x1, y1, cx0), cx1 - left, lineYAt(x0, y0, x1, y1, cx1));
}

/** 화면 경계 테스트 — 글자·도형이 x=128·256·384 경계에 걸치게 그려 옆 화면으로 이어지는지 본다 */
static void drawBoundaryTest() {
    for (int s = 0; s < ds::kNumScreens; s++) {
        U8G2& u = display.screen(s);
        const int ox = -s * ds::kScreenWidth;   // 가상 x → 화면 s의 x
        u.clearBuffer();
        u.setFont(u8g2_font_galmuri7_kr);
        for (int b = 1; b < ds::kNumScreens; b++) {
            const int bx = b * ds::kScreenWidth;
            u.drawUTF8(ox + bx - 24, 9, "경계한글ABC");        // 한글이 경계에 걸침
            u.drawFrame(ox + bx - 10, 14, 20, 12);            // 상자가 경계에 걸침
            u.drawUTF8(ox + bx - 16, 39, "ch12 -45");         // 영문·숫자가 경계에 걸침
        }
        u.drawHLine(ox, 50, 512);                             // 4화면을 잇는 가로선
        drawLine512(u, s, 0, 63, 511, 52);                    // 4화면을 잇는 사선 (끊김 없이 이어져야 함)
    }
    display.push();
}

// ---------------------------------------------------------------- T1/T2 WiFi 스캔
static int g_apCount = 0;

static void wifiScan(const char* tag, bool passive, uint32_t msPerChan) {
    const uint32_t t0 = millis();
    int n = WiFi.scanNetworks(false, true, passive, msPerChan);
    const uint32_t dt = millis() - t0;
    int perCh[15] = {0};
    for (int i = 0; i < n; i++) {
        int ch = WiFi.channel(i);
        if (ch >= 1 && ch <= 14) perCh[ch]++;
    }
    Serial.printf("[%s] passive=%d msPerChan=%u -> %d APs in %u ms | ch:", tag, passive, msPerChan, n, dt);
    for (int ch = 1; ch <= 14; ch++) Serial.printf(" %d", perCh[ch]);
    Serial.println();
    if (strcmp(tag, "T1") == 0) {
        for (int i = 0; i < n; i++) {
            delay(5);   // CDC 버퍼 넘침으로 줄이 빠지지 않게
            Serial.printf("  %2d ch%2d %4d dBm enc=%d %s [%s]\n", i, WiFi.channel(i), WiFi.RSSI(i),
                          WiFi.encryptionType(i), WiFi.BSSIDstr(i).c_str(), WiFi.SSID(i).c_str());
        }
        // 화면: 센 순서(스캔 결과는 RSSI 내림차순) 상위 6개 SSID — 한글 렌더링 확인
        String lines[6];
        for (int i = 0; i < 6 && i < n; i++)
            lines[i] = String(WiFi.channel(i)) + " " + String(WiFi.RSSI(i)) + " " + WiFi.SSID(i);
        drawLines(lines, n < 6 ? n : 6);
        g_apCount = n;
    }
    WiFi.scanDelete();
}

// ---------------------------------------------------------------- T3 프로미스큐어스
struct ChanStat { volatile uint32_t mgmt, ctrl, data, misc; volatile int32_t rssiSum; };
static ChanStat g_stat[14];
static volatile uint8_t g_curCh = 1;

static void IRAM_ATTR onPkt(void* buf, wifi_promiscuous_pkt_type_t type) {
    const wifi_promiscuous_pkt_t* p = (const wifi_promiscuous_pkt_t*)buf;
    ChanStat& s = g_stat[g_curCh];
    switch (type) {
        case WIFI_PKT_MGMT: s.mgmt++; break;
        case WIFI_PKT_CTRL: s.ctrl++; break;
        case WIFI_PKT_DATA: s.data++; break;
        default: s.misc++; break;
    }
    s.rssiSum += p->rx_ctrl.rssi;
}

static uint32_t sniffSweep(uint32_t dwellMs, bool print) {
    memset((void*)g_stat, 0, sizeof(g_stat));
    wifi_promiscuous_filter_t f = {.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL};
    esp_wifi_set_promiscuous_filter(&f);
    esp_wifi_set_promiscuous_rx_cb(onPkt);
    const uint32_t t0 = millis();
    esp_wifi_set_promiscuous(true);
    for (uint8_t ch = 1; ch <= 13; ch++) {
        g_curCh = ch;
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        delay(dwellMs);
    }
    esp_wifi_set_promiscuous(false);
    const uint32_t dt = millis() - t0;
    if (print) {
        Serial.printf("[T3] dwell=%u ms sweep=%u ms\n", dwellMs, dt);
        for (int ch = 1; ch <= 13; ch++) {
            const ChanStat& s = g_stat[ch];
            delay(5);
            uint32_t tot = s.mgmt + s.ctrl + s.data + s.misc;
            Serial.printf("  ch%2d total=%5u (%5u/s) mgmt=%4u ctrl=%4u data=%5u avgRssi=%d\n", ch, tot,
                          tot * 1000 / dwellMs, s.mgmt, s.ctrl, s.data, tot ? (int)(s.rssiSum / (int32_t)tot) : 0);
        }
    }
    return dt;
}

// ---------------------------------------------------------------- T4 BLE
static int bleScan(uint32_t ms, bool print) {
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(false);
    scan->setInterval(100);
    scan->setWindow(99);
    scan->setDuplicateFilter(false);
    const uint32_t t0 = millis();
    NimBLEScanResults r = scan->getResults(ms, false);
    const uint32_t dt = millis() - t0;
    int n = r.getCount();
    if (print) {
        int randomAddr = 0, near = 0;
        for (int i = 0; i < n; i++) {
            const NimBLEAdvertisedDevice* d = r.getDevice(i);
            if (d->getAddress().getType() != BLE_ADDR_PUBLIC) randomAddr++;
            if (d->getRSSI() > -70) near++;
            std::string md = d->getManufacturerData();
            uint16_t mfr = md.size() >= 2 ? (uint8_t)md[0] | ((uint8_t)md[1] << 8) : 0xFFFF;
            Serial.printf("  %s type=%d %4d dBm mfr=0x%04X name=[%s]\n", d->getAddress().toString().c_str(),
                          d->getAddress().getType(), d->getRSSI(), mfr, d->getName().c_str());
        }
        Serial.printf("[T4] scan=%u ms (요청 %u) -> %d devices (random addr %d, >-70dBm %d)\n", dt, ms, n, randomAddr, near);
    }
    scan->clearResults();
    return n;
}

// ---------------------------------------------------------------- setup / loop
void setup() {
    Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(0);
#endif
    delay(1500);
    display.begin();
    String boot[2] = {"Wireless_Analyzer P0 spike", "한글 테스트: 공유기_5G 와이파이"};
    drawLines(boot, 2);
    delay(2000);
    drawBoundaryTest();
    delay(8000);

    Serial.printf("[INFO] heap free=%u (boot)\n", ESP.getFreeHeap());
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_err_t e = esp_wifi_set_country_code("KR", true);
    wifi_country_t c;
    esp_wifi_get_country(&c);
    Serial.printf("[INFO] country set=%s -> cc=%.2s schan=%d nchan=%d\n", esp_err_to_name(e), c.cc, c.schan, c.nchan);
    Serial.printf("[INFO] heap free=%u (wifi on)\n", ESP.getFreeHeap());

    wifiScan("T1", false, 120);     // 액티브 (채널당 기본값 근처)
    wifiScan("T1b", false, 60);     // 액티브 짧게
    wifiScan("T2", true, 300);      // 패시브 (비콘 주기 102ms × 약 3)
    sniffSweep(150, true);
    sniffSweep(500, true);          // 머무는 시간을 늘리면 집계가 비례해 늘어나는지

    NimBLEDevice::init("");
    Serial.printf("[INFO] heap free=%u (wifi+ble on)\n", ESP.getFreeHeap());
    bleScan(3000, true);
}

void loop() {
    // T5: 실제 사용 패턴 순환 — 단계별 소요 시간·힙·결과 개수
    static uint32_t cycle = 0;
    uint32_t t0 = millis();
    int n = WiFi.scanNetworks(false, true, false, 120);
    WiFi.scanDelete();
    uint32_t t1 = millis();
    sniffSweep(150, false);
    uint32_t total = 0;
    for (int ch = 1; ch <= 13; ch++) total += g_stat[ch].mgmt + g_stat[ch].ctrl + g_stat[ch].data + g_stat[ch].misc;
    uint32_t t2 = millis();
    int b = bleScan(3000, false);
    uint32_t t3 = millis();
    Serial.printf("[T5] #%u wifi=%u ms (%d AP) sniff=%u ms (%u pkts) ble=%u ms (%d dev) cycle=%u ms heap=%u min=%u\n",
                  ++cycle, t1 - t0, n, t2 - t1, total, t3 - t2, b, t3 - t0, ESP.getFreeHeap(), ESP.getMinFreeHeap());
    String lines[3] = {"T5 cycle #" + String(cycle) + "  " + String(t3 - t0) + " ms",
                       "AP " + String(n) + "  pkts " + String(total) + "  BLE " + String(b),
                       "heap " + String(ESP.getFreeHeap())};
    drawLines(lines, 3);
}
