// worynim@gmail.com
/**
 * @file scheduler.cpp
 * @brief scheduler.h 구현
 */
#include "scheduler.h"

#include "ble_scan.h"
#include "wifi_scan.h"
#include "wifi_sniff.h"

namespace wa {

namespace {
bool g_bleTurn = false;   // WiFi와 BLE가 둘 다 켜져 있을 때 다음 차례가 BLE인지
}  // namespace

uint8_t schedulerPoll(ApTable& table, ChannelTraffic* traffic, BleTable& ble, const SchedConfig& cfg, uint32_t nowMs) {
    uint8_t ev = 0;
    // 1) 진행 중인 일 마무리 — 하나라도 진행 중이면 새로 시작하지 않는다
    if (sniffRunning()) {
        if (!cfg.sniff || cfg.paused) sniffStop();   // 혼잡도 모드를 벗어나거나 일시정지하면 바로 그만둔다
        else if (sniffPoll(traffic)) ev |= kEvSniffDone;
        if (sniffRunning()) return ev;
        g_bleTurn = true;
    }
    if (bleScanRunning()) {
        if (!cfg.ble || cfg.paused) bleScanStop();
        else if (bleScanPoll(ble, nowMs)) ev |= kEvBleDone;
        if (bleScanRunning()) return ev;
        g_bleTurn = false;
    }
    if (wifiScanRunning()) {
        if (!wifiScanPoll(table, nowMs)) {
            if (wifiScanRunning()) return ev;
        } else {
            ev |= kEvScanDone;
            g_bleTurn = true;
            if (cfg.sniff && !cfg.paused) {
                sniffStart();
                return ev;
            }
        }
    }
    // 2) 다음 일 시작
    if (cfg.paused) return ev;
    if (cfg.ble && (g_bleTurn || !cfg.wifi)) bleScanStart();
    else if (cfg.wifi) wifiScanStart(cfg.scanChannel);
    return ev;
}

}  // namespace wa
