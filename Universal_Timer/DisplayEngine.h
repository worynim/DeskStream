// worynim@gmail.com
/**
 * @file DisplayEngine.h
 * @brief 4개 OLED 독립 제어 엔진 (U8g2 기반)
 * @details DeskStream 보드의 HW I2C + 고속 SW I2C 구조를 활용한 디스플레이 제어
 */
#ifndef DISPLAY_ENGINE_H
#define DISPLAY_ENGINE_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "driver/i2c_master.h"

#include "config.h"

/**
 * @brief 4개의 OLED 디스플레이를 독립적으로 제어하는 클래스
 * @details Sound_Visualizer_4_OLEDs의 I2C 초기화 구조를 동일하게 활용
 */
class DisplayEngine {
public:
    DisplayEngine();

    /**
     * @brief I2C 버스 및 4개 OLED 초기화
     */
    void begin();

    /**
     * @brief 개별 OLED 화면의 U8g2 객체 포인터 반환
     * @param idx 화면 인덱스 (0~3)
     * @return U8G2 포인터
     */
    U8G2* getScreen(int idx);

    /**
     * @brief 모든 OLED 화면 내용 전송 (sendBuffer)
     */
    void flush();

    /**
     * @brief 특정 OLED 화면만 전송
     * @param idx 화면 인덱스 (0~3)
     */
    void flushScreen(int idx);

    // HW I2C 핸들 및 패킷 버퍼 (C 콜백 함수에서 직접 접근)
    i2c_master_bus_handle_t i2c_bus_handle;
    i2c_master_dev_handle_t i2c_dev_handle[2];
    uint8_t i2c_packet_buf[2][1030];
    uint16_t i2c_packet_ptr[2];

    static U8G2* screens[NUM_SCREENS];

    /**
     * @brief 각 화면별 1024바이트 독립 버퍼 반환
     */
    uint8_t* getScreenBuffer(int idx) {
        if (idx >= 0 && idx < NUM_SCREENS) return screen_buffers[idx];
        return nullptr;
    }

private:
    // 화면별 독립 프레임 버퍼 (U8g2 static 버퍼 공유 충돌 방지)
    uint8_t screen_buffers[NUM_SCREENS][1024];
};

extern DisplayEngine display;

#endif // DISPLAY_ENGINE_H

