// worynim@gmail.com
/**
 * @file traffic.h
 * @brief 채널 활동량 — 채널마다 받은 제어·데이터 프레임 수/초의 지수 평균
 * @details Arduino 의존 없는 순수 로직이라 호스트에서 테스트한다 (test/test_analysis.cpp).
 *          ESP32-C3는 스트림이 1개라 2스트림 데이터 프레임(대용량 전송 본체)을 해독하지 못한다. 그래서 전송 시간 합(사용률)은
 *          부하를 거의 반영하지 못했고(PLAN §3.5), 대신 그 응답(Block ACK 등 제어 프레임)과 해독되는 데이터 프레임의 수를 센다.
 *          비콘 등 관리 프레임은 AP 수에 비례할 뿐이라 빼고, 오류 프레임(rx_state ≠ 0)은 필드를 믿을 수 없어 뺀다.
 */
#ifndef WA_TRAFFIC_H
#define WA_TRAFFIC_H

#include <stdint.h>

namespace wa {

/** 채널 하나의 활동량 — 순회할 때마다 지수 평균으로 갱신 */
struct ChannelTraffic {
    float fps;         ///< 초당 제어·데이터 프레임 수
    uint16_t sweeps;   ///< 측정한 횟수 (0이면 값 없음)
};

/** 한 번 머문 결과로 갱신 — 처음이면 그대로, 이후는 alpha 비율로 섞는다 */
void trafficUpdate(ChannelTraffic& t, uint32_t frames, uint32_t dwellUs, float alpha);

}  // namespace wa

#endif
