// worynim@gmail.com
/**
 * @file DisplayEngine.cpp
 * @brief 4개 OLED 고속 디스플레이 제어 엔진 구현체
 * @details HW I2C (OLED 1, 2) ESP-IDF 직접 전송 및 SW I2C (OLED 3, 4) 초고속 레지스터 비트뱅잉
 */
#include "DisplayEngine.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include "esp_err.h"

// ─── U8g2 객체 생성 ──────────────────────────────────────
// OLED 1, 2: HW I2C (GPIO 5, 6)
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_1(U8G2_R2, 255, 255, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_2(U8G2_R2, 255, 255, U8X8_PIN_NONE);

// OLED 3, 4: SW I2C (GPIO 2, 3) - 고속 비트뱅잉 콜백 적용
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_3(U8G2_R2, SW_SCL_PIN, SW_SDA_PIN, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_4(U8G2_R2, SW_SCL_PIN, SW_SDA_PIN, U8X8_PIN_NONE);

// 정적 스크린 배열 초기화 (0: OLED 1, 1: OLED 2, 2: OLED 3, 3: OLED 4)
U8G2* DisplayEngine::screens[NUM_SCREENS] = { &u8g2_1, &u8g2_2, &u8g2_3, &u8g2_4 };

// ─── HW I2C 콜백 (OLED 1, 2 begin() 초기화용) ─────────────────────
extern "C" uint8_t u8x8_byte_esp32_idf_0(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch(msg) {
        case U8X8_MSG_BYTE_INIT: break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            display.i2c_packet_ptr[0] = 0;
            break;
        case U8X8_MSG_BYTE_SEND:
            if (display.i2c_packet_ptr[0] + arg_int <= sizeof(display.i2c_packet_buf[0])) {
                memcpy(&display.i2c_packet_buf[0][display.i2c_packet_ptr[0]], arg_ptr, arg_int);
                display.i2c_packet_ptr[0] += arg_int;
            }
            break;
        case U8X8_MSG_BYTE_END_TRANSFER:
            if (display.i2c_packet_ptr[0] > 0) {
                i2c_master_transmit(display.i2c_dev_handle[0],
                    display.i2c_packet_buf[0], display.i2c_packet_ptr[0],
                    pdMS_TO_TICKS(I2C_TX_TIMEOUT_MS));
            }
            break;
        default: return 0;
    }
    return 1;
}

extern "C" uint8_t u8x8_byte_esp32_idf_1(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch(msg) {
        case U8X8_MSG_BYTE_INIT: break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            display.i2c_packet_ptr[1] = 0;
            break;
        case U8X8_MSG_BYTE_SEND:
            if (display.i2c_packet_ptr[1] + arg_int <= sizeof(display.i2c_packet_buf[1])) {
                memcpy(&display.i2c_packet_buf[1][display.i2c_packet_ptr[1]], arg_ptr, arg_int);
                display.i2c_packet_ptr[1] += arg_int;
            }
            break;
        case U8X8_MSG_BYTE_END_TRANSFER:
            if (display.i2c_packet_ptr[1] > 0) {
                i2c_master_transmit(display.i2c_dev_handle[1],
                    display.i2c_packet_buf[1], display.i2c_packet_ptr[1],
                    pdMS_TO_TICKS(I2C_TX_TIMEOUT_MS));
            }
            break;
        default: return 0;
    }
    return 1;
}

// ─── SW I2C 고속 레지스터 직접 제어 콜백 (블로킹 최소화) ─────────
extern "C" uint8_t u8x8_gpio_and_delay_fast(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            pinMode(u8x8->pins[U8X8_PIN_I2C_CLOCK], OUTPUT_OPEN_DRAIN);
            pinMode(u8x8->pins[U8X8_PIN_I2C_DATA], OUTPUT_OPEN_DRAIN);
            gpio_pullup_en((gpio_num_t)SW_SCL_PIN);
            gpio_pullup_en((gpio_num_t)SW_SDA_PIN);
            break;
        case U8X8_MSG_DELAY_100NANO:
            break; // 딜레이 없이 최고 속도 실행
        case U8X8_MSG_DELAY_10MICRO:
            esp_rom_delay_us(2);
            break;
        case U8X8_MSG_DELAY_MILLI:
            delay(arg_int);
            break;
        case U8X8_MSG_GPIO_I2C_CLOCK:
            if (arg_int) GPIO.out_w1ts.val = (1 << SW_SCL_PIN);
            else         GPIO.out_w1tc.val = (1 << SW_SCL_PIN);
            break;
        case U8X8_MSG_GPIO_I2C_DATA:
            if (arg_int) GPIO.out_w1ts.val = (1 << SW_SDA_PIN);
            else         GPIO.out_w1tc.val = (1 << SW_SDA_PIN);
            break;
        default:
            // ACK 성공을 위해 0 반환 (I2C 슬레이브 ACK는 LOW)
            u8x8_SetGPIOResult(u8x8, 0);
            break;
    }
    return 1;
}

// ─── DisplayEngine 클래스 구현 ───────────────────────────────

DisplayEngine::DisplayEngine() {}

void DisplayEngine::begin() {
    // [1] ESP-IDF I2C 마스터 버스 구성 (HW SDA 5, SCL 6)
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = (gpio_num_t)HW_SDA_PIN;
    bus_cfg.scl_io_num = (gpio_num_t)HW_SCL_PIN;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = I2C_GLITCH_FILTER;
    bus_cfg.intr_priority = I2C_INTR_PRIO;
    bus_cfg.flags.enable_internal_pullup = true;

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus_handle));

    // [2] I2C 장치 등록 (0x3C: OLED 1, 0x3D: OLED 2)
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = 0x3C;
    dev_cfg.scl_speed_hz = I2C_SPEED_HZ;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &i2c_dev_handle[0]));

    dev_cfg.device_address = 0x3D;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &i2c_dev_handle[1]));

    // [3] HW OLED 1 (0x3C) 초기화
    u8g2_1.getU8x8()->byte_cb = u8x8_byte_esp32_idf_0;
    u8g2_1.begin();
    u8g2_1.getU8x8()->byte_cb = u8x8_byte_esp32_idf_0;

    // [4] HW OLED 2 (0x3D) 초기화
    u8g2_2.getU8x8()->byte_cb = u8x8_byte_esp32_idf_1;
    u8g2_2.setI2CAddress(0x3D * 2);
    u8g2_2.begin();
    u8g2_2.getU8x8()->byte_cb = u8x8_byte_esp32_idf_1;
    u8g2_2.setI2CAddress(0x3D * 2);

    // [5] SW OLED 3, 4 초기화 (고속 레지스터 콜백 장착)
    u8g2_3.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_fast;
    u8g2_3.begin();
    u8g2_3.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_fast;

    u8g2_4.setI2CAddress(0x3D * 2);
    u8g2_4.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_fast;
    u8g2_4.begin();
    u8g2_4.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_fast;
    u8g2_4.setI2CAddress(0x3D * 2);

    // [6] 각 화면에 고유 독립 프레임 버퍼 주입 (U8g2 static 버퍼 충돌 차단)
    for (int i = 0; i < NUM_SCREENS; i++) {
        memset(screen_buffers[i], 0, sizeof(screen_buffers[i]));
        screens[i]->getU8g2()->tile_buf_ptr = screen_buffers[i];
        screens[i]->clearBuffer();
    }
    flush();
}

