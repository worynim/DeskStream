// worynim@gmail.com
/**
 * @file bitbang_i2c.h
 * @brief ESP32-C3 전용 **4버스 동시(lock-step) BitBang I2C** 드라이버
 * @details SCL 1개를 공유하고 SDA만 화면별로 분리한 구성에서, 4개 화면에
 *          **서로 다른 바이트를 같은 클럭에 동시에** 실어 보낸다.
 *
 *          왜 되는가: I2C 슬레이브는 SCL 상승 에지에서 **자기 SDA만** 래치한다.
 *          따라서 SDA_N에 각자 다른 비트를 실어도 4개가 동시에 자기 바이트를 받는다.
 *          ACK도 슬레이브마다 자기 SDA를 당기므로 한 번의 클럭으로 4개를 모두 읽는다.
 *
 *          왜 필요한가: ESP32-C3는 단일 코어라 "백그라운드에서 순차 전송"이 실제로는
 *          메인 루프를 멈춘다. 순차 4버스는 풀 프레임에 80~110ms가 걸려 10ms 예산의
 *          애니메이션이 무너지지만, 동시 전송은 같은 일을 1/4 시간에 끝낸다.
 *
 * @note [BB v1.0.0] Multi_BitBang(bitbank2)을 기반 라이브러리로 내장하고
 *       (lib/Multi_BitBang/, 순차 API·버스 스캔용), 본 파일은 ESP32-C3의
 *       GPIO 레지스터를 직접 써서 **동시 전송 경로**를 추가한 확장이다.
 *       원본 라이브러리 파일은 손대지 않았다.
 */
#ifndef BITBANG_I2C_H
#define BITBANG_I2C_H

#include <Arduino.h>
#include <driver/gpio.h>
#include <soc/gpio_struct.h>
#include "esp_cpu.h"     // esp_cpu_get_cycle_count() — 부팅 시 지연 보정
#include "config.h"
#include "bitbang_i2c_core.h"

class BitBangI2C {
public:
    /**
     * @brief 버스와 핀을 초기화한다
     * @param sdaPins   버스 순서대로의 SDA 핀 번호 (길이 >= busCount)
     * @param sclPin    공유 SCL 핀
     * @param busCount  버스 수 (<= BB_CORE_MAX_BUSES)
     * @param halfBitNs SCL 반주기 지연 (ns). 0이면 보정하지 않고 최대 속도
     * @note ⚠ Multi_BitBang의 Multi_I2CInit()을 **먼저** 끝내고 이 함수를 부른다.
     *       원본 라이브러리는 pinMode(INPUT/OUTPUT)로 핀을 잡으므로, 나중에 부르면
     *       여기서 설정한 Open-Drain + 입력활성 설정을 덮어쓴다.
     */
    void begin(const uint8_t* sdaPins, uint8_t sclPin, uint8_t busCount, uint16_t halfBitNs);

    /** @brief 보정된 반주기 지연의 **실측값** (ns) — 시리얼 보고용 */
    uint32_t measuredHalfBitNs() const { return _measuredNs; }

    /**
     * @brief 참여하는 모든 버스에 **같은 길이**의 서로 다른 데이터를 동시에 보낸다
     * @param addr     I2C 7비트 주소 (이 프로젝트는 전부 0x3C)
     * @param payloads 버스별 페이로드 포인터 배열. 비참여 버스는 nullptr 가능
     * @param length   페이로드 길이 (전 참여 버스 공통 — 호출부가 합집합으로 맞춘다)
     * @param busMask  참여 버스 마스크 (bit N = 버스 N)
     * @return 1 = 전 참여 버스가 전 구간 ACK, 0 = 실패(호출부가 dirty를 남겨 재시도)
     *
     * @details 페이로드에는 제어바이트(0x00 명령 / 0x40 데이터)가 **포함**되어야 한다.
     *          U8g2의 byte_cb가 넘겨주는 패킷과 같은 형식이다.
     */
    uint8_t writeConcurrent(uint8_t addr, const uint8_t* const* payloads,
                            uint16_t length, uint8_t busMask);

    /** @brief 해당 버스에 주소가 응답하는가 (브링업 진단용) */
    uint8_t probe(uint8_t bus, uint8_t addr);

