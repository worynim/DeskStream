// worynim@gmail.com
/**
 * @file agent_ble.h
 * @brief BLE UART (Nordic UART Service UUID) — 데몬과 USB 시리얼 대신 무선으로 같은 JSON Lines 를 주고받는다
 * @details NimBLE-Arduino 2.x. RX(쓰기) 바이트는 NimBLE 태스크에서 스트림 버퍼에 넣고 loop() 가 꺼내 읽는다.
 *          TX 는 20바이트씩 notify (데몬이 줄바꿈으로 다시 잇는다). 연결이 끊기면 다시 광고한다.
 *          BLE 링크 자체는 암호화하지 않는다 — 버튼·승인 신호는 agent_auth 서명으로 보호한다.
 */
#ifndef AGENT_BLE_H
#define AGENT_BLE_H

#include <stddef.h>

namespace am {

/** BLE 서버 시작 + 광고 (장치 이름 name) */
void bleBegin(const char* name);

/** 데몬(central)이 연결돼 있는지 */
bool bleConnected();

/** 받은 바이트를 최대 cap 만큼 꺼낸다 (비차단). 꺼낸 바이트 수 */
size_t bleRead(char* buf, size_t cap);

/** 한 줄(줄바꿈 포함 안 함)을 보낸다 — 연결이 없으면 무시 */
void bleWriteLine(const char* line);

}  // namespace am

#endif
