#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <U8g2lib.h>
#include <Wire.h>
#include "config.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include "driver/i2c_master.h"
#include "LittleFS.h"

class DisplayManager;
extern DisplayManager display;

extern "C" uint8_t u8x8_byte_esp32_idf_0(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
extern "C" uint8_t u8x8_byte_esp32_idf_1(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

extern "C" uint8_t u8x8_gpio_and_delay_esp32_c3_fast(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
  switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
      pinMode(u8x8->pins[U8X8_PIN_I2C_CLOCK], OUTPUT_OPEN_DRAIN);
      pinMode(u8x8->pins[U8X8_PIN_I2C_DATA], OUTPUT_OPEN_DRAIN);
      gpio_pullup_en((gpio_num_t)SW_SCL_PIN);
      gpio_pullup_en((gpio_num_t)SW_SDA_PIN);
      break;
    case U8X8_MSG_GPIO_I2C_CLOCK:
      if (arg_int) GPIO.out_w1ts.val = (1 << SW_SCL_PIN);
      else GPIO.out_w1tc.val = (1 << SW_SCL_PIN);
      break;
    case U8X8_MSG_GPIO_I2C_DATA:
      if (arg_int) GPIO.out_w1ts.val = (1 << SW_SDA_PIN);
      else GPIO.out_w1tc.val = (1 << SW_SDA_PIN);
      break;
    default: break;
  }
  return 1;
}

class DisplayManager {
public:
    U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_1, u8g2_2, u8g2_3, u8g2_4;
    U8G2* screens[NUM_SCREENS];
    i2c_master_bus_handle_t i2c_bus_handle;
    i2c_master_dev_handle_t i2c_dev_handle[2];
    uint8_t i2c_packet_buf[2][HW_I2C_BUF_SIZE];
    uint16_t i2c_packet_ptr[2];
    uint8_t u8g2_buffers[NUM_SCREENS][SCREEN_WIDTH * PAGES_PER_SCREEN]; 
    uint8_t shadow_buffer[NUM_SCREENS][SCREEN_WIDTH * PAGES_PER_SCREEN];
    uint8_t hw_dirty_mask[2], hw_first_tile[2][PAGES_PER_SCREEN], hw_tile_count[2][PAGES_PER_SCREEN];
    volatile bool is_transmitting_hw = false;
    TaskHandle_t hw_task_handle = nullptr, main_task_handle = nullptr;

    DisplayManager() : 
        u8g2_1(U8G2_R2, 255, 255, U8X8_PIN_NONE), u8g2_2(U8G2_R2, 255, 255, U8X8_PIN_NONE),
        u8g2_3(U8G2_R2, SW_SCL_PIN, SW_SDA_PIN, U8X8_PIN_NONE), u8g2_4(U8G2_R2, SW_SCL_PIN, SW_SDA_PIN, U8X8_PIN_NONE) {
        screens[0] = &u8g2_1; screens[1] = &u8g2_2; screens[2] = &u8g2_3; screens[3] = &u8g2_4;
    }

    void begin() {
        main_task_handle = xTaskGetCurrentTaskHandle();
        memset(shadow_buffer, 0xFF, sizeof(shadow_buffer));
        for (int i = 0; i < NUM_SCREENS; i++) screens[i]->getU8g2()->tile_buf_ptr = u8g2_buffers[i];
        i2c_master_bus_config_t bus_cfg = {};
        bus_cfg.i2c_port = I2C_NUM_0; bus_cfg.sda_io_num = (gpio_num_t)HW_SDA_PIN; bus_cfg.scl_io_num = (gpio_num_t)HW_SCL_PIN;
        bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT; bus_cfg.glitch_ignore_cnt = 7; bus_cfg.flags.enable_internal_pullup = true;
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus_handle));
        i2c_device_config_t dev_cfg = {};
        dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7; dev_cfg.device_address = 0x3C; dev_cfg.scl_speed_hz = I2C_SPEED_HZ;
        ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &i2c_dev_handle[0]));
        dev_cfg.device_address = 0x3D;
        ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &i2c_dev_handle[1]));
        u8g2_1.getU8x8()->byte_cb = u8x8_byte_esp32_idf_0; u8g2_1.begin();
        u8g2_2.getU8x8()->byte_cb = u8x8_byte_esp32_idf_1; u8g2_2.setI2CAddress(0x3D * 2); u8g2_2.begin();
        u8g2_3.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast; u8g2_3.begin();
        u8g2_4.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast; u8g2_4.setI2CAddress(0x3D * 2); u8g2_4.begin();
        xTaskCreatePinnedToCore(i2c_hw_task, "I2C_HW", HW_TASK_STACK, this, HW_TASK_PRIO, &hw_task_handle, HW_TASK_CORE);
        for (int i = 0; i < 4; i++) screens[i]->clearBuffer();
        pushParallel();
    }

    void drawCenterText(int idx, const String& text) {
        U8G2* u8g2 = screens[idx];
        u8g2->clearBuffer();
        int i = 0; int charCount = 0; String chars[8];
        while (i < text.length() && charCount < 8) {
            int len = 1; unsigned char c = (unsigned char)text[i];
            if (c < 0x80) len = 1; else if ((c & 0xE0) == 0xC0) len = 2; else if ((c & 0xE0) == 0xE0) len = 3; else if ((c & 0xF8) == 0xF0) len = 4;
            chars[charCount++] = text.substring(i, i + len); i += len;
        }

        // --- 레이아웃 엔진 적용 ---
        int pos[8]; // 각 글자의 X 시작 위치
        if (idx == 0) { // [오전/후] 중앙 정렬
            int totalW = charCount * 32;
            int startX = (128 - totalW) / 2;
            for (int j = 0; j < charCount; j++) pos[j] = startX + (j * 32);
        } else { // [시/분/초] 단위 우측 고정, 숫자 잔여 공간 중앙 정렬
            // 마지막 글자는 단위이므로 맨 오른쪽(96px) 고정
            pos[charCount - 1] = 96;
            // 앞의 숫자 글자들(charCount-1 개)을 0~96px(3칸) 사이 중앙 정렬
            int numChars = charCount - 1;
            if (numChars > 0) {
                int startX = (96 - (numChars * 32)) / 2;
                for (int j = 0; j < numChars; j++) pos[j] = startX + (j * 32);
            }
        }

        bool allFound = true;
        for (int j = 0; j < charCount; j++) {
            String hexStr = "";
            for (int k = 0; k < chars[j].length(); k++) {
                char buf[3]; sprintf(buf, "%02X", (unsigned char)chars[j][k]); hexStr += buf;
            }
            String path = "/c_" + hexStr + ".bin";
            if (LittleFS.exists(path)) {
                File f = LittleFS.open(path, "r");
                uint8_t img[256]; f.read(img, 256); f.close();
                u8g2->drawBitmap(pos[j], 0, 4, 64, img);
            } else { allFound = false; break; }
        }

        if (!allFound || charCount == 0) {
            u8g2->clearBuffer(); u8g2->setFont(KOREAN_FONT);
            int w = u8g2->getUTF8Width(text.c_str());
            u8g2->drawUTF8((SCREEN_WIDTH - w) / 2, TEXT_Y_POS, text.c_str());
        }
    }

    void pushParallel() {
        if (is_transmitting_hw && hw_task_handle) { ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(I2C_SYNC_TIMEOUT_MS)); is_transmitting_hw = false; }
        bool any_hw_dirty = false;
        for (int s = 0; s < 2; s++) {
            hw_dirty_mask[s] = 0; uint8_t* buf = screens[s]->getBufferPtr();
            for (int p = 0; p < PAGES_PER_SCREEN; p++) {
                bool page_dirty = false; int first_tile = -1, last_tile = -1;
                for (int t = 0; t < TILES_PER_PAGE; t++) {
                    bool tile_dirty = false;
                    for (int tx = 0; tx < 8; tx++) {
                        int idx = p * SCREEN_WIDTH + t * 8 + tx;
                        if (buf[idx] != shadow_buffer[s][idx]) { tile_dirty = true; shadow_buffer[s][idx] = buf[idx]; }
                    }
                    if (tile_dirty) { if (first_tile == -1) first_tile = t; last_tile = t; page_dirty = true; }
                }
                if (page_dirty) { hw_dirty_mask[s] |= (1 << p); hw_first_tile[s][p] = (uint8_t)first_tile; hw_tile_count[s][p] = (uint8_t)(last_tile - first_tile + 1); any_hw_dirty = true; }
            }
        }
        if (hw_task_handle && any_hw_dirty) { is_transmitting_hw = true; xTaskNotifyGive(hw_task_handle); }
        for (int s = 2; s < 4; s++) {
            uint8_t* buf = screens[s]->getBufferPtr();
            for (int p = 0; p < PAGES_PER_SCREEN; p++) {
                int first_tile = -1, last_tile = -1; bool page_dirty = false;
                for (int t = 0; t < TILES_PER_PAGE; t++) {
                    bool tile_dirty = false;
                    for (int tx = 0; tx < 8; tx++) {
                        int idx = p * SCREEN_WIDTH + t * 8 + tx;
                        if (buf[idx] != shadow_buffer[s][idx]) { tile_dirty = true; shadow_buffer[s][idx] = buf[idx]; }
                    }
                    if (tile_dirty) { if (first_tile == -1) first_tile = t; last_tile = t; page_dirty = true; }
                }
                if (page_dirty) screens[s]->updateDisplayArea(first_tile, p, last_tile - first_tile + 1, 1);
            }
        }
    }

    static void i2c_hw_task(void* arg) {
        DisplayManager* mgr = (DisplayManager*)arg;
        while (1) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            for (int s = 0; s < 2; s++) {
                uint8_t mask = mgr->hw_dirty_mask[s]; if (mask == 0) continue;
                i2c_master_dev_handle_t dev_h = mgr->i2c_dev_handle[s];
                for (int p = 0; p < PAGES_PER_SCREEN; p++) {
                    if (!(mask & (1 << p))) continue;
                    uint8_t ft = mgr->hw_first_tile[s][p], tc = mgr->hw_tile_count[s][p], col = ft * 8, len = tc * 8;
                    uint8_t cmd[] = {0x00, (uint8_t)(0xB0|p), (uint8_t)(col & 0x0F), (uint8_t)(0x10|(col>>4))};
                    i2c_master_transmit(dev_h, cmd, 4, pdMS_TO_TICKS(I2C_CMD_TIMEOUT_MS));
                    uint8_t tx[129]; tx[0] = 0x40; memcpy(&tx[1], &mgr->shadow_buffer[s][p * SCREEN_WIDTH + col], len);
                    i2c_master_transmit(dev_h, tx, len + 1, pdMS_TO_TICKS(I2C_TX_TIMEOUT_MS));
                }
            }
            if (mgr->main_task_handle) xTaskNotifyGive(mgr->main_task_handle);
        }
    }

    void showStatus(const String& msg) {
        U8G2* u8g2 = screens[0]; u8g2->clearBuffer(); u8g2->setFont(STATUS_FONT);
        u8g2->drawStr(0, 10, msg.c_str()); pushParallel();
    }
};

