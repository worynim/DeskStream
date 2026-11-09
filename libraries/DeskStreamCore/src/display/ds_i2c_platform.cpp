// worynim@gmail.com
/**
 * @file ds_i2c_platform.cpp
 * @brief HW I2C 버스 비동기 전송 · 셰도 버퍼 · 버스 자가 복구 구현
 * @note 출처: Hangeul_Clock/i2c_platform.cpp (v2.10.0)
 */
#include "ds_i2c_platform.h"
#include "ds_i2c_retry_policy.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"

namespace ds {

// U8g2 콜백은 버스 인덱스만 받으므로, 동작 중인 플랫폼을 파일 정적 포인터로 찾는다 (보드당 1개).
static I2CPlatform* s_platform = nullptr;

I2CPlatform::I2CPlatform() {
    memset(_shadow_buffer, 0xFF, sizeof(_shadow_buffer));
}

bool I2CPlatform::createBus() {
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = (gpio_num_t)kHwSdaPin;
    bus_cfg.scl_io_num = (gpio_num_t)kHwSclPin;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;
    if (i2c_new_master_bus(&bus_cfg, &_i2c_bus_handle) != ESP_OK) return false;

    // 디바이스 등록 (0x3C, 0x3D)
    for (int i = 0; i < 2; i++) {
        i2c_device_config_t dev_cfg = {};
        dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        dev_cfg.device_address = kOledAddr[i];
        dev_cfg.scl_speed_hz = _cfg.hwI2cHz;
        if (i2c_master_bus_add_device(_i2c_bus_handle, &dev_cfg, &_i2c_dev_handle[i]) != ESP_OK) return false;
    }
    return true;
}

void I2CPlatform::begin(const DisplayConfig& cfg) {
    _cfg = cfg;
    s_platform = this;
    _main_task_handle = xTaskGetCurrentTaskHandle();

    if (!createBus()) Serial.println("[I2C] HW bus init FAILED!");

    // HW 전송 백그라운드 태스크
    xTaskCreatePinnedToCore(i2c_hw_task, "I2C_HW", _cfg.hwTaskStack, this, _cfg.hwTaskPrio,
                            &_hw_task_handle, _cfg.hwTaskCore);
}

void I2CPlatform::notifyTransmission() {
    if (_hw_task_handle) {
        _is_transmitting = true;
        xTaskNotifyGive(_hw_task_handle);
    }
}

void I2CPlatform::waitForSync() {
    if (_is_transmitting && _hw_task_handle) {
        const uint32_t w0 = micros();
        const uint32_t got = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(_cfg.syncTimeoutMs));
        _statWaitUs += (uint32_t)(micros() - w0);
        if (got == 0) {
            Serial.printf("[I2C] Sync Timeout! (%u ms)\n", (unsigned)_cfg.syncTimeoutMs);
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

void I2CPlatform::invalidateShadow(uint8_t screenIdx) {
    if (screenIdx < kNumScreens) {
        // 0xEE는 0x00(Clear)이나 0xFF(Initial)와 다른 특수 값으로,
        // 다음 push 시 강제로 하드웨어 전송을 유도함
        memset(_shadow_buffer[screenIdx], 0xEE, sizeof(_shadow_buffer[screenIdx]));
    }
}

/**
 * @brief 한 화면의 한 페이지를 셰도 버퍼에서 I2C로 전송한다. 성공했으면 true.
 * @details 병합 모드: 명령과 데이터를 **한 트랜잭션**으로 보낸다 (SSD1306 제어 바이트의 Co 비트).
 *            0x80 = "Co=1, D/C#=0" → 다음 1바이트는 명령이고 그 뒤에 또 제어 바이트가 온다.
 *            0x40 = "Co=0, D/C#=1" → 이후 STOP까지 전부 GDDRAM 데이터.
 *          분리 모드: CMD(페이지/열 지정)를 먼저 보내고 **성공한 경우에만** 데이터를 보낸다.
 */
bool I2CPlatform::transmitPage(uint8_t screenIdx, uint8_t page) {
    i2c_master_dev_handle_t dev = _i2c_dev_handle[screenIdx];
    const uint8_t col = _hw_first_tile[screenIdx][page] * 8;
    const uint8_t len = _hw_tile_count[screenIdx][page] * 8;
    const uint8_t* src = &_shadow_buffer[screenIdx][page * kScreenWidth + col];

    if (_cfg.hwMergedPageTx) {
        uint8_t tx[7 + kScreenWidth];
        tx[0] = 0x80; tx[1] = (uint8_t)(0xB0 | page);          // 페이지 주소
        tx[2] = 0x80; tx[3] = (uint8_t)(col & 0x0F);           // 열 하위 니블
        tx[4] = 0x80; tx[5] = (uint8_t)(0x10 | (col >> 4));    // 열 상위 니블
        tx[6] = 0x40;
        memcpy(&tx[7], src, len);
        const esp_err_t err = i2c_master_transmit(dev, tx, 7 + len, pdMS_TO_TICKS(_cfg.txTimeoutMs));
        if (err != ESP_OK) {
            Serial.printf("[I2C] PAGE Error: 0x%X (S:%d, P:%d)\n", err, screenIdx, page);
            return false;
        }
        _statHwBits += 2u + 9u * (1u + 7u + len);   // 주소 + 명령 3쌍 + 0x40 + len — 한 트랜잭션
        _statHwTx += 1;
        return true;
    }

    uint8_t cmd[] = {0x00, (uint8_t)(0xB0 | page), (uint8_t)(col & 0x0F), (uint8_t)(0x10 | (col >> 4))};
    esp_err_t err = i2c_master_transmit(dev, cmd, 4, pdMS_TO_TICKS(_cfg.cmdTimeoutMs));
    if (err != ESP_OK) {
        Serial.printf("[I2C] CMD Error: 0x%X (S:%d, P:%d)\n", err, screenIdx, page);
        return false;
    }
    uint8_t tx[1 + kScreenWidth];
    tx[0] = 0x40;
    memcpy(&tx[1], src, len);
    err = i2c_master_transmit(dev, tx, len + 1, pdMS_TO_TICKS(_cfg.txTimeoutMs));
    if (err != ESP_OK) {
        Serial.printf("[I2C] DATA Error: 0x%X (S:%d, P:%d)\n", err, screenIdx, page);
        return false;
    }
    _statHwBits += (2u + 9u * (1u + 4u)) + (2u + 9u * (1u + 1u + len));   // CMD + DATA 두 트랜잭션
    _statHwTx += 2;
    return true;
}

void I2CPlatform::i2c_hw_task(void* arg) {
    I2CPlatform* plt = (I2CPlatform*)arg;
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const uint32_t t0 = micros();   // 통계: 이번 회차 HW 전송 시간
        bool has_error = false;

        for (int s = 0; s < 2; s++) {
            uint8_t mask = plt->_hw_dirty_mask[s];
            if (mask == 0) continue;

            // 실패한 페이지는 비트를 남겨 둔다 → 다음 알림 때 재전송된다.
            //   (마스크 갱신 규칙은 ds_i2c_retry_policy.h의 순수 함수 — 회귀 테스트 대상)
            uint8_t page_ok[kPagesPerScreen];
            for (int p = 0; p < kPagesPerScreen; p++) {
                if (!(mask & (1 << p))) { page_ok[p] = 1; continue; }   // 시도 안 한 페이지는 '유지'
                page_ok[p] = plt->transmitPage(s, p) ? 1 : 0;
                if (!page_ok[p]) has_error = true;
            }
            plt->_hw_dirty_mask[s] = dirtyMaskAfterSend(mask, page_ok, kPagesPerScreen);
        }

        if (has_error) {
            plt->_error_count++;
            if (plt->_error_count > plt->_cfg.errorThreshold) plt->recoverBus();
        } else if (plt->_error_count > 0) {
            plt->_error_count--;
        }

        plt->_statHwUs += (uint32_t)(micros() - t0);
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
    pinMode(kHwSclPin, OUTPUT_OPEN_DRAIN);
    pinMode(kHwSdaPin, INPUT_PULLUP);
    for (int i = 0; i < 16; i++) {
        digitalWrite(kHwSclPin, LOW);
        delayMicroseconds(10);
        digitalWrite(kHwSclPin, HIGH);
        delayMicroseconds(10);
    }

    // 3. I2C 버스 및 디바이스 재초기화
    Serial.println(createBus() ? "[I2C] HW Bus Re-initialized." : "[I2C] Bus Re-init FAILED!");
    _error_count = 0;
}

}  // namespace ds

// --- U8g2 콜백 ---

// U8g2는 버스 인덱스를 넘겨주지 않으므로, 두 화면을 별도 함수 포인터로 등록한다.
// 실제 전송 로직은 동일하므로 busIdx만 받아 한 번만 구현한다.
static uint8_t u8x8_byte_esp32_idf(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr, uint8_t busIdx) {
    ds::I2CPlatform* plt = ds::s_platform;
    if (!plt) return 0;
    uint8_t* buf = plt->getPacketBuf(busIdx);
    uint16_t& ptr = plt->getPacketPtr(busIdx);
    // 버퍼가 넘치면 그 패킷은 전송하지 않는다 — 절반만 나간 페이지는 화면에 찢김이 된다.
    static bool overflow[2] = {false, false};
    switch (msg) {
        case U8X8_MSG_BYTE_INIT: break;
        case U8X8_MSG_BYTE_START_TRANSFER: ptr = 0; overflow[busIdx] = false; break;
        case U8X8_MSG_BYTE_SEND:
            if ((uint32_t)ptr + arg_int > ds::I2CPlatform::kPacketBufSize) { overflow[busIdx] = true; break; }
            memcpy(&buf[ptr], arg_ptr, arg_int);
            ptr += arg_int;
            break;
        case U8X8_MSG_BYTE_END_TRANSFER:
            if (overflow[busIdx]) { overflow[busIdx] = false; return 0; }
            i2c_master_transmit(plt->getDevHandle(busIdx), buf, ptr, pdMS_TO_TICKS(plt->txTimeoutMs()));
            break;
        default: return 0;
    }
    return 1;
}

extern "C" uint8_t ds_u8x8_byte_esp32_idf_0(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    return u8x8_byte_esp32_idf(u8x8, msg, arg_int, arg_ptr, 0);
}

extern "C" uint8_t ds_u8x8_byte_esp32_idf_1(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    return u8x8_byte_esp32_idf(u8x8, msg, arg_int, arg_ptr, 1);
}

/**
 * @brief SW 버스(화면 2·3)의 gpio 콜백
 * @details 실제 비트 전송은 ds_sw_i2c_fast의 byte 콜백이 레지스터로 직접 한다.
 *          여기서는 U8g2 초기화 경로의 핀 설정과 비트 조작만 처리한다.
 *          핀은 하드코딩하지 않고 u8x8->pins에서 읽는다.
 */
extern "C" uint8_t ds_u8x8_gpio_and_delay_esp32_c3_fast(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    const uint8_t scl = u8x8->pins[U8X8_PIN_I2C_CLOCK];
    const uint8_t sda = u8x8->pins[U8X8_PIN_I2C_DATA];
    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            pinMode(scl, OUTPUT_OPEN_DRAIN);
            pinMode(sda, OUTPUT_OPEN_DRAIN);
            gpio_pullup_en((gpio_num_t)scl);
            gpio_pullup_en((gpio_num_t)sda);
            break;
        case U8X8_MSG_GPIO_I2C_CLOCK:
            if (arg_int) GPIO.out_w1ts.val = (1u << scl);
            else GPIO.out_w1tc.val = (1u << scl);
            break;
        case U8X8_MSG_GPIO_I2C_DATA:
            if (arg_int) GPIO.out_w1ts.val = (1u << sda);
            else GPIO.out_w1tc.val = (1u << sda);
            break;
        default: break;
    }
    return 1;
}
