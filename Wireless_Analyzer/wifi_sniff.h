// worynim@gmail.com
/**
 * @file wifi_sniff.h
 * @brief 프로미스큐어스 채널 순회 — 채널 1~13에 차례로 머물며 받은 제어·데이터 프레임 수로 채널 활동량을 잰다
 * @details WiFi 스캔과 라디오를 같이 쓸 수 없어 scheduler가 스캔 사이에 한 바퀴씩 돌린다.
 *          대용량 전송 본체(2스트림)는 해독되지 않아 그 응답인 제어 프레임(Block ACK 등)이 주된 단서다 (traffic.h).
 */
#ifndef WA_WIFI_SNIFF_H
#define WA_WIFI_SNIFF_H

#include <stdint.h>

#include "traffic.h"

namespace wa {

/** 채널 1부터 순회를 시작한다 */
void sniffStart();

/** 순회 중이면 true */
bool sniffRunning();

/**
 * 매 loop 호출. 머문 시간이 지나면 그 채널 결과로 traffic[채널]을 갱신하고 다음 채널로 옮긴다.
 * 채널 13까지 끝나면 프로미스큐어스를 끄고 true를 돌려준다. traffic은 [kMaxChannel + 1] 배열.
 */
bool sniffPoll(ChannelTraffic* traffic);

/** 순회를 중간에 멈춘다 (그 채널 결과는 버린다) */
void sniffStop();

}  // namespace wa

#endif
