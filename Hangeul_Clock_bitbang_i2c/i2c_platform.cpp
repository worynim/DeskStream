// worynim@gmail.com
/**
 * @file i2c_platform.cpp
 * @brief 4버스 BitBang I2C 전송 계층 구현 — 캡처 후 재생
 * @details 명령 바이트는 **U8g2가 만든 것을 그대로** 쓰고(캡처), 전송만 4버스에
 *          동시에 한다(재생). 자세한 배경은 i2c_platform.h의 이력 참조.
 */
#include "i2c_platform.h"
#include "Multi_BitBang.h"

I2CPlatform i2cPlatform;

/** 화면 인덱스 = 버스 번호. config.h의 BB_SDA_PINS를 그대로 쓴다. */
static const uint8_t BB_SDA_PINS_ARR[NUM_SCREENS] = BB_SDA_PINS;

I2CPlatform::I2CPlatform() {
    memset(_shadow_buffer, 0xFF, sizeof(_shadow_buffer));
    memset(_packet_ptr, 0, sizeof(_packet_ptr));
    memset(_cap_len, 0, sizeof(_cap_len));
    memset(_cap_tx_cnt, 0, sizeof(_cap_tx_cnt));
}

/**
 * @brief 부팅 시 내장 Multi_BitBang으로 4버스에 0x3C가 응답하는지 확인한다
 * @details 브링업에서 **가장 먼저** 알아야 하는 정보가 "배선이 살아 있는가"다.
 *          화면이 안 나올 때 코드를 의심할지 배선을 의심할지 여기서 갈린다.
 * @note ⚠ 이 함수는 반드시 bitBang.begin() **전에** 끝나야 한다.
 *       원본 라이브러리는 pinMode(INPUT/OUTPUT)로 핀을 잡으므로, 나중에 부르면
 *       우리가 설정한 Open-Drain + 입력활성 설정을 덮어쓴다.
 */
static void scanBusesForDiagnostics() {
    static uint8_t sda[NUM_SCREENS];
    static uint8_t scl[NUM_SCREENS];
    static int32_t clk[NUM_SCREENS];
    for (uint8_t i = 0; i < NUM_SCREENS; i++) {
        sda[i] = BB_SDA_PINS_ARR[i];
        scl[i] = BB_SCL_PIN;
        clk[i] = BB_I2C_SPEED_HZ;
    }

    Multi_I2CInit(sda, scl, clk, NUM_SCREENS);

    Serial.println("[I2C] BitBang bus scan (Multi_BitBang)");
    for (uint8_t b = 0; b < NUM_SCREENS; b++) {
        uint8_t map[16];
        Multi_I2CScan(b, map);
        const bool found = (map[OLED_I2C_ADDR >> 3] & (1u << (OLED_I2C_ADDR & 7))) != 0;
        Serial.printf("[I2C] 화면%u (SDA %u): 0x%02X %s\n",
                      (unsigned)b, (unsigned)BB_SDA_PINS_ARR[b],
                      (unsigned)OLED_I2C_ADDR, found ? "OK" : "NOT FOUND");
    }
}

