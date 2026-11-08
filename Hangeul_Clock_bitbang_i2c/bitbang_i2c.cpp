// worynim@gmail.com
/**
 * @file bitbang_i2c.cpp
 * @brief ESP32-C3 4버스 동시 BitBang I2C 드라이버 구현
 * @details 프로토콜(START/바이트/ACK/STOP)과 버스 자가 복구를 구현한다.
 *          "어떤 핀을 HIGH/LOW로 둘 것인가"의 계산은 bitbang_i2c_core.h의 순수 함수가
 *          담당한다 — 그쪽이 호스트 테스트 대상이다.
 */
#include "bitbang_i2c.h"

BitBangI2C bitBang;

/**
 * @brief 목표 반주기(ns)에 맞춰 NOP 루프 횟수를 정한다
 * @details 빈 루프 1회의 비용을 **실측**해 나눗셈 한 번으로 횟수를 구한다.
 *          CPU 주파수를 런타임에 읽으므로 주파수가 바뀌어도(DFS) 어긋나지 않는다.
 *          마지막에 실제 지연을 다시 재서 `_measuredNs`에 남긴다 — 시리얼로 보고한다.
 */
void BitBangI2C::calibrateBitDelay(uint16_t targetNs) {
    _halfBitNs = targetNs;
    _delayIters = 0;
    _measuredNs = 0;
    if (targetNs == 0) return;   // 지연 없이 최대 속도

    const uint32_t cpuHz = (uint32_t)getCpuFrequencyMhz() * 1000000UL;
    if (cpuHz == 0) return;

    const uint32_t targetCycles =
        (uint32_t)(((uint64_t)targetNs * (uint64_t)cpuHz) / 1000000000ULL);

    // 빈 루프 1회의 비용 실측 (파이프라인 워밍업 포함)
    const uint32_t iters = 256;
    const uint32_t c0 = esp_cpu_get_cycle_count();
    for (uint32_t i = 0; i < iters; i++) { __asm__ __volatile__("nop"); }
    const uint32_t c1 = esp_cpu_get_cycle_count();
    const uint32_t perIter = (c1 - c0) / iters;
    if (perIter == 0) return;    // 측정 실패 → 지연 없음

    _delayIters = (targetCycles > perIter) ? ((targetCycles / perIter) - 1u) : 0u;

    // 실제 지연을 다시 재서 보고용으로 남긴다
    const uint32_t m0 = esp_cpu_get_cycle_count();
    for (int k = 0; k < 64; k++) bitDelay();
    const uint32_t m1 = esp_cpu_get_cycle_count();
    _measuredNs = (uint32_t)(((uint64_t)((m1 - m0) / 64u) * 1000000000ULL) / (uint64_t)cpuHz);
}

