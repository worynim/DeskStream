// worynim@gmail.com
/**
 * @file ui.h
 * @brief 화면 모드 렌더러 — 채널 스펙트럼, 채널 혼잡도 막대, AP 목록, AP 추적, BLE 기기, 요약, 부팅 화면
 */
#ifndef WA_UI_H
#define WA_UI_H

#include <DeskStreamCore.h>

#include "analysis.h"
#include "ble_table.h"
#include "traffic.h"

namespace wa {

enum class Mode : uint8_t { Spectrum, Channels, List, Track, Ble, Summary, Count };

constexpr int kBleHistLen = 60;               ///< BLE 기기 수 기록 (스캔 1회 = 1개)
using BleCountHistory = RssiRing<kBleHistLen>;   ///< 기기 수는 최대 64라 int8 링버퍼를 그대로 쓴다

/** 화면에 필요한 상태 */
struct UiState {
    Mode mode;
    bool paused;
    uint32_t scans;              ///< 끝난 스캔 횟수 (0이면 아직 결과 없음)
    const ApInfo* sel;           ///< 선택한 AP (목록 커서·추적 대상), 없으면 nullptr — 테이블에서 지워져도 마지막 정보 유지
    const TrackHistory* track;   ///< 추적 모드 RSSI 기록 (선택 AP)
    bool buzzer;                 ///< 추적 부저 켜짐
    const ChannelTraffic* traffic;   ///< 채널별 실측 활동량 [kMaxChannel + 1] (sweeps 0 = 아직 없음)
    bool modeToast;                  ///< 모드 이름 알림을 띄운다 (모드를 바꾼 직후 잠깐)
    const BleTable* ble;             ///< 근처 BLE 기기
    const BleCountHistory* bleHist;  ///< BLE 기기 수 기록
    uint32_t bleScans;               ///< 끝난 BLE 스캔 횟수 (0이면 아직 결과 없음)
    bool flipped;                    ///< 화면 180° 뒤집힘 — 패널은 setFlip, 화면 순서는 여기서 반대로 그린다
};

/** 부팅 화면 (이름·버전) — flipped면 화면 순서를 반대로 */
void uiDrawBoot(ds::DisplayCore& display, const char* version, bool flipped);

/** 현재 모드를 4화면에 그리고 보낸다 */
void uiDraw(ds::DisplayCore& display, const ApTable& table, const ChannelStat* stats, const UiState& st);

}  // namespace wa

#endif
