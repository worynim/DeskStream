// worynim@gmail.com
/**
 * @file i2c_platform.h
 * @brief 4× 1.3" OLED를 위한 BitBang I2C 전송 계층
 *
 * @details 원본 Hangeul_Clock의 "HW I2C 2화면 + SW I2C 2화면" 이원화를
 *          **4버스 BitBang 단일 경로**로 교체했다. 1.3" 모듈은 주소를 바꿀 수 없어
 *          (전부 0x3C) 버스를 4개 만들어야 한다.
 *
 * @details ## 전송 방식 — 캡처 후 재생
 *
 *          **명령 바이트를 우리가 만들지 않는다.** 그게 이 구조의 핵심이다.
 *
 *          ① 각 화면의 `updateDisplayArea()`를 **캡처 모드**로 호출하면, byte_cb가
 *             전송하는 대신 화면별 버퍼에 **트랜잭션 단위로** 담는다.
 *             (명령 순서·열 오프셋·제어바이트 규칙이 전부 U8g2 책임이 된다)
 *          ② 캡처분을 트랜잭션 번호끼리 짝지어 **4버스에 동시 전송**한다.
 *             SCL을 공유하므로 참여 화면은 같은 트랜잭션 구조·같은 길이여야 한다.
 *
 *          ## 이력 — 왜 이 형태가 되었나
 *
 *          | 버전 | 방식 | 결과 |
 *          |:--|:--|:--|
 *          | v1.0.0 | 명령+데이터를 한 트랜잭션에 | ❌ 제어바이트 Co 비트 규칙 위반 → 노이즈 |
 *          | v1.0.1 | 트랜잭션 분리 (손 페이로드) | ❌ 화면 깨짐 |
 *          | v1.0.3 | U8g2 CAD에 전송 위임 (순차) | ✅ 정상, 🐢 느림 |
 *          | v1.0.4 | 캡처·재생 + dirty 장부 경유 | ❌ 화면 깨짐 |
 *          | **v1.0.9** | **캡처·재생, 장부 없이** | ✅ 정상, 원본과 동급 속도 |
 *
 *          **교훈**: 바이트를 만들지 말고(②가 아니라 ①), 검증된 코드를 한 번에
 *          하나씩만 바꾼다. 손으로 만든 페이로드와 dirty 장부는 그래서 사라졌다.
 */
#ifndef I2C_PLATFORM_H
#define I2C_PLATFORM_H

#include <Arduino.h>
#include <U8g2lib.h>     // u8x8_t — 아래 byte_cb 선언에 필요
#include "config.h"
#include "bitbang_i2c.h"

/** 캡처 버퍼 크기 — 한 페이지(명령 트랜잭션 2개 + 데이터 ≤24B 청크 6개)에 충분하다 */
#define BB_CAP_BUF_SIZE 256
/** 한 페이지에서 캡처할 최대 트랜잭션 수 */
#define BB_CAP_TX_MAX 16

/**
 * @brief 4버스 BitBang 전송 및 셰도 버퍼 관리
 */
class I2CPlatform {
public:
    I2CPlatform();

    void begin();

    // ── 셰도 버퍼 (diff 기준) ───────────────────────────────────────────
    void setShadowData(uint8_t screenIdx, int offset, uint8_t data);
    uint8_t getShadowData(uint8_t screenIdx, int offset) const;
    void invalidateShadow(uint8_t screenIdx);

    // ── 캡처 후 재생 ────────────────────────────────────────────────────
    /**
     * @brief 캡처 모드 시작 — 이후 byte_cb는 전송하지 않고 버퍼에 담는다
     * @note 반드시 `captureEnd()`와 짝을 맞춘다.
     */
    void captureBegin();

    /** @brief 캡처 모드 종료 */
    void captureEnd() {
        for (uint8_t s = 0; s < NUM_SCREENS; s++) {
            if (_cap_in_tx[s]) captureCloseTransaction(s);   // END 없이 끝난 경우 방어
        }
        _capturing = false;
    }

    /**
     * @brief 캡처한 트랜잭션을 참여 버스들에 **동시에** 재생한다
     * @param busMask 참여 버스 마스크
     * @return 성공한 버스 마스크 (0 = 전부 실패)
     * @details 트랜잭션 구조가 화면마다 다르면 동시 재생이 불가능하므로 0을 돌려준다.
     *          호출부는 이 경우 dirty를 유지해 다음 회차에 다시 시도한다.
     */
    uint8_t replayCaptured(uint8_t busMask);

    /** @brief U8g2 byte_cb 본체 (화면 인덱스 = 버스 번호) */
    uint8_t handleByteCb(uint8_t busIdx, uint8_t msg, uint8_t arg_int, void *arg_ptr);

    /** @brief 버스 자가 복구 — SCL 펄스로 붙잡힌 SDA를 풀어준다 */
    void recoverBus();

    /** @brief 아직 전송되지 않은 패킷을 즉시 내보낸다 (초기화 시퀀스용) */
    uint8_t flushPacket(uint8_t busIdx);

    uint32_t errorCount() const { return _error_count; }

private:
    /**
     * 셰도 버퍼 — "화면에 실제로 들어가 있는 값"의 사본.
     * 페이지 변경 구간을 찾는 diff의 기준이다.
     */
    uint8_t _shadow_buffer[NUM_SCREENS][SCREEN_WIDTH * PAGES_PER_SCREEN];

    /** U8g2가 START_TRANSFER~END_TRANSFER 사이에 누적하는 패킷 (즉시 전송 경로용) */
    uint8_t  _packet_buf[NUM_SCREENS][I2C_PACKET_BUF_SIZE];
    uint16_t _packet_ptr[NUM_SCREENS];

    // ── 캡처 상태 ───────────────────────────────────────────────────────
    bool     _capturing = false;
    /** 화면별로 캡처한 바이트 스트림 (트랜잭션들이 이어 붙어 있다) */
    uint8_t  _cap_buf[NUM_SCREENS][BB_CAP_BUF_SIZE];
    uint16_t _cap_len[NUM_SCREENS];
    /** 화면별 트랜잭션 목록 — _cap_buf 안의 (시작, 길이) */
    uint16_t _cap_tx_off[NUM_SCREENS][BB_CAP_TX_MAX];
    uint16_t _cap_tx_len[NUM_SCREENS][BB_CAP_TX_MAX];
    uint8_t  _cap_tx_cnt[NUM_SCREENS];
    /** 지금 열려 있는(START~END) 트랜잭션이 누적한 길이 */
    uint16_t _cap_cur[NUM_SCREENS];
    bool     _cap_overflow = false;
    /** 캡처 구간에서 열려 있는 트랜잭션이 있는가 (START~END 짝) */
    /** 버스별로 START~END 짝이 열려 있는가 */
    bool     _cap_in_tx[NUM_SCREENS] = { false, false, false, false };

    uint32_t _error_count = 0;

    /** 열려 있는 트랜잭션을 확정해 목록에 추가한다 (START~END 짝) */
    void captureCloseTransaction(uint8_t busIdx);
};

extern I2CPlatform i2cPlatform;

/**
 * U8g2 전용 하위 레벨 콜백.
 * @note U8g2는 "몇 번 버스인지"를 콜백에 넘겨주지 않으므로 화면 수만큼 따로 등록한다.
 */
extern "C" uint8_t u8x8_byte_bb_0(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
extern "C" uint8_t u8x8_byte_bb_1(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
extern "C" uint8_t u8x8_byte_bb_2(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
extern "C" uint8_t u8x8_byte_bb_3(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

#endif  // I2C_PLATFORM_H