U8G2* DisplayEngine::getScreen(int idx) {
    if (idx < 0 || idx >= NUM_SCREENS) return nullptr;

#if REVERSE_SCREEN_ORDER
    return screens[NUM_SCREENS - 1 - idx];
#else
    return screens[idx];
#endif
}

void DisplayEngine::flush() {
    for (int i = 0; i < NUM_SCREENS; i++) {
        flushScreen(i);
    }
}

void DisplayEngine::flushScreen(int s) {
    if (s < 0 || s >= NUM_SCREENS) return;

    if (s < 2) {
        // [HW OLED 1, 2] ESP-IDF 하드웨어 I2C 직접 전송 (1MHz 속도)
        uint8_t* u8g2_buf = screens[s]->getBufferPtr();
        if (!u8g2_buf) return;
        i2c_master_dev_handle_t dev_h = i2c_dev_handle[s];

        for (int p = 0; p < PAGES_PER_SCREEN; p++) {
            uint8_t cmd[] = { 0x00, (uint8_t)(0xB0 | p), 0x00, 0x10 };
            i2c_master_transmit(dev_h, cmd, sizeof(cmd), pdMS_TO_TICKS(I2C_CMD_TIMEOUT_MS));

            uint8_t tx[SCREEN_WIDTH + 1];
            tx[0] = 0x40;
            memcpy(&tx[1], &u8g2_buf[p * SCREEN_WIDTH], SCREEN_WIDTH);
            i2c_master_transmit(dev_h, tx, sizeof(tx), pdMS_TO_TICKS(I2C_TX_TIMEOUT_MS));
        }
    } else {
        // [SW OLED 3, 4] 고속 레지스터 제어 비트뱅잉
        screens[s]->sendBuffer();
    }
}

// 전역 인스턴스
DisplayEngine display;
