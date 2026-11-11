// worynim@gmail.com
/**
 * @file wifi_sniff.cpp
 * @brief wifi_sniff.h 구현
 */
#include "wifi_sniff.h"

#include <Arduino.h>
#include <esp_wifi.h>

#include "analysis.h"
#include "config.h"

namespace wa {

namespace {
bool g_running = false;
uint8_t g_channel = 0;
uint32_t g_dwellStartUs = 0;

// 콜백(WiFi 태스크)과 loop가 함께 쓰는 집계 — g_lock으로 보호
portMUX_TYPE g_lock = portMUX_INITIALIZER_UNLOCKED;
uint8_t g_cbChannel = 0;
uint32_t g_frames = 0;

void onPacket(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_CTRL && type != WIFI_PKT_DATA) return;   // 비콘 등 관리 프레임은 활동량이 아니다
    const wifi_pkt_rx_ctrl_t& rx = ((const wifi_promiscuous_pkt_t*)buf)->rx_ctrl;
    if (rx.rx_state != 0) return;                                  // 오류 프레임은 필드를 믿을 수 없다
    portENTER_CRITICAL(&g_lock);
    if (rx.channel == g_cbChannel) g_frames++;   // 채널을 바꾸는 순간 앞 채널에서 받은 프레임은 버린다
    portEXIT_CRITICAL(&g_lock);
}

void tuneTo(uint8_t ch) {
    portENTER_CRITICAL(&g_lock);
    g_cbChannel = ch;
    g_frames = 0;
    portEXIT_CRITICAL(&g_lock);
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    g_channel = ch;
    g_dwellStartUs = micros();
}
}  // namespace

void sniffStart() {
    wifi_promiscuous_filter_t f = {.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL};
    esp_wifi_set_promiscuous_filter(&f);
    wifi_promiscuous_filter_t cf = {.filter_mask = WIFI_PROMIS_CTRL_FILTER_MASK_ALL};
    esp_wifi_set_promiscuous_ctrl_filter(&cf);   // FILTER_MASK_ALL만으로는 제어 프레임이 안 들어온다 (P0)
    esp_wifi_set_promiscuous_rx_cb(onPacket);
    esp_wifi_set_promiscuous(true);
    g_running = true;
    tuneTo(kMinChannel);
}

bool sniffRunning() { return g_running; }

bool sniffPoll(ChannelTraffic* traffic) {
    if (!g_running) return false;
    const uint32_t dwellUs = micros() - g_dwellStartUs;
    if (dwellUs < WA_SNIFF_DWELL_MS * 1000UL) return false;
    portENTER_CRITICAL(&g_lock);
    const uint32_t frames = g_frames;
    portEXIT_CRITICAL(&g_lock);
    trafficUpdate(traffic[g_channel], frames, dwellUs, WA_SNIFF_ALPHA);
    if (g_channel < kMaxChannel) {
        tuneTo(g_channel + 1);
        return false;
    }
    sniffStop();
    return true;
}

void sniffStop() {
    if (!g_running) return;
    esp_wifi_set_promiscuous(false);
    portENTER_CRITICAL(&g_lock);
    g_cbChannel = 0;
    portEXIT_CRITICAL(&g_lock);
    g_running = false;
}

}  // namespace wa
