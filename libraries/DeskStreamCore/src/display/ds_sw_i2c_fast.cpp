// worynim@gmail.com
/**
 * @file ds_sw_i2c_fast.cpp
 * @brief SW I2C 고속 byte 콜백 구현 (ds_sw_i2c_fast.h 참조)
 * @note 출처: Hangeul_Clock/sw_i2c_fast.cpp (v2.10.0). config.h 매크로 → swI2cFastConfigure()
 */
#include "ds_sw_i2c_fast.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include "esp_cpu.h"

// 두 화면(0x3C/0x3D)이 같은 SDA/SCL을 쓰므로 상태는 하나면 된다.
static uint32_t s_sclBit = 0, s_sdaBit = 0;
static uint32_t s_delayIters = 0;
static uint32_t s_measuredNs = 0;
static bool     s_ready = false;
static bool     s_abort = false;     // 현재 트랜잭션에서 SCL stuck 발생 → END까지 건너뛴다
static uint32_t s_errors = 0;

// 타이밍 설정 (swI2cFastConfigure로 바꾼다 — 기본값은 Hangeul_Clock v2.10.0 채택값)
static uint32_t s_halfBitNs = 100;
static uint32_t s_sclSpinMax = 32;
static uint32_t s_stretchTimeoutUs = 100;

namespace ds {
void swI2cFastConfigure(uint32_t halfBitNs, uint32_t sclSpinMax, uint32_t stretchTimeoutUs) {
    s_halfBitNs = halfBitNs;
    s_sclSpinMax = sclSpinMax;
    s_stretchTimeoutUs = stretchTimeoutUs;
}
uint32_t swI2cFastMeasuredNs() { return s_measuredNs; }
uint32_t swI2cFastErrorCount() { return s_errors; }
}  // namespace ds

static inline void bitDelay() {
    for (uint32_t i = 0; i < s_delayIters; i++) { __asm__ __volatile__("nop"); }
}
static inline void sclLow()  { GPIO.out_w1tc.val = s_sclBit; }
static inline void sdaLow()  { GPIO.out_w1tc.val = s_sdaBit; }
static inline void sdaHigh() { GPIO.out_w1ts.val = s_sdaBit; }   // Open-Drain: 릴리스

/** SCL을 놓고 실제 HIGH까지 기다린다. 0 = 타임아웃(stuck) */
static inline uint8_t sclWaitHigh() {
    GPIO.out_w1ts.val = s_sclBit;
    for (uint32_t n = 0; n < s_sclSpinMax; n++) {
        if (GPIO.in.val & s_sclBit) return 1;
    }
    const uint32_t t0 = micros();
    while (!(GPIO.in.val & s_sclBit)) {
        if ((uint32_t)(micros() - t0) > s_stretchTimeoutUs) return 0;
    }
    return 1;
}

/** 반주기 NOP 횟수를 CPU 사이클로 보정한다 (delayMicroseconds는 오버헤드가 커서 못 쓴다) */
static void calibrate(uint32_t targetNs) {
    s_delayIters = 0;
    s_measuredNs = 0;
    const uint32_t cpuHz = (uint32_t)getCpuFrequencyMhz() * 1000000UL;
    if (targetNs == 0 || cpuHz == 0) return;
    const uint32_t targetCycles = (uint32_t)(((uint64_t)targetNs * cpuHz) / 1000000000ULL);

    const uint32_t iters = 256;
    const uint32_t c0 = esp_cpu_get_cycle_count();
    for (uint32_t i = 0; i < iters; i++) { __asm__ __volatile__("nop"); }
    const uint32_t perIter = (esp_cpu_get_cycle_count() - c0) / iters;
    if (perIter == 0) return;
    s_delayIters = (targetCycles > perIter) ? ((targetCycles / perIter) - 1u) : 0u;

    const uint32_t m0 = esp_cpu_get_cycle_count();
    for (int k = 0; k < 64; k++) bitDelay();
    s_measuredNs = (uint32_t)(((uint64_t)((esp_cpu_get_cycle_count() - m0) / 64u) * 1000000000ULL) / cpuHz);
}

static void initPins(uint8_t scl, uint8_t sda) {
    s_sclBit = 1u << scl;
    s_sdaBit = 1u << sda;
    // INPUT_OUTPUT_OD: Open-Drain 출력 + 입력 버퍼 (SCL 상승을 읽어야 한다)
    gpio_set_direction((gpio_num_t)scl, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_direction((gpio_num_t)sda, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_pullup_en((gpio_num_t)scl);
    gpio_pullup_en((gpio_num_t)sda);
    sdaHigh();
    GPIO.out_w1ts.val = s_sclBit;
    if (!s_ready) { calibrate(s_halfBitNs); s_ready = true; }
}

static void IRAM_ATTR i2cStart() {
    sdaHigh();
    if (!sclWaitHigh()) { s_abort = true; return; }
    bitDelay();
    sdaLow();                 // SCL HIGH 동안 SDA HIGH→LOW = START
    bitDelay();
    sclLow();
}

static void IRAM_ATTR i2cStop() {
    sdaLow();
    bitDelay();
    (void)sclWaitHigh();
    bitDelay();
    sdaHigh();                // SCL HIGH 동안 SDA LOW→HIGH = STOP
    bitDelay();
}

/** 1바이트 + ACK 클럭. 실패(SCL stuck) 시 0 */
static inline uint8_t sendByte(uint8_t b) {
#pragma GCC unroll 8
    for (int i = 0; i < 8; i++) {
        if (b & 0x80) sdaHigh(); else sdaLow();
        b <<= 1;
        bitDelay();
        if (!sclWaitHigh()) return 0;
        bitDelay();
        sclLow();
    }
    sdaHigh();                // ACK: SDA 릴리스 (U8g2 원본처럼 읽지는 않는다)
    bitDelay();
    if (!sclWaitHigh()) return 0;
    bitDelay();
    sclLow();
    return 1;
}

__attribute__((optimize("O2"))) static void IRAM_ATTR sendBytes(const uint8_t* p, uint8_t n) {
    while (n--) {
        if (!sendByte(*p++)) { s_abort = true; s_errors++; return; }
    }
}

extern "C" uint8_t ds_u8x8_byte_sw_i2c_fast(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    switch (msg) {
        case U8X8_MSG_BYTE_INIT:
            initPins(u8x8->pins[U8X8_PIN_I2C_CLOCK], u8x8->pins[U8X8_PIN_I2C_DATA]);
            break;
        case U8X8_MSG_BYTE_START_TRANSFER: {
            s_abort = false;
            i2cStart();
            if (s_abort) { s_errors++; break; }
            const uint8_t addr = u8x8_GetI2CAddress(u8x8);   // 이미 8비트(쓰기) 형태
            sendBytes(&addr, 1);
            break;
        }
        case U8X8_MSG_BYTE_SEND:
            if (!s_abort) sendBytes((const uint8_t*)arg_ptr, arg_int);
            break;
        case U8X8_MSG_BYTE_END_TRANSFER:
            i2cStop();
            break;
        case U8X8_MSG_BYTE_SET_DC:
        default:
            break;
    }
    return 1;
}
