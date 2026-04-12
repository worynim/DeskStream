/**
 * @file i2c_platform.cpp
 * @brief I2C 하드웨어 추상화 및 병렬 전송 클래스 구현
 * @details 4개 OLED 분산 전송, 버스 자가 치유(Recovery) 및 하드웨어 통신 무결성 확보 로직 구현
 */
#include "i2c_platform.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"

I2CPlatform i2cPlatform;

// OLED 기기 주소 (config.h 참조)
static const uint8_t I2C_ADDRS[2] = {I2C_ADDR_HW_0, I2C_ADDR_HW_1};

I2CPlatform::I2CPlatform() {
    memset(_shadow_buffer, 0xFF, sizeof(_shadow_buffer));
}

void I2CPlatform::begin() {
    _main_task_handle = xTaskGetCurrentTaskHandle();

    // 1. I2C 마스터 버스 설정 (ESP-IDF)
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = (gpio_num_t)HW_SDA_PIN;
    bus_cfg.scl_io_num = (gpio_num_t)HW_SCL_PIN;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &_i2c_bus_handle));

    // 2. 디바이스 등록 (0x3C, 0x3D)
    for (int i = 0; i < 2; i++) {
        i2c_device_config_t dev_cfg = {};
        dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        dev_cfg.device_address = I2C_ADDRS[i];
        dev_cfg.scl_speed_hz = I2C_SPEED_HZ;
        ESP_ERROR_CHECK(i2c_master_bus_add_device(_i2c_bus_handle, &dev_cfg, &_i2c_dev_handle[i]));
    }

    // 3. HW 전송 백그라운드 태스크 생성
    xTaskCreatePinnedToCore(i2c_hw_task, "I2C_HW", HW_TASK_STACK, this, HW_TASK_PRIO, &_hw_task_handle, HW_TASK_CORE);
}

void I2CPlatform::notifyTransmission() {
    if (_hw_task_handle) {
        _is_transmitting = true;
        xTaskNotifyGive(_hw_task_handle);
    }
}

void I2CPlatform::waitForSync(uint32_t timeout_ms) {
    if (_is_transmitting && _hw_task_handle) {
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeout_ms)) == 0) {
            Serial.printf("[I2C] Sync Timeout! (%d ms)\n", timeout_ms);
            _error_count += 5; // 동기화 타임아웃은 심각한 지연이므로 가중치 부여
        }
        _is_transmitting = false;
    }
}

void I2CPlatform::preparePageUpdate(uint8_t screenIdx, uint8_t page, uint8_t firstTile, uint8_t tileCount) {
    if (screenIdx < 2) {
        _hw_dirty_mask[screenIdx] |= (1 << page);
        _hw_first_tile[screenIdx][page] = firstTile;
        _hw_tile_count[screenIdx][page] = tileCount;
    }
}

void I2CPlatform::setShadowData(uint8_t screenIdx, int offset, uint8_t data) {
    if (screenIdx < NUM_SCREENS && offset < (SCREEN_WIDTH * PAGES_PER_SCREEN)) {
        _shadow_buffer[screenIdx][offset] = data;
    }
}

void I2CPlatform::invalidateShadow(uint8_t screenIdx) {
    if (screenIdx < NUM_SCREENS) {
        // 0xEE는 0x00(Clear)이나 0xFF(Initial)와 다른 특수 값으로, 
        // 다음 pushParallel 시 강제로 하드웨어 전송을 유도함
        memset(_shadow_buffer[screenIdx], 0xEE, sizeof(_shadow_buffer[screenIdx]));
    }
}

uint8_t I2CPlatform::getShadowData(uint8_t screenIdx, int offset) const {
    if (screenIdx < NUM_SCREENS && offset < (SCREEN_WIDTH * PAGES_PER_SCREEN)) {
        return _shadow_buffer[screenIdx][offset];
    }
    return 0;
}

esp_err_t I2CPlatform::sendCommand(uint8_t screenIdx, const uint8_t* cmd, size_t len) {
    if (screenIdx >= 2) return ESP_ERR_INVALID_ARG;
    return i2c_master_transmit(_i2c_dev_handle[screenIdx], cmd, len, pdMS_TO_TICKS(I2C_CMD_TIMEOUT_MS));
}

esp_err_t I2CPlatform::sendData(uint8_t screenIdx, const uint8_t* data, size_t len) {
    if (screenIdx >= 2) return ESP_ERR_INVALID_ARG;
    return i2c_master_transmit(_i2c_dev_handle[screenIdx], data, len, pdMS_TO_TICKS(I2C_TX_TIMEOUT_MS));
}

