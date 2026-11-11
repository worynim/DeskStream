// worynim@gmail.com
/**
 * @file scheduler.h
 * @brief 라디오 시분할 스케줄러 — WiFi 스캔, 채널 순회 측정, BLE 스캔을 번갈아 실행한다
 * @details C3는 라디오가 하나라 셋을 동시에 못 한다. 하나가 진행 중이면 끝날 때까지 다른 것을 시작하지 않는다.
 *          순서: WiFi 스캔 → (sniff면) 순회 한 바퀴 → (ble면) BLE 스캔 → WiFi 스캔 … . 켜진 것만 돈다.
 *          일시정지면 새로 시작하지 않는다 (순회·BLE는 바로 멈추고, WiFi 스캔은 끝까지 기다린다).
 */
#ifndef WA_SCHEDULER_H
#define WA_SCHEDULER_H

#include <stdint.h>

#include "analysis.h"
#include "ble_table.h"
#include "traffic.h"

namespace wa {

/** 이번 loop에서 바라는 동작 */
struct SchedConfig {
    bool paused;
    bool wifi;             ///< WiFi 스캔 (BLE 화면에서는 끈다)
    uint8_t scanChannel;   ///< 0 = 전체 스캔, 아니면 그 채널만 (추적 모드)
    bool sniff;            ///< WiFi 스캔 뒤 채널 순회 측정 (혼잡도 모드)
    bool ble;              ///< BLE 스캔 (BLE 화면)
};

enum : uint8_t { kEvScanDone = 1, kEvSniffDone = 2, kEvBleDone = 4 };

/** 매 loop 호출. 이번에 끝난 일을 kEv* 비트로 돌려준다 */
uint8_t schedulerPoll(ApTable& table, ChannelTraffic* traffic, BleTable& ble, const SchedConfig& cfg, uint32_t nowMs);

}  // namespace wa

#endif
