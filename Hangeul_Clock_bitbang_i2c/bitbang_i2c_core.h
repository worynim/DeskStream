// worynim@gmail.com
/**
 * @file bitbang_i2c_core.h
 * @brief BitBang I2C의 **순수 판단 로직** (Arduino/ESP32 의존 없음)
 * @details 비트뱅 드라이버에서 버그가 나기 쉬운 지점은 GPIO를 만지는 코드가 아니라
 *          "어떤 핀을 HIGH/LOW로 둘 것인가"를 **계산**하는 부분이다. 비트 순서(MSB 먼저),
 *          ACK 극성(LOW가 ACK), 4버스 동시 전송의 열 구간 정렬 — 셋 다 값만 있고
 *          하드웨어가 없다. 그래서 이 파일로 분리해 호스트(g++) 테스트가 가능하게 한다.
 * @note [BB v1.0.0] 이 헤더는 `Arduino.h`를 포함하지 **않는다**. 호스트 테스트가
 *       그대로 include 할 수 있어야 하기 때문이다 (test/test_bitbang_core.cpp).
 */
#ifndef BITBANG_I2C_CORE_H
#define BITBANG_I2C_CORE_H

#include <stdint.h>

/** 동시 전송이 다룰 수 있는 최대 버스 수 (이 프로젝트는 4) */
#define BB_CORE_MAX_BUSES 4

/**
 * @brief 한 바이트를 8비트로 보낼 때 비트별 SDA 핀 마스크를 계산한다
 * @param bytes      버스별로 보낼 바이트 (길이 >= busCount)
 * @param sdaPins    버스별 SDA GPIO 번호 (길이 >= busCount)
 * @param busCount   전체 버스 수
 * @param activeMask 이번 트랜잭션에 참여하는 버스 마스크 (bit N = 버스 N)
 * @param ones       [출력] 비트 i에서 HIGH(릴리스)로 둘 SDA **핀** 마스크
 * @param zeros      [출력] 비트 i에서 LOW로 당길 SDA **핀** 마스크
 *
 * @details I2C는 **MSB 먼저** 보낸다. 순서를 뒤집으면 화면에 엉뚱한 바이트가 들어가는데,
 *          비트뱅에서는 "화면이 이상하다"로만 보여 원인 추적이 어렵다.
 *          이 함수가 그 규칙의 유일한 정의처다.
 * @note activeMask에 없는 버스는 ones/zeros 어디에도 들어가지 않는다 → 호출부가
 *       해당 핀을 릴리스 상태로 유지하므로, 그 화면에는 START/STOP이 생기지 않는다.
 */
static inline void bbByteBitMasks(const uint8_t* bytes, const uint8_t* sdaPins,
                                  uint8_t busCount, uint8_t activeMask,
                                  uint32_t* ones, uint32_t* zeros) {
    for (int i = 0; i < 8; i++) { ones[i] = 0; zeros[i] = 0; }

    for (uint8_t b = 0; b < busCount && b < BB_CORE_MAX_BUSES; b++) {
        if (!(activeMask & (uint8_t)(1u << b))) continue;
        const uint32_t pinBit = (uint32_t)1u << sdaPins[b];
        const uint8_t  byte   = bytes[b];
        for (int i = 0; i < 8; i++) {
            // 0x80 >> i : MSB부터. (byte << i) & 0x80 과 같다.
            if (byte & (uint8_t)(0x80u >> i)) ones[i]  |= pinBit;
            else                              zeros[i] |= pinBit;
        }
    }
}

/**
 * @brief ACK 클럭에서 읽은 GPIO 입력 레지스터로부터 "ACK한 버스" 마스크를 만든다
 * @param inReg      GPIO 입력 레지스터 값 (핀 단위)
 * @param sdaPins    버스별 SDA GPIO 번호
 * @param busCount   전체 버스 수
 * @param activeMask 이번 트랜잭션 참여 버스 마스크
 * @return ACK한 버스 마스크 (bit N = 버스 N이 SDA를 LOW로 당겼다)
 *
 * @details **극성이 반대다** — I2C는 슬레이브가 SDA를 LOW로 당겨야 ACK다.
 *          이 한 줄이 뒤집히면 모든 전송이 조용히 실패한다(또는 stuck-low를 성공으로
 *          오독한다). 그래서 순수 함수로 떼어 회귀 테스트로 고정한다.
 */
static inline uint8_t bbAckMask(uint32_t inReg, const uint8_t* sdaPins,
                                uint8_t busCount, uint8_t activeMask) {
    uint8_t ack = 0;
    for (uint8_t b = 0; b < busCount && b < BB_CORE_MAX_BUSES; b++) {
        if (!(activeMask & (uint8_t)(1u << b))) continue;
        if (!(inReg & ((uint32_t)1u << sdaPins[b]))) ack |= (uint8_t)(1u << b);
    }
    return ack;
}

/**
 * @brief 4버스 동시 전송에 쓸 **공통 열 구간**을 계산한다 (합집합)
 * @param firstTile 버스별 시작 타일 (tileCount가 0이면 무시)
 * @param tileCount 버스별 타일 수 (0 = 그 화면은 이 페이지에 변경 없음)
 * @param busCount  버스 수
 * @param outFirst  [출력] 공통 시작 타일
 * @param outCount  [출력] 공통 타일 수 (0 = 참여 버스 없음)
 *
 * @details **동시 전송의 유일한 제약**이다. SCL을 공유하므로 4개 SDA는 같은 클럭에
 *          같은 개수의 비트를 실어야 한다. 화면마다 바뀐 열 구간이 다르면 그대로는
 *          정렬이 깨지므로, 참여 화면들의 구간을 **합집합**으로 넓혀 모두 같은
 *          바이트 수를 보내게 한다. 넓어진 구간은 같은 값을 다시 쓰는 것뿐이라
 *          화면에는 아무 차이가 없다(멱등).
 */
static inline void bbUnionColumnRange(const uint8_t* firstTile, const uint8_t* tileCount,
                                      uint8_t busCount,
                                      uint8_t* outFirst, uint8_t* outCount) {
    int lo = 0x7FFF;
    int hi = -1;
    for (uint8_t b = 0; b < busCount && b < BB_CORE_MAX_BUSES; b++) {
        if (tileCount[b] == 0) continue;
        const int f = firstTile[b];
        const int l = firstTile[b] + tileCount[b] - 1;
        if (f < lo) lo = f;
        if (l > hi) hi = l;
    }
    if (hi < 0) { *outFirst = 0; *outCount = 0; return; }
    *outFirst = (uint8_t)lo;
    *outCount = (uint8_t)(hi - lo + 1);
}

#endif  // BITBANG_I2C_CORE_H
