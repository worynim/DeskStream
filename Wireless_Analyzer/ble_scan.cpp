// worynim@gmail.com
/**
 * @file ble_scan.cpp
 * @brief ble_scan.h 구현
 */
#include "ble_scan.h"

#include <NimBLEDevice.h>
#include <string.h>

#include "config.h"

namespace wa {

namespace {
bool g_running = false;
}  // namespace

void bleScanBegin() {
    NimBLEDevice::init("");
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);
}

void bleScanStart() {
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->clearResults();
    g_running = scan->start(WA_BLE_SCAN_MS, false, true);
}

bool bleScanRunning() { return g_running; }

bool bleScanPoll(BleTable& table, uint32_t nowMs) {
    if (!g_running) return false;
    NimBLEScan* scan = NimBLEDevice::getScan();
    if (scan->isScanning()) return false;
    g_running = false;
    NimBLEScanResults r = scan->getResults();
    for (int i = 0; i < r.getCount(); i++) {
        const NimBLEAdvertisedDevice* adv = r.getDevice(i);
        BleDevice d = {};
        memcpy(d.addr, adv->getAddress().getVal(), 6);
        d.rssi = adv->getRSSI();
        const std::string md = adv->getManufacturerData();
        d.mfr = md.size() >= 2 ? (uint16_t)((uint8_t)md[0] | ((uint8_t)md[1] << 8)) : kNoMfr;
        copyUtf8(d.name, adv->getName().c_str(), kBleNameLen);
        d.lastSeenMs = nowMs;
        table.upsert(d);
    }
    scan->clearResults();
    table.prune(nowMs, WA_BLE_TTL_MS);
    return true;
}

void bleScanStop() {
    if (!g_running) return;
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->stop();
    scan->clearResults();
    g_running = false;
}

}  // namespace wa
