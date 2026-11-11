// worynim@gmail.com
/**
 * @file ble_table.cpp
 * @brief ble_table.h 구현
 */
#include "ble_table.h"

#include <string.h>

namespace wa {

void BleTable::upsert(const BleDevice& d) {
    for (int i = 0; i < _count; i++) {
        if (memcmp(_d[i].addr, d.addr, 6) == 0) {
            char keep[kBleNameLen];
            memcpy(keep, _d[i].name, kBleNameLen);
            _d[i] = d;
            if (!d.name[0]) memcpy(_d[i].name, keep, kBleNameLen);   // 스캔 응답이 없던 회차에도 이름 유지
            return;
        }
    }
    if (_count < kBleMax) {
        _d[_count++] = d;
        return;
    }
    int weakest = 0;
    for (int i = 1; i < _count; i++)
        if (_d[i].rssi < _d[weakest].rssi) weakest = i;
    if (d.rssi > _d[weakest].rssi) _d[weakest] = d;
}

void BleTable::prune(uint32_t nowMs, uint32_t ttlMs) {
    int w = 0;
    for (int i = 0; i < _count; i++)
        if (nowMs - _d[i].lastSeenMs <= ttlMs) _d[w++] = _d[i];
    _count = w;
}

int BleTable::sortedByRssi(uint8_t* out, int maxOut) const {
    uint8_t idx[kBleMax];
    for (int i = 0; i < _count; i++) {   // 삽입 정렬, 같은 RSSI는 먼저 들어온 순서 유지
        int pos = i;
        while (pos > 0 && _d[idx[pos - 1]].rssi < _d[i].rssi) {
            idx[pos] = idx[pos - 1];
            pos--;
        }
        idx[pos] = (uint8_t)i;
    }
    const int n = _count < maxOut ? _count : maxOut;
    memcpy(out, idx, n);
    return n;
}

BleZones BleTable::zones() const {
    BleZones z = {0, 0, 0};
    for (int i = 0; i < _count; i++) {
        if (_d[i].rssi > kBleNearDbm) z.nearCount++;
        else if (_d[i].rssi < kBleFarDbm) z.farCount++;
        else z.midCount++;
    }
    return z;
}

void copyUtf8(char* dst, const char* src, int n) {
    int len = 0;
    while (src[len] && len < n - 1) len++;
    if (src[len]) {   // 잘렸으면 글자 시작까지 되돌린다
        while (len > 0 && ((uint8_t)src[len] & 0xC0) == 0x80) len--;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

const char* bleMfrName(uint16_t id) {
    // Bluetooth SIG Assigned Numbers — 회사 ID (자주 보이는 것만)
    switch (id) {
        case 0x0006: return "Microsoft";
        case 0x004C: return "Apple";
        case 0x0059: return "Nordic";
        case 0x0075: return "Samsung";
        case 0x0087: return "Garmin";
        case 0x009E: return "Bose";
        case 0x00C4: return "LG";
        case 0x00E0: return "Google";
        case 0x012D: return "Sony";
        case 0x0157: return "Huami";
        case 0x0171: return "Amazon";
        case 0x01DA: return "Logitech";
        case 0x027D: return "Huawei";
        case 0x02E5: return "Espressif";
        case 0x038F: return "Xiaomi";
        default: return nullptr;
    }
}

}  // namespace wa