void BitBangI2C::begin(const uint8_t* sdaPins, uint8_t sclPin, uint8_t busCount, uint16_t halfBitNs) {
    _busCount = (busCount > BB_CORE_MAX_BUSES) ? (uint8_t)BB_CORE_MAX_BUSES : busCount;
    _scl      = sclPin;
    calibrateBitDelay(halfBitNs);
    _sdaMask  = 0;

    for (uint8_t b = 0; b < _busCount; b++) {
        _sda[b] = sdaPins[b];
        _sdaMask |= (uint32_t)1u << _sda[b];
    }
    _sclBit = (uint32_t)1u << _scl;
    bbBuildNibbleLut(_sda, _busCount, _nib);   // [FPS 개선] 핫 경로용 비트 마스크 표

    // INPUT_OUTPUT_OD = Open-Drain 출력 + 입력 버퍼 활성.
    //   입력이 꺼지면 ACK(SDA가 LOW로 당겨졌는지)를 읽을 수 없다.
    //   내부 풀업도 켠다 — 모듈 내장 풀업과 병렬로 걸려 상승 에지를 돕는다.
    for (uint8_t b = 0; b < _busCount; b++) {
        gpio_set_direction((gpio_num_t)_sda[b], GPIO_MODE_INPUT_OUTPUT_OD);
        gpio_pullup_en((gpio_num_t)_sda[b]);
    }
    gpio_set_direction((gpio_num_t)_scl, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_pullup_en((gpio_num_t)_scl);

    // 유휴 상태: SDA/SCL 모두 릴리스(HIGH)
    sdaReleaseAll();
    sclHigh();
    bitDelay();
}

uint8_t BitBangI2C::start(uint8_t busMask) {
    const uint32_t pm = pinMask(busMask);
    sdaReleaseAll();                 // SDA를 먼저 HIGH로 (START의 전제)
    if (!sclWaitHigh()) return 0;    // SCL HIGH
    bitDelay();
    sdaDrive(0, pm);                 // SCL이 HIGH인 동안 SDA HIGH→LOW = START
    bitDelay();
    sclLow();
    bitDelay();
    return 1;
}

void BitBangI2C::stop(uint8_t busMask) {
    const uint32_t pm = pinMask(busMask);
    sdaDrive(0, pm);                 // SCL은 이미 LOW → SDA를 LOW로 준비
    bitDelay();
    (void)sclWaitHigh();             // SCL HIGH (STOP 직전까지 SDA는 LOW)
    bitDelay();
    sdaDrive(pm, 0);                 // SCL이 HIGH인 동안 SDA LOW→HIGH = STOP
    bitDelay();
}

uint8_t BitBangI2C::writeBytePerBus(const uint8_t* bytes, uint8_t busMask) {
    uint32_t ones[8];
    uint32_t zeros[8];
    bbByteBitMasks(bytes, _sda, _busCount, busMask, ones, zeros);

    for (int i = 0; i < 8; i++) {
        sdaDrive(ones[i], zeros[i]);   // 비트 준비 (MSB 먼저 — 코어가 보장)
        bitDelay();
        if (!sclWaitHigh()) return 0;  // 상승 에지에서 슬레이브가 래치
        bitDelay();
        sclLow();
    }

    // ACK 비트: SDA를 전부 놓아주고 슬레이브가 당기는지 한 번에 읽는다
    sdaReleaseAll();
    bitDelay();
    if (!sclWaitHigh()) return 0;
    _lastAck = bbAckMask(sdaReadAll(), _sda, _busCount, busMask);
    bitDelay();
    sclLow();
    bitDelay();
    return 1;
}

uint8_t BitBangI2C::writeConcurrent(uint8_t addr, const uint8_t* const* payloads,
                                    uint16_t length, uint8_t busMask) {
    // 통계용 래퍼 — 성공한 트랜잭션만 센다 (실패분은 비트 수가 확정되지 않는다)
    const uint32_t t0 = micros();
    const uint8_t ok = writeConcurrentImpl(addr, payloads, length, busMask);
    if (ok) {
        _statUs   += (uint32_t)(micros() - t0);
        _statBits += 2u + 9u * (1u + (uint32_t)length);   // START + (주소+페이로드)×9 + STOP
        _statTx++;
    }
    return ok;
}

/**
 * @brief [FPS 개선] 핫 경로 — 니블 표로 4버스에 바이트 1개를 싣고 ACK를 읽는다
 * @param bytes 버스별 바이트 (비참여 버스는 0이어야 한다 — 표의 니블 0 행이 0이라 무해)
 * @param pm    참여 버스의 SDA 핀 마스크
 * @return 1 = 전 참여 버스 ACK, 0 = NACK 또는 SCL 타임아웃
 * @details 예전 경로(writeBytePerBus)와 전이 순서는 같다. 바뀐 것은
 *          ① 비트 마스크를 이중 루프 대신 표 OR 세 번으로 구하고
 *          ② SCL 상승 대기가 micros() 대신 레지스터 스핀으로 끝나고 (sclWaitHigh)
 *          ③ ACK를 버스별 루프 대신 `GPIO.in & pm == 0` 한 번으로 판정한다.
 */
inline uint8_t BitBangI2C::sendByteLut(const uint8_t* bytes, uint32_t pm) {
#pragma GCC unroll 8
    for (int i = 0; i < 8; i++) {
        const uint32_t ones = bbOnesFromLut(_nib, bytes, i);
        sdaDrive(ones, pm & ~ones);    // 비트 준비 (SCL LOW 구간)
        bitDelay();
        // SCL이 실제로 HIGH가 될 때까지 기다린다. 확인을 생략하면(놓기만 하면) 풀업 RC 때문에
        //   SCL이 HIGH에 닿기 전에 다시 내려가 패널이 클럭을 못 본다 — 실기에서 전혀 동작하지
        //   않음을 확인했다 (예전 BB_DATA_SCL_CHECK 0 옵션, 그래서 제거).
        if (!sclWaitHigh()) return 0;  // 상승 에지에서 슬레이브가 래치
        bitDelay();
        sclLow();
    }
    // ACK: SDA를 놓고, 참여 버스의 SDA가 **전부** LOW면 전원 ACK
    sdaReleaseAll();
    bitDelay();
    if (!sclWaitHigh()) return 0;      // ACK 비트는 항상 확인 — stuck 감지 유지
    const uint32_t in = GPIO.in.val;
    bitDelay();
    sclLow();
    bitDelay();
    return ((in & pm) == 0) ? 1 : 0;
}

/**
 * @note [FPS 개선] IRAM에 두고 -O2로 컴파일한다. 프로젝트 기본은 -Os(크기 우선)라 핫 루프가
 *       펼쳐지지 않았고, 플래시 실행은 캐시 미스(특히 WiFi 동작 중)에 취약하다.
 */
__attribute__((optimize("O2")))
uint8_t IRAM_ATTR BitBangI2C::writeConcurrentImpl(uint8_t addr, const uint8_t* const* payloads,
                                                  uint16_t length, uint8_t busMask) {
    if (_busCount == 0) return 0;
    busMask &= (uint8_t)((1u << _busCount) - 1u);
    if (busMask == 0) return 0;
    const uint32_t pm = pinMask(busMask);

    // 비참여 버스는 nullptr로 만들어 0을 싣게 한다
    const uint8_t* p[BB_CORE_MAX_BUSES];
    for (uint8_t b = 0; b < BB_CORE_MAX_BUSES; b++) {
        p[b] = (b < _busCount && (busMask & (1u << b))) ? payloads[b] : nullptr;
    }

    if (!start(busMask)) { _errors++; return 0; }

    // 1. 주소 바이트 (참여 버스 공통, R/W=0 쓰기)
    uint8_t bytes[BB_CORE_MAX_BUSES];
    const uint8_t addrByte = (uint8_t)(addr << 1);
    for (uint8_t b = 0; b < BB_CORE_MAX_BUSES; b++) bytes[b] = (busMask & (1u << b)) ? addrByte : 0;
    if (!sendByteLut(bytes, pm)) {
        stop(busMask);
        _errors++;
        return 0;
    }

    // 2. 페이로드 — 버스마다 다른 값을 같은 클럭에 싣는다
    for (uint16_t i = 0; i < length; i++) {
        bytes[0] = p[0] ? p[0][i] : 0;
        bytes[1] = p[1] ? p[1][i] : 0;
        bytes[2] = p[2] ? p[2][i] : 0;
        bytes[3] = p[3] ? p[3][i] : 0;
        if (!sendByteLut(bytes, pm)) {
            stop(busMask);
            _errors++;
            return 0;
        }
    }

    stop(busMask);
    return 1;
}

uint8_t BitBangI2C::probe(uint8_t bus, uint8_t addr) {
    if (bus >= _busCount) return 0;
    const uint8_t mask = (uint8_t)(1u << bus);

    uint8_t bytes[BB_CORE_MAX_BUSES];
    const uint8_t addrByte = (uint8_t)(addr << 1);
    for (uint8_t b = 0; b < _busCount; b++) bytes[b] = addrByte;

    if (!start(mask)) return 0;
    const uint8_t ok = writeBytePerBus(bytes, mask);
    stop(mask);
    return (ok && (_lastAck & mask)) ? 1 : 0;
}

void BitBangI2C::recover() {
    // 1. SDA를 놓고 SCL을 10번 펄스 — 데이터를 붙잡고 있는 슬레이브가 남은 비트를 밀어낸다
    sdaReleaseAll();
    sclHigh();
    bitDelay();
    for (int i = 0; i < 10; i++) {
        sclLow();
        bitDelay();
        sclHigh();
        bitDelay();
    }
    // 2. STOP 조건을 만들어 버스를 유휴 상태로 되돌린다
    sclLow();
    bitDelay();
    sdaDrive(0, _sdaMask);
    bitDelay();
    sclHigh();
    bitDelay();
    sdaReleaseAll();
    bitDelay();
}
