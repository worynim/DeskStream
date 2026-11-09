// worynim@gmail.com
/**
 * @file ds_i2c_platform.h
 * @brief HW I2C 버스(화면 0·1) 비동기 전송 · 셰도 버퍼 · 버스 자가 복구
 * @details ESP-IDF i2c_master 드라이버를 쓰고, 전송은 별도 태스크(I2C_HW)가 맡아
 *          메인 태스크의 SW 버스 전송과 병렬로 돈다. 셰도 버퍼는 4화면 모두 여기 있다.
 * @note 출처: Hangeul_Clock/i2c_platform.* (v2.10.0). config.h 매크로 → DisplayConfig.
 *       DisplayCore 내부 부품이다 — 스케치는 보통 DisplayCore만 쓴다.
 */
#ifndef DS_I2C_PLATFORM_H
#define DS_I2C_PLATFORM_H

#include <Arduino.h>
#include <driver/i2c_master.h>
#include <U8g2lib.h>
#include "../ds_board.h"
#include "ds_display_config.h"

namespace ds {

class I2CPlatform {
public:
    I2CPlatform();

    // 초기화 및 전송 상태 관리
    void begin(const DisplayConfig& cfg);
    void notifyTransmission();
    bool isTransmitting() const { return _is_transmitting; }
    void waitForSync();

    /**
     * @brief 아직 HW 버스로 전송되지 않은 페이지가 남아 있는가
     * @details I2C 전송에 실패한 페이지는 dirty 비트를 지우지 않고 남겨 둔다.
     *          그런데 셰도 버퍼는 이미 새 값으로 갱신되었으므로 다음 diff는 그 페이지를
     *          잡지 못한다. → 호출부가 "새 diff"뿐 아니라 **이 잔여분까지** 보고 알림을 보내야
     *          재시도가 일어난다.
     */
    bool hasPendingHwUpdate() const { return _hw_dirty_mask[0] || _hw_dirty_mask[1]; }

    // 디스플레이 전송 인터페이스
    void preparePageUpdate(uint8_t screenIdx, uint8_t page, uint8_t firstTile, uint8_t tileCount);
    uint8_t* shadow(uint8_t screenIdx) { return _shadow_buffer[screenIdx]; }
    void invalidateShadow(uint8_t screenIdx);

    // 자가 복구 로직
    void recoverBus();

    // U8g2 byte 콜백(화면 0·1 초기화·명령용)이 쓰는 패킷 버퍼
    uint8_t* getPacketBuf(uint8_t busIdx) { return _i2c_packet_buf[busIdx]; }
    uint16_t& getPacketPtr(uint8_t busIdx) { return _i2c_packet_ptr[busIdx]; }
    i2c_master_dev_handle_t getDevHandle(uint8_t screenIdx) { return _i2c_dev_handle[screenIdx]; }
    uint32_t txTimeoutMs() const { return _cfg.txTimeoutMs; }

    // ── 전송 통계 (FPS 벤치마크용) ──────────────────────────────────────
    /**
     * @brief HW 버스가 실제로 낸 비트 주기 수·전송 시간, 메인이 HW를 기다린 시간을 누적한다
     * @details 비트 주기 = START 1 + (주소 1 + 페이로드 N) × 9(8비트+ACK) + STOP 1.
     *          `statHwBits / statHwUs` = HW 버스 실효 SCL 주파수.
     */
    void     resetStats() { _statHwBits = 0; _statHwUs = 0; _statWaitUs = 0; _statHwTx = 0; }
    uint64_t statHwBits() const { return _statHwBits; }
    uint64_t statHwUs()   const { return _statHwUs; }
    uint64_t statWaitUs() const { return _statWaitUs; }
    uint32_t statHwTx()   const { return _statHwTx; }

    /** HW 버퍼 크기 — U8g2 명령 패킷 상한 */
    static constexpr uint16_t kPacketBufSize = 256;

private:
    static void i2c_hw_task(void* arg);
    bool createBus();
    bool transmitPage(uint8_t screenIdx, uint8_t page);

    DisplayConfig _cfg;
    i2c_master_bus_handle_t _i2c_bus_handle = nullptr;
    i2c_master_dev_handle_t _i2c_dev_handle[2] = {nullptr, nullptr};

    uint8_t _i2c_packet_buf[2][kPacketBufSize];
    uint16_t _i2c_packet_ptr[2] = {0, 0};

    uint8_t _shadow_buffer[kNumScreens][kScreenBufBytes];
    uint8_t _hw_dirty_mask[2] = {0, 0};
    uint8_t _hw_first_tile[2][kPagesPerScreen];
    uint8_t _hw_tile_count[2][kPagesPerScreen];

    volatile bool _is_transmitting = false;
    uint32_t _error_count = 0;

    volatile uint64_t _statHwBits = 0;   // HW 태스크가 갱신, 메인이 읽음 (벤치마크는 동기화 후 읽는다)
    volatile uint64_t _statHwUs   = 0;
    uint64_t _statWaitUs = 0;
    volatile uint32_t _statHwTx   = 0;

    TaskHandle_t _hw_task_handle = nullptr;
    TaskHandle_t _main_task_handle = nullptr;
};

}  // namespace ds

// U8g2 콜백 (화면 0·1: HW 패킷 전송, 화면 2·3: SW gpio 초기화)
extern "C" uint8_t ds_u8x8_byte_esp32_idf_0(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr);
extern "C" uint8_t ds_u8x8_byte_esp32_idf_1(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr);
extern "C" uint8_t ds_u8x8_gpio_and_delay_esp32_c3_fast(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr);

#endif