extern "C" uint8_t u8x8_byte_esp32_idf_0(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch(msg) {
        case U8X8_MSG_BYTE_INIT: break;
        case U8X8_MSG_BYTE_START_TRANSFER: display.i2c_packet_ptr[0] = 0; break;
        case U8X8_MSG_BYTE_SEND: memcpy(&display.i2c_packet_buf[0][display.i2c_packet_ptr[0]], arg_ptr, arg_int); display.i2c_packet_ptr[0] += arg_int; break;
        case U8X8_MSG_BYTE_END_TRANSFER: i2c_master_transmit(display.i2c_dev_handle[0], display.i2c_packet_buf[0], display.i2c_packet_ptr[0], pdMS_TO_TICKS(I2C_TX_TIMEOUT_MS)); break;
        default: return 0;
    }
    return 1;
}

extern "C" uint8_t u8x8_byte_esp32_idf_1(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch(msg) {
        case U8X8_MSG_BYTE_INIT: break;
        case U8X8_MSG_BYTE_START_TRANSFER: display.i2c_packet_ptr[1] = 0; break;
        case U8X8_MSG_BYTE_SEND: memcpy(&display.i2c_packet_buf[1][display.i2c_packet_ptr[1]], arg_ptr, arg_int); display.i2c_packet_ptr[1] += arg_int; break;
        case U8X8_MSG_BYTE_END_TRANSFER: i2c_master_transmit(display.i2c_dev_handle[1], display.i2c_packet_buf[1], display.i2c_packet_ptr[1], pdMS_TO_TICKS(I2C_TX_TIMEOUT_MS)); break;
        default: return 0;
    }
    return 1;
}
#endif
