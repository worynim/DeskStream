// worynim@gmail.com
/**
 * @file agent_auth.h
 * @brief BLE 로 보내는 버튼·승인 신호의 서명 — USB 로 등록한 비밀키 + 연결마다 바뀌는 nonce + 증가 카운터
 * @details 위협: 같은 이름으로 광고하는 가짜 장치에 데몬이 연결해 "승인"을 받는 것.
 *          - 키(32바이트)는 데몬이 만들어 **USB 로만** 보낸다 ({"t":"setkey"}) → NVS 저장.
 *          - BLE 연결마다 데몬이 nonce 를 보낸다 ({"t":"nonce"}) → 카운터를 1부터 다시 센다.
 *          - 장치가 보내는 줄: 본문에 "c"(카운터)를 넣고 끝에 "sig" = HMAC-SHA256(키, nonce + 본문) 앞 16바이트(hex 32자).
 *            데몬(bridge_py/security.py)은 BLE 로 온 decide·btn 을 서명·카운터가 맞을 때만 받는다.
 *          - kid = HMAC(키, "kid") 앞 4바이트 hex — hello 에 실어 데몬이 키 일치 여부를 안다.
 */
#ifndef AGENT_AUTH_H
#define AGENT_AUTH_H

#include <stddef.h>
#include <stdint.h>

namespace am {

/** NVS 에서 키를 읽는다 (setup 에서 1회) */
void authBegin();

/** 키가 등록돼 있는지 */
bool authHasKey();

/** 키 id (hex 8자, 키 없으면 빈 문자열) */
const char* authKid();

/** 64자 hex 키를 저장한다 (USB 로 받은 setkey 에서만 호출). 형식이 틀리면 false */
bool authSetKey(const char* hex64);

/** BLE 연결의 nonce 를 정한다 — 카운터를 0 으로 되돌린다 */
void authSetNonce(const char* nonce);

/**
 * @brief 본문 JSON(마지막 '}' 로 끝남, "c" 자리는 비워 둠)에 카운터와 서명을 붙여 out 에 쓴다
 * @param body 예: {"t":"decide","id":7,"act":"allow"}
 * @return 쓴 줄 (키·nonce 가 없으면 카운터만 붙이고 서명 없음)
 */
const char* authSignLine(const char* body, char* out, size_t cap);

}  // namespace am

#endif