void I2CPlatform::i2c_hw_task(void* arg) {
    I2CPlatform* plt = (I2CPlatform*)arg;
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        bool has_error = false;

        for (int s = 0; s < 2; s++) {
            uint8_t mask = plt->_hw_dirty_mask[s];
            if (mask == 0) continue;
            
            i2c_master_dev_handle_t dev_h = plt->_i2c_dev_handle[s];
            for (int p = 0; p < PAGES_PER_SCREEN; p++) {
                if (!(mask & (1 << p))) continue;

                uint8_t ft = plt->_hw_first_tile[s][p];
                uint8_t tc = plt->_hw_tile_count[s][p];
                uint8_t col = ft * 8;
                uint8_t len = tc * 8;

                // 1. Page/Column 설정 명령
                uint8_t cmd[] = {0x00, (uint8_t)(0xB0 | p), (uint8_t)(col & 0x0F), (uint8_t)(0x10 | (col >> 4))};
                esp_err_t err = i2c_master_transmit(dev_h, cmd, 4, pdMS_TO_TICKS(I2C_CMD_TIMEOUT_MS));
                if (err != ESP_OK) {
                    Serial.printf("[I2C] CMD Error: 0x%X (S:%d, P:%d)\n", err, s, p);
                    has_error = true;
                }

                if (!has_error) {
                    // 2. 데이터 전송
                    uint8_t tx[129];
                    tx[0] = 0x40;
                    memcpy(&tx[1], &plt->_shadow_buffer[s][p * SCREEN_WIDTH + col], len);
                    err = i2c_master_transmit(dev_h, tx, len + 1, pdMS_TO_TICKS(I2C_TX_TIMEOUT_MS));
                    if (err != ESP_OK) {
                        Serial.printf("[I2C] DATA Error: 0x%X (S:%d, P:%d)\n", err, s, p);
                        has_error = true;
                    }
                }
            }
            plt->_hw_dirty_mask[s] = 0; // 전송 완료 후 마스크 초기화
        }

        if (has_error) {
            plt->_error_count++;
            if (plt->_error_count > 50) { // I2C_ERROR_THRESHOLD
                plt->recoverBus();
            }
        } else if (plt->_error_count > 0) {
            plt->_error_count--;
        }

        if (plt->_main_task_handle) xTaskNotifyGive(plt->_main_task_handle);
    }
}

void I2CPlatform::recoverBus() {
    Serial.println("[I2C] Physical Bus Recovery Sequence Started...");
    
    // 1. 기존 I2C 리소스 해제
    for (int i = 0; i < 2; i++) {
        if (_i2c_dev_handle[i]) {
            i2c_master_bus_rm_device(_i2c_dev_handle[i]);
            _i2c_dev_handle[i] = nullptr;
        }
    }
    if (_i2c_bus_handle) {
        i2c_del_master_bus(_i2c_bus_handle);
        _i2c_bus_handle = nullptr;
    }

    // 2. Bus Recovery (Clock Pulsing) - Stuck 데이터 라인 해제
    pinMode(HW_SCL_PIN, OUTPUT_OPEN_DRAIN);
    pinMode(HW_SDA_PIN, INPUT_PULLUP);
    
    for (int i = 0; i < 16; i++) {
        digitalWrite(HW_SCL_PIN, LOW);
        delayMicroseconds(10);
        digitalWrite(HW_SCL_PIN, HIGH);
        delayMicroseconds(10);
    }
    
    // 3. I2C 버스 및 디바이스 재초기화
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = (gpio_num_t)HW_SDA_PIN;
    bus_cfg.scl_io_num = (gpio_num_t)HW_SCL_PIN;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;
    
    if (i2c_new_master_bus(&bus_cfg, &_i2c_bus_handle) == ESP_OK) {
        for (int i = 0; i < 2; i++) {
            i2c_device_config_t dev_cfg = {};
            dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
            dev_cfg.device_address = I2C_ADDRS[i];
            dev_cfg.scl_speed_hz = I2C_SPEED_HZ;
            i2c_master_bus_add_device(_i2c_bus_handle, &dev_cfg, &_i2c_dev_handle[i]);
        }
        Serial.println("[I2C] HW Bus Re-initialized.");
    } else {
        Serial.println("[I2C] Bus Re-init FAILED!");
    }

    _error_count = 0;
}

// --- U8g2 콜백 함수 구현 (이관됨) ---

extern "C" uint8_t u8x8_byte_esp32_idf_0(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch(msg) {
        case U8X8_MSG_BYTE_INIT: break;
        case U8X8_MSG_BYTE_START_TRANSFER: i2cPlatform.getPacketPtr(0) = 0; break;
        case U8X8_MSG_BYTE_SEND: 
            memcpy(&i2cPlatform.getPacketBuf(0)[i2cPlatform.getPacketPtr(0)], arg_ptr, arg_int); 
            i2cPlatform.getPacketPtr(0) += arg_int; 
            break;
        case U8X8_MSG_BYTE_END_TRANSFER: 
            i2c_master_transmit(i2cPlatform.getDevHandle(0), i2cPlatform.getPacketBuf(0), i2cPlatform.getPacketPtr(0), pdMS_TO_TICKS(I2C_TX_TIMEOUT_MS)); 
            break;
        default: return 0;
    }
    return 1;
}

extern "C" uint8_t u8x8_byte_esp32_idf_1(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch(msg) {
        case U8X8_MSG_BYTE_INIT: break;
        case U8X8_MSG_BYTE_START_TRANSFER: i2cPlatform.getPacketPtr(1) = 0; break;
        case U8X8_MSG_BYTE_SEND: 
            memcpy(&i2cPlatform.getPacketBuf(1)[i2cPlatform.getPacketPtr(1)], arg_ptr, arg_int); 
            i2cPlatform.getPacketPtr(1) += arg_int; 
            break;
        case U8X8_MSG_BYTE_END_TRANSFER: 
            i2c_master_transmit(i2cPlatform.getDevHandle(1), i2cPlatform.getPacketBuf(1), i2cPlatform.getPacketPtr(1), pdMS_TO_TICKS(I2C_TX_TIMEOUT_MS)); 
            break;
        default: return 0;
    }
    return 1;
}

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
