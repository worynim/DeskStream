#ifndef DISPLAY_ENGINE_H
#define DISPLAY_ENGINE_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <LovyanGFX.hpp>
#include "driver/i2c_master.h"  // ESP-IDF v5.0+ I2C Master 드라이버

#include "config.h"

/**
 * @brief 4개의 OLED 디스플레이와 512x64 가상 캔버스를 제어하는 클래스
 * 
 * Sound_Visualizer_4_OLEDs의 DisplayEngine을 기반으로,
 * 오디오 관련 로직을 제거하고 화면 수신 스트리머에 맞게 최적화함.
 * 
 * 핵심 최적화:
 *  - ESP-IDF I2C Master로 HW I2C 1MHz 직접 제어 (OLED 1, 2)
 *  - Bit-bang 고속 SW I2C 콜백 (딜레이 제거) (OLED 3, 4)
 *  - HW 전송을 FreeRTOS 백그라운드 태스크에서 병렬 실행
 *  - Dirty-Tile Update: 변경된 타일만 전송하여 대역폭 절약
 */
class DisplayEngine {
public:
    DisplayEngine();
    void begin();
    void pushCanvas(bool force_no_flip = false); // 전체 캔버스를 4개 OLED에 병렬 전송
    void smartDelay(uint32_t ms);
    void clearCanvas();        // 캔버스 전체 클리어
    
    LGFX_Sprite& getCanvas()             { return canvas; }
    uint32_t getLastTransmitTime() const { return last_transmit_time; }
    void incrementFrameCounter()         { frame_counter++; }
    uint32_t getFrameCounter() const     { return frame_counter; }
    void resetFrameCounter()             { frame_counter = 0; }

    // [공개] HW I2C 제어 및 버퍼 (C 콜백 함수 접근용)
    i2c_master_bus_handle_t i2c_bus_handle;
    i2c_master_dev_handle_t i2c_dev_handle[2];
    uint8_t  i2c_packet_buf[2][HW_I2C_BUF_SIZE];
    uint16_t i2c_packet_ptr[2];
    uint8_t  hw_dirty_mask[NUM_SCREENS / 2];
    uint8_t  hw_first_tile[2][8];
    uint8_t  hw_tile_count[2][8];

    static U8G2* screens[NUM_SCREENS];
    i2c_master_dev_handle_t getHWDevHandle(int idx) { return i2c_dev_handle[idx]; }
    U8G2* getU8g2(int idx) { return screens[idx]; }

private:
    LGFX_Sprite canvas;
    uint8_t shadow_buffer[NUM_SCREENS][SCREEN_BUF_SIZE];
    uint32_t last_transmit_time = 0;
    volatile uint32_t frame_counter = 0;
    volatile bool is_transmitting_hw = false;

    TaskHandle_t hw_task_handle = nullptr;
    static void i2c_hw_task(void* arg);  // HW 전송 전용 백그라운드 태스크
};

extern DisplayEngine display;

#endif // DISPLAY_ENGINE_H