void I2CPlatform::begin() {
    // 1. 배선 진단 (순차 — 부팅 1회뿐이라 속도는 문제가 되지 않는다)
    scanBusesForDiagnostics();

    // 2. 동시 전송 드라이버 초기화 — 이 시점부터 핀 설정의 최종 권한을 가진다
    bitBang.begin(BB_SDA_PINS_ARR, BB_SCL_PIN, NUM_SCREENS, BB_HALF_BIT_NS);

    // 클럭 설정을 보고한다 — "왜 느린가"를 추측하지 않기 위한 계측값이다.
    const uint32_t measured = bitBang.measuredHalfBitNs();
    if (BB_HALF_BIT_NS == 0) {
        Serial.println("[I2C] 반주기 지연: 없음 (최대 속도 — SCL 상승을 기다려 자연 페이싱)");
    } else {
        Serial.printf("[I2C] 반주기 지연: 목표 %uns / 실측 %uns -> 약 %u kHz\n",
                      (unsigned)BB_HALF_BIT_NS, (unsigned)measured,
                      (unsigned)(measured ? (500000UL / measured) : 0));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 셰도 버퍼 · dirty 장부
// ─────────────────────────────────────────────────────────────────────────────

void I2CPlatform::setShadowData(uint8_t screenIdx, int offset, uint8_t data) {
    if (screenIdx < NUM_SCREENS && offset < (SCREEN_WIDTH * PAGES_PER_SCREEN)) {
        _shadow_buffer[screenIdx][offset] = data;
    }
}

uint8_t I2CPlatform::getShadowData(uint8_t screenIdx, int offset) const {
    if (screenIdx < NUM_SCREENS && offset < (SCREEN_WIDTH * PAGES_PER_SCREEN)) {
        return _shadow_buffer[screenIdx][offset];
    }
    return 0;
}

void I2CPlatform::invalidateShadow(uint8_t screenIdx) {
    if (screenIdx < NUM_SCREENS) {
        // 0xEE는 0x00(Clear)이나 0xFF(Initial)와 다른 특수 값으로,
        // 다음 diff가 모든 타일을 변경으로 잡게 한다.
        memset(_shadow_buffer[screenIdx], 0xEE, sizeof(_shadow_buffer[screenIdx]));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 캡처
// ─────────────────────────────────────────────────────────────────────────────

void I2CPlatform::captureBegin() {
    for (uint8_t s = 0; s < NUM_SCREENS; s++) {
        _cap_len[s] = 0;
        _cap_tx_cnt[s] = 0;
        _cap_cur[s] = 0;
    }
    _cap_overflow = false;
    for (uint8_t s = 0; s < NUM_SCREENS; s++) _cap_in_tx[s] = false;
    _capturing = true;
}

/**
 * @brief START~END 사이에 담긴 바이트를 한 트랜잭션으로 확정한다
 * @details `_cap_cur[bus]`가 이번 트랜잭션이 누적한 길이,
 *          `_cap_len[bus]`가 전체 스트림 길이다. 시작 오프셋 = 전체 - 이번 길이.
 */
void I2CPlatform::captureCloseTransaction(uint8_t busIdx) {
    const uint16_t cur = _cap_cur[busIdx];
    _cap_cur[busIdx] = 0;
    if (cur == 0) return;   // 담긴 바이트가 없는 START/END 쌍은 트랜잭션이 아니다

    if (_cap_tx_cnt[busIdx] >= BB_CAP_TX_MAX) { _cap_overflow = true; return; }
    const uint8_t idx = _cap_tx_cnt[busIdx];
    _cap_tx_off[busIdx][idx] = (uint16_t)(_cap_len[busIdx] - cur);
    _cap_tx_len[busIdx][idx] = cur;
    _cap_tx_cnt[busIdx] = (uint8_t)(idx + 1);
}

uint8_t I2CPlatform::handleByteCb(uint8_t busIdx, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    if (busIdx >= NUM_SCREENS) return 0;

    if (_capturing) {
        switch (msg) {
            case U8X8_MSG_BYTE_INIT:
            case U8X8_MSG_BYTE_SET_DC:      // I2C에서는 무시된다
                break;
            case U8X8_MSG_BYTE_START_TRANSFER:
                if (_cap_in_tx[busIdx]) captureCloseTransaction(busIdx);  // START 겹침 방어
                _cap_in_tx[busIdx] = true;
                break;
            case U8X8_MSG_BYTE_SEND:
                if ((uint32_t)_cap_len[busIdx] + arg_int > BB_CAP_BUF_SIZE) {
                    _cap_overflow = true;
                    break;
                }
                memcpy(&_cap_buf[busIdx][_cap_len[busIdx]], arg_ptr, arg_int);
                _cap_len[busIdx]       = (uint16_t)(_cap_len[busIdx] + arg_int);
                _cap_cur[busIdx]       = (uint16_t)(_cap_cur[busIdx] + arg_int);
                break;
            case U8X8_MSG_BYTE_END_TRANSFER:
                captureCloseTransaction(busIdx);
                _cap_in_tx[busIdx] = false;
                break;
            default:
                return 0;
        }
        return 1;
    }

    // ── 즉시 전송 경로 (초기화 시퀀스) ──
    switch (msg) {
        case U8X8_MSG_BYTE_INIT:
        case U8X8_MSG_BYTE_SET_DC:
            break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            _packet_ptr[busIdx] = 0;
            break;
        case U8X8_MSG_BYTE_SEND:
            if ((uint32_t)_packet_ptr[busIdx] + arg_int > I2C_PACKET_BUF_SIZE) {
                // 절반만 나간 명령 시퀀스는 화면을 알 수 없는 상태로 만든다 → 버린다
                _packet_ptr[busIdx] = 0;
                return 0;
            }
            memcpy(&_packet_buf[busIdx][_packet_ptr[busIdx]], arg_ptr, arg_int);
            _packet_ptr[busIdx] = (uint16_t)(_packet_ptr[busIdx] + arg_int);
            break;
        case U8X8_MSG_BYTE_END_TRANSFER:
            return flushPacket(busIdx);
        default:
            return 0;
    }
    return 1;
}

/** @brief 누적된 패킷을 해당 버스로 즉시 내보낸다 (초기화 시퀀스 전용) */
uint8_t I2CPlatform::flushPacket(uint8_t busIdx) {
    const uint16_t len = _packet_ptr[busIdx];
    _packet_ptr[busIdx] = 0;
    if (len == 0) return 1;

    const uint8_t *payloads[NUM_SCREENS];
    for (uint8_t s = 0; s < NUM_SCREENS; s++) payloads[s] = nullptr;
    payloads[busIdx] = _packet_buf[busIdx];

    const uint8_t ok = bitBang.writeConcurrent(OLED_I2C_ADDR, payloads, len,
                                               (uint8_t)(1u << busIdx));
    if (!ok) _error_count++;
    return ok ? 1 : 0;
}

/**
 * @brief 캡처한 트랜잭션들을 참여 버스에 동시 재생한다
 * @details 트랜잭션 **번호끼리** 짝지어 한 번에 쏜다. SCL을 공유하므로 참여 화면은
 *          같은 개수·같은 길이의 트랜잭션을 가져야 한다 — 다르면 정렬이 깨지므로
 *          재생하지 않고 0을 돌려준다(호출부가 dirty를 유지해 다음 회차에 재시도).
 */
uint8_t I2CPlatform::replayCaptured(uint8_t busMask) {
    busMask &= (uint8_t)((1u << NUM_SCREENS) - 1u);
    if (busMask == 0) return 0;
    if (_cap_overflow) { _error_count++; return 0; }

    // 참여 화면의 트랜잭션 수가 모두 같아야 한다
    uint8_t txCount = 0;
    bool first = true;
    for (uint8_t s = 0; s < NUM_SCREENS; s++) {
        if (!(busMask & (uint8_t)(1u << s))) continue;
        if (first) { txCount = _cap_tx_cnt[s]; first = false; }
        else if (_cap_tx_cnt[s] != txCount) { _error_count++; return 0; }
    }
    if (txCount == 0) return 0;

    for (uint8_t t = 0; t < txCount; t++) {
        // 길이도 같아야 한다 (SCL을 공유하므로 비트 수가 어긋나면 안 된다)
        uint16_t len = 0;
        bool lenFirst = true;
        bool lenOk = true;
        for (uint8_t s = 0; s < NUM_SCREENS; s++) {
            if (!(busMask & (uint8_t)(1u << s))) continue;
            if (lenFirst) { len = _cap_tx_len[s][t]; lenFirst = false; }
            else if (_cap_tx_len[s][t] != len) { lenOk = false; }
        }
        if (!lenOk || len == 0) { _error_count++; return 0; }

        const uint8_t *payloads[NUM_SCREENS];
        for (uint8_t s = 0; s < NUM_SCREENS; s++) {
            payloads[s] = (busMask & (uint8_t)(1u << s))
                        ? &_cap_buf[s][_cap_tx_off[s][t]] : nullptr;
        }

        if (!bitBang.writeConcurrent(OLED_I2C_ADDR, payloads, len, busMask)) {
            _error_count++;
            return 0;
        }
    }
    return busMask;
}

void I2CPlatform::recoverBus() {
    Serial.println("[I2C] BitBang bus recovery (clock pulsing)...");
    bitBang.recover();
    _error_count = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// U8g2 콜백 래퍼
// ─────────────────────────────────────────────────────────────────────────────

extern "C" uint8_t u8x8_byte_bb_0(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    (void)u8x8;
    return i2cPlatform.handleByteCb(0, msg, arg_int, arg_ptr);
}
extern "C" uint8_t u8x8_byte_bb_1(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    (void)u8x8;
    return i2cPlatform.handleByteCb(1, msg, arg_int, arg_ptr);
}
extern "C" uint8_t u8x8_byte_bb_2(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    (void)u8x8;
    return i2cPlatform.handleByteCb(2, msg, arg_int, arg_ptr);
}
extern "C" uint8_t u8x8_byte_bb_3(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    (void)u8x8;
    return i2cPlatform.handleByteCb(3, msg, arg_int, arg_ptr);
}
