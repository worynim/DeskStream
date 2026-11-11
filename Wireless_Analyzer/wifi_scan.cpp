// worynim@gmail.com
/**
 * @file wifi_scan.cpp
 * @brief wifi_scan.h 구현
 */
#include "wifi_scan.h"

#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

#include "config.h"

namespace wa {

namespace {
bool g_running = false;
uint8_t g_scanChannel = 0;   // 진행 중인 스캔의 채널 (0 = 전체)
uint8_t g_lastChannel = 0;   // 마지막으로 끝난 스캔의 채널
uint32_t g_count = 0;
}  // namespace

void wifiScanBegin() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_err_t e = esp_wifi_set_country_code(WA_COUNTRY_CODE, true);
    if (e != ESP_OK) Serial.printf("[WIFI] country code %s\n", esp_err_to_name(e));
}

void wifiScanStart(uint8_t channel) {
    // async, show_hidden, active, 채널당 ms, 채널(0 = 전체)
    WiFi.scanNetworks(true, true, false, WA_SCAN_MS_PER_CH, channel);
    g_scanChannel = channel;
    g_running = true;
}

bool wifiScanRunning() { return g_running; }

bool wifiScanPoll(ApTable& table, uint32_t nowMs) {
    if (!g_running) return false;
    const int16_t n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return false;
    g_running = false;
    if (n == WIFI_SCAN_FAILED) {
        Serial.println("[WIFI] scan failed");
        return false;
    }
    for (int i = 0; i < n; i++) {
        ApInfo ap = {};
        memcpy(ap.bssid, WiFi.BSSID(i), 6);
        strlcpy(ap.ssid, WiFi.SSID(i).c_str(), sizeof(ap.ssid));
        ap.channel = (uint8_t)WiFi.channel(i);
        ap.rssi = (int8_t)WiFi.RSSI(i);
        ap.enc = (uint8_t)WiFi.encryptionType(i);
        ap.lastSeenMs = nowMs;
        table.upsert(ap);
    }
    WiFi.scanDelete();
    table.recordSample(nowMs, g_scanChannel);
    table.prune(nowMs, WA_AP_TTL_MS, g_scanChannel);
    g_lastChannel = g_scanChannel;
    g_count++;
    return true;
}

uint32_t wifiScanCount() { return g_count; }

uint8_t wifiScanLastChannel() { return g_lastChannel; }

}  // namespace wa
