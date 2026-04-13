// worynim@gmail.com
/**
 * @file i2c_platform.h
 * @brief I2C 하드웨어 추상화 및 병렬 전송 클래스 정의
 * @details 4개 OLED 디스플레이를 위한 병렬 I2C 전송 로직 및 버스 자가 치유(Recovery) 관리
 */
#ifndef I2C_PLATFORM_H
#define I2C_PLATFORM_H

#include <Arduino.h>
#include <driver/i2c_master.h>
#include <U8g2lib.h>
#include "config.h"

/**
 * @brief 하드웨어 I2C 통신 및 디스플레이 전송 추상화 레이어
 */
class I2CPlatform {
public:
    I2CPlatform();

    // 초기화 및 전송 상태 관리
    void begin();
    void notifyTransmission();
    bool isTransmitting() const { return _is_transmitting; }
    void waitForSync(uint32_t timeout_ms);

    // 디스플레이 전송 인터페이스 (상위 레벨)
    void preparePageUpdate(uint8_t screenIdx, uint8_t page, uint8_t firstTile, uint8_t tileCount);
    void setShadowData(uint8_t screenIdx, int offset, uint8_t data);
    uint8_t getShadowData(uint8_t screenIdx, int offset) const;
    void invalidateShadow(uint8_t screenIdx);

    // 하위 레벨 명령 (SSD1306 전용)
    esp_err_t sendCommand(uint8_t screenIdx, const uint8_t* cmd, size_t len);
    esp_err_t sendData(uint8_t screenIdx, const uint8_t* data, size_t len);

    // 자가 복구 로직
    void checkAndRecover();
    void recoverBus();

    // 외부용 패킷 버퍼 접근 (U8g2 콜백용)
    uint8_t* getPacketBuf(uint8_t busIdx) { return _i2c_packet_buf[busIdx]; }
    uint16_t& getPacketPtr(uint8_t busIdx) { return _i2c_packet_ptr[busIdx]; }
    i2c_master_dev_handle_t getDevHandle(uint8_t screenIdx) { return _i2c_dev_handle[screenIdx]; }

    // HW 전송 태스크
    static void i2c_hw_task(void* arg);

private:
    i2c_master_bus_handle_t _i2c_bus_handle;
    i2c_master_dev_handle_t _i2c_dev_handle[2];
    
    uint8_t _i2c_packet_buf[2][HW_I2C_BUF_SIZE];
    uint16_t _i2c_packet_ptr[2];

    uint8_t _shadow_buffer[NUM_SCREENS][SCREEN_WIDTH * PAGES_PER_SCREEN];
    uint8_t _hw_dirty_mask[2];
    uint8_t _hw_first_tile[2][PAGES_PER_SCREEN];
    uint8_t _hw_tile_count[2][PAGES_PER_SCREEN];

    volatile bool _is_transmitting = false;
    uint32_t _error_count = 0;
    
    TaskHandle_t _hw_task_handle = nullptr;
    TaskHandle_t _main_task_handle = nullptr;
};

extern I2CPlatform i2cPlatform;

// U8g2 전용 하위 레벨 콜백 함수 (i2c_platform.cpp에 구현됨)
extern "C" uint8_t u8x8_byte_esp32_idf_0(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
extern "C" uint8_t u8x8_byte_esp32_idf_1(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
extern "C" uint8_t u8x8_gpio_and_delay_esp32_c3_fast(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr);

#endif
