#ifndef DISPLAY_ENGINE_H
#define DISPLAY_ENGINE_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <LovyanGFX.hpp>
#include "driver/i2c_master.h" // ESP-IDF v5.0+ I2C Master 드라이버

#include "config.h"

/**
 * @brief 4개의 OLED 디스플레이와 512x64 가상 캔버스를 제어하는 클래스
 */
class DisplayEngine {
public:
    DisplayEngine();
    void begin();
    void pushCanvas(); // 전체 전송 (HW/SW 병렬)
    void smartDelay(uint32_t ms);
    
    LGFX_Sprite& getCanvas() { return canvas; }
    uint32_t getLastTransmitTime() const { return last_transmit_time; }
    void incrementFrameCounter() { frame_counter++; }
    uint32_t getFrameCounter() const { return frame_counter; }
    void resetFrameCounter() { frame_counter = 0; }
    
    // [공개] HW I2C 제어 및 버퍼 (C 콜백 함수 접근용)
    i2c_master_bus_handle_t i2c_bus_handle;
    i2c_master_dev_handle_t i2c_dev_handle[2];
    uint8_t i2c_packet_buf[2][HW_I2C_BUF_SIZE];
    uint16_t i2c_packet_ptr[2];
    uint8_t hw_dirty_mask[NUM_SCREENS / 2];
    uint8_t hw_first_tile[2][8];
    uint8_t hw_tile_count[2][8];

    static U8G2* screens[NUM_SCREENS];
    i2c_master_dev_handle_t getHWDevHandle(int idx) { return i2c_dev_handle[idx]; }
    U8G2* getU8g2(int idx) { return screens[idx]; }

private:
    LGFX_Sprite canvas;
    uint8_t shadow_buffer[NUM_SCREENS][SCREEN_BUF_SIZE]; 
    uint32_t last_transmit_time = 0;
    volatile uint32_t frame_counter = 0;
    volatile bool is_transmitting_hw = false; // HW 전송 중인지 확인용 플래그

    TaskHandle_t hw_task_handle = nullptr;
    static void i2c_hw_task(void* arg); // 하드웨어 전송 전용 태스크
};

extern DisplayEngine display;

#endif
