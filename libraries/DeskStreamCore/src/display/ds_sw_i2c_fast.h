// worynim@gmail.com
/**
 * @file ds_sw_i2c_fast.h
 * @brief SW I2C 버스(화면 2·3)용 고속 U8g2 byte 콜백
 * @details U8g2 기본 `u8x8_byte_sw_i2c`는 비트 엣지마다 gpio 콜백을 호출해 CPU 한계로
 *          실효 763kHz에 묶여 있었다. 이 콜백은 레지스터를 직접 쓰고, 반주기 지연을
 *          부팅 시 보정한 NOP 루프로 맞춰 1MHz 이상을 낸다.
 *            · SCL은 매 비트 **실제 HIGH가 될 때까지** 기다린다. 풀업 RC 상승을 무시하면
 *              패널이 비트를 못 받는다.
 *            · 상승 대기는 레지스터 스핀 먼저, 그래도 LOW일 때만 micros() 타임아웃 경로.
 *          ACK는 U8g2 원본과 같이 읽지 않는다(9번째 클럭만 준다).
 * @note 출처: Hangeul_Clock/sw_i2c_fast.* (v2.10.0)
 */
#ifndef DS_SW_I2C_FAST_H
#define DS_SW_I2C_FAST_H

#include <Arduino.h>
#include <U8g2lib.h>

namespace ds {

/**
 * @brief 타이밍 설정 — U8g2 begin()(= BYTE_INIT, 이때 지연 보정) **전에** 호출한다
 * @param halfBitNs       반주기 지연 (ns). 0 = 지연 없음
 * @param sclSpinMax      SCL 상승을 레지스터로 다시 읽는 횟수 (넘으면 micros() 경로)
 * @param stretchTimeoutUs SCL이 이 시간 넘게 LOW면 트랜잭션 중단
 */
void swI2cFastConfigure(uint32_t halfBitNs, uint32_t sclSpinMax, uint32_t stretchTimeoutUs);

/** 보정된 실제 반주기 지연 (ns) — 벤치마크 보고용 */
uint32_t swI2cFastMeasuredNs();

/** SCL stuck으로 중단된 트랜잭션 수 */
uint32_t swI2cFastErrorCount();

}  // namespace ds

/** U8g2 byte_cb — u8g2.getU8x8()->byte_cb 에 끼운다. 핀은 u8x8->pins[I2C_CLOCK/DATA]를 쓴다 */
extern "C" uint8_t ds_u8x8_byte_sw_i2c_fast(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr);

#endif
