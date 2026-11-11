// worynim@gmail.com
/**
 * @file ble_scan.h
 * @brief NimBLE 액티브 스캔 → BleTable 갱신
 * @details 기기 이름은 대부분 스캔 응답에 들어 있어 액티브 스캔(기기에 스캔 요청을 보냄)을 쓴다.
 *          스캔은 비차단 — 끝났는지 매 loop 확인한다. 언제 스캔할지는 scheduler가 정한다 (WiFi와 라디오를 번갈아 쓴다).
 */
#ifndef WA_BLE_SCAN_H
#define WA_BLE_SCAN_H

#include <stdint.h>

#include "ble_table.h"

namespace wa {

/** NimBLE 초기화 (스캔 설정 포함) */
void bleScanBegin();

/** WA_BLE_SCAN_MS 동안 스캔 시작 */
void bleScanStart();

/** 시작한 스캔이 아직 처리되지 않았으면 true */
bool bleScanRunning();

/** 매 loop 호출. 스캔이 끝났으면 결과를 table에 넣고(오래된 기기 정리 포함) true를 돌려준다 */
bool bleScanPoll(BleTable& table, uint32_t nowMs);

/** 진행 중인 스캔을 멈추고 결과를 버린다 */
void bleScanStop();

}  // namespace wa

#endif
