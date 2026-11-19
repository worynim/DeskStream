// worynim@gmail.com
/**
 * @file hid_ble.h
 * @brief BLE HID 장치 (키보드 · 미디어 키 · 마우스 휠), 신호 전송 태스크, 연결 기기 슬롯 1~3
 */
#pragma once
#include <stdint.h>

#include "model.h"

namespace hid {

constexpr int kNumSlots = 3;

struct Job {
    Signal signal;
    bool up;           // true = BTN1(위/이전), false = BTN2(아래/다음)
    uint8_t count;     // 휠 칸 수·방향키 횟수 (그 외 1)
    uint8_t gapMs;     // 칸·키 사이 간격
    bool parkCursor;   // 휠 전에 커서를 화면 오른쪽 끝으로
};

void begin();                 // NimBLE 초기화, 슬롯 불러오기, 광고 시작, 전송 태스크 생성
bool connected();
bool takeStateChanged();      // 연결·슬롯 상태가 바뀌었으면 true를 돌려주고 지운다 (화면 갱신용)

// 미연결이거나 이전 신호를 보내는 중이면 false (버린다). 아니면 전송 태스크에 넘기고 true
bool post(const Job& job);

// ---------- 슬롯 ----------
// 페어링된 슬롯을 고르면 그 기기만 연결을 받고(다른 기기에는 보이지 않음),
// 빈 슬롯을 고르면 페어링 대기 — 처음 본딩되는 새 기기가 그 슬롯에 들어간다.
int activeSlot();             // 쓰는 슬롯 (페어링 대기 중엔 새 기기를 받을 슬롯)
bool slotPaired(int s);
void selectSlot(int s, uint32_t nowMs);
void repairSlot(int s, uint32_t nowMs);       // 새 기기 페어링 대기 — 기존 기기는 새 기기가 본딩될 때 교체 (취소하면 그대로)
bool pairing();                               // 새 기기 페어링 대기 중
uint8_t pairingSecondsLeft(uint32_t nowMs);   // 쓰던 슬롯으로 돌아가기까지 남은 초, 0 = 돌아갈 기기 없음
void cancelPairing();                         // 쓰던 슬롯(기존 기기)으로 돌아간다 (돌아갈 기기가 있을 때만)
void loop(uint32_t nowMs);                    // 페어링 대기 60초가 지나면 cancelPairing()

}  // namespace hid
