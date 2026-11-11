// worynim@gmail.com
/**
 * @file ble_table.h
 * @brief 근처 BLE 기기 목록 — 주소로 중복을 없애고 최근 본 기기만 센다
 * @details Arduino 의존 없는 순수 로직이라 호스트에서 테스트한다 (test/test_analysis.cpp).
 *          주소는 같은 기기를 구분하는 데만 쓰고 화면에 표시하거나 장치 밖으로 보내지 않는다 (PLAN §5 결정 3).
 *          휴대폰은 주소를 주기적으로(약 15분) 바꾸므로, "최근 ttl 동안 본 서로 다른 주소 수"를 기기 수로 본다.
 */
#ifndef WA_BLE_TABLE_H
#define WA_BLE_TABLE_H

#include <stdint.h>

namespace wa {

constexpr int kBleMax = 64;          ///< 테이블 크기 — 가득 차면 가장 약한 기기를 밀어낸다
constexpr int kBleNameLen = 24;      ///< 이름 저장 바이트 (UTF-8 글자 중간에서 자르지 않음, NUL 포함)
constexpr uint16_t kNoMfr = 0xFFFF;  ///< 제조사 데이터 없음

struct BleDevice {
    uint8_t addr[6];
    int8_t rssi;
    uint16_t mfr;              ///< 제조사 데이터 앞 2바이트(Bluetooth SIG 회사 ID), 없으면 kNoMfr
    char name[kBleNameLen];    ///< 광고한 이름, 없으면 빈 문자열
    uint32_t lastSeenMs;
};

/** 거리 구간 기준 (RSSI) */
constexpr int kBleNearDbm = -60;   ///< 이보다 세면 가까움
constexpr int kBleFarDbm = -80;    ///< 이보다 약하면 멂

struct BleZones {
    int nearCount, midCount, farCount;
};

class BleTable {
public:
    /** 새로 보거나 갱신 (이름은 새 값이 비어 있으면 전에 받은 이름을 유지). 가득 차면 더 센 기기만 가장 약한 기기를 대신한다 */
    void upsert(const BleDevice& d);
    /** now - lastSeenMs > ttlMs 인 기기를 지운다 */
    void prune(uint32_t nowMs, uint32_t ttlMs);
    int count() const { return _count; }
    const BleDevice& at(int i) const { return _d[i]; }
    /** RSSI 내림차순 인덱스를 out에 채우고 개수를 돌려준다 */
    int sortedByRssi(uint8_t* out, int maxOut) const;
    BleZones zones() const;

private:
    BleDevice _d[kBleMax];
    int _count = 0;
};

/** src를 dst(크기 n)에 복사하되 UTF-8 글자 중간에서 자르지 않는다 */
void copyUtf8(char* dst, const char* src, int n);

/** 회사 ID → 짧은 제조사 이름. 모르는 ID면 nullptr */
const char* bleMfrName(uint16_t id);

}  // namespace wa

#endif