    /**
     * @brief 버스 자가 복구 — SCL을 펄스해 SDA를 붙잡고 있는 슬레이브를 밀어낸다
     * @details BitBang에서는 stuck-low가 **ACK 성공으로 오독**되므로, 타임아웃으로
     *          이상을 감지한 뒤 반드시 이 함수로 버스를 유휴 상태로 되돌려야 한다.
     */
    void recover();

    uint32_t errorCount() const { return _errors; }

    // ── 저수준 핀 조작 ──────────────────────────────────────────────────
    // 모든 SDA/SCL은 Open-Drain: 래치 1 = Hi-Z(외부 풀업이 HIGH로 끌어올림), 0 = LOW로 당김.
    // GPIO_MODE_INPUT_OUTPUT_OD 로 두면 출력과 동시에 입력(ACK 읽기)도 살아 있다.
    //   ⚠ GPIO_MODE_OUTPUT_OD 만 쓰면 입력 버퍼가 꺼져 ACK를 못 읽는다.

    /**
     * @brief 반주기 지연 — 부팅 시 실측 보정한 NOP 루프
     * @details `delayMicroseconds()`는 호출 오버헤드가 커서 1us를 요청해도 실효 1.3us가
     *          된다(실측 455kHz). 여기서는 CPU 사이클을 한 번 재서 목표 ns에 맞춘 횟수만큼
     *          NOP를 돈다 — 오버헤드가 사실상 없다.
     * @note `_delayIters == 0`이면 지연 없이 **최대 속도**로 돈다. 이때 SCL 상승은
     *       `sclWaitHigh()`가 실제 상승을 기다려 자연히 맞춰지지만, LOW 기간이 짧아져
     *       슬레이브가 못 따라올 수 있다 (화면이 깨지면 BB_HALF_BIT_NS를 키운다).
     */
    inline void bitDelay() const {
        for (uint32_t i = 0; i < _delayIters; i++) { __asm__ __volatile__("nop"); }
    }
    inline void sclLow()   const { GPIO.out_w1tc.val = _sclBit; }
    inline void sclHigh()  const { GPIO.out_w1ts.val = _sclBit; }
    inline void sdaDrive(uint32_t ones, uint32_t zeros) const {
        GPIO.out_w1ts.val = ones;
        GPIO.out_w1tc.val = zeros;
    }
    inline void sdaReleaseAll() const { GPIO.out_w1ts.val = _sdaMask; }
    inline uint32_t sdaReadAll() const { return GPIO.in.val & _sdaMask; }

    /**
     * @brief SCL을 HIGH로 놓고 실제로 HIGH가 될 때까지 기다린다
     * @return 0 = 타임아웃 (BB_SCL_STRETCH_TIMEOUT_US 초과)
     * @details 정상 경로에서는 첫 읽기에서 바로 통과하므로 오버헤드가 없다.
     *          micros()는 **비정상 경로에서만** 부른다.
     */
    inline uint8_t sclWaitHigh() const {
        sclHigh();
        if (GPIO.in.val & _sclBit) return 1;
        const uint32_t t0 = micros();
        while (!(GPIO.in.val & _sclBit)) {
            if ((uint32_t)(micros() - t0) > BB_SCL_STRETCH_TIMEOUT_US) return 0;
        }
        return 1;
    }

    inline uint32_t pinMask(uint8_t busMask) const {
        uint32_t m = 0;
        for (uint8_t b = 0; b < _busCount; b++) {
            if (busMask & (uint8_t)(1u << b)) m |= (uint32_t)1u << _sda[b];
        }
        return m;
    }

private:
    uint8_t  _sda[BB_CORE_MAX_BUSES];
    uint8_t  _busCount   = 0;
    uint8_t  _scl        = 0;
    uint16_t _halfBitNs  = 0;   // 목표 반주기 (ns)
    uint32_t _delayIters = 0;   // 보정된 NOP 루프 반복 횟수 (0 = 지연 없음)
    uint32_t _measuredNs = 0;   // 실측 반주기 (ns) — 보고용
    uint8_t  _lastAck    = 0;
    uint32_t _errors     = 0;
    uint32_t _sdaMask    = 0;
    uint32_t _sclBit     = 0;

    void    calibrateBitDelay(uint16_t targetNs);
    uint8_t start(uint8_t busMask);
    void    stop(uint8_t busMask);
    uint8_t writeBytePerBus(const uint8_t* bytes, uint8_t busMask);
};

extern BitBangI2C bitBang;

#endif  // BITBANG_I2C_H
