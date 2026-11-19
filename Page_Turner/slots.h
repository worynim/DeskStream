// worynim@gmail.com
/**
 * @file slots.h
 * @brief 연결 기기 슬롯 1~3 — 슬롯마다 기기 신원 주소("aa:bb:cc:dd:ee:ff") 하나 (하드웨어 무관 — 호스트 테스트 대상)
 */
#pragma once
#include <stdint.h>

class SlotTable {
public:
    static constexpr int kSlots = 3;
    static constexpr int kAddrLen = 18;   // "aa:bb:cc:dd:ee:ff" + '\0'

    bool isEmpty(int s) const { return _addr[s][0] == '\0'; }
    const char* addr(int s) const { return _addr[s]; }
    int find(const char* a) const;        // 대소문자 무시, 없으면 -1
    int firstEmpty() const;               // 없으면 -1
    void assign(int s, const char* a);    // 다른 슬롯에 같은 기기가 있으면 그 슬롯은 비운다
    void clear(int s);
    void adopt(const char* a);            // 없으면 첫 빈 슬롯에 넣는다 (빈 슬롯이 없으면 무시)

    // 슬롯 pairingSlot에서 새 기기를 기다리다 그만둘 때 돌아갈 슬롯:
    // preferred가 페어링돼 있고 pairingSlot이 아니면 그 슬롯, 아니면 다른 첫 페어링 슬롯, 없으면 -1
    int returnTarget(int pairingSlot, int preferred) const;

private:
    char _addr[kSlots][kAddrLen] = {};
};
