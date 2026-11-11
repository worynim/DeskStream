// worynim@gmail.com
/**
 * @file wifi_scan.h
 * @brief 비동기 WiFi 스캔 → ApTable 갱신
 * @details 스캔 중에도 loop()가 돌아 버튼·화면이 멈추지 않는다. 공유기에는 접속하지 않는다.
 *          언제 스캔할지는 scheduler가 정한다 (채널 순회 측정과 라디오를 번갈아 쓴다).
 */
#ifndef WA_WIFI_SCAN_H
#define WA_WIFI_SCAN_H

#include <stdint.h>

#include "analysis.h"

namespace wa {

/** STA 모드로 켜고 국가 코드를 설정한다 */
void wifiScanBegin();

/** 스캔 시작 — channel이 0이면 채널 1~13 전체, 아니면 그 채널만 (추적 모드) */
void wifiScanStart(uint8_t channel);

/** 시작한 스캔이 아직 처리되지 않았으면 true */
bool wifiScanRunning();

/**
 * 매 loop 호출. 진행 중인 스캔이 끝났으면 결과를 table에 넣고(RSSI 기록·오래된 AP 정리 포함) true를 돌려준다.
 * 실패하면 false를 돌려주고 진행 중 상태를 푼다 (다음 시작은 호출 쪽이 한다).
 */
bool wifiScanPoll(ApTable& table, uint32_t nowMs);

/** 마지막으로 끝난 스캔이 채널 하나만 본 스캔이면 그 채널, 전체 스캔이면 0 */
uint8_t wifiScanLastChannel();

/** 지금까지 끝난 스캔 횟수 */
uint32_t wifiScanCount();

}  // namespace wa

#endif
