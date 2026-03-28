#include "DisplayEngine.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// (핀 설정 및 상수는 DisplayEngine.h에 정의됨)

// U8g2 객체 생성
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_1(U8G2_R0, 255, 255, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_2(U8G2_R0, 255, 255, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_3(U8G2_R0, SW_SCL_PIN, SW_SDA_PIN, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_4(U8G2_R0, SW_SCL_PIN, SW_SDA_PIN, U8X8_PIN_NONE);

// 정적 스크린 배열 초기화
U8G2* DisplayEngine::screens[NUM_SCREENS] = { &u8g2_1, &u8g2_2, &u8g2_3, &u8g2_4 };

// 메인 태스크 핸들 저장용 (태스크 간 동기화)
static TaskHandle_t main_task_h = NULL;

// 하드웨어 OLED 1 전용 콜백 (버스 0, 장치 0)
extern "C" uint8_t u8x8_byte_esp32_idf_0(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch(msg) {
        case U8X8_MSG_BYTE_INIT: break;
        case U8X8_MSG_BYTE_START_TRANSFER: display.i2c_packet_ptr[0] = 0; break;
        case U8X8_MSG_BYTE_SEND:
            memcpy(&display.i2c_packet_buf[0][display.i2c_packet_ptr[0]], arg_ptr, arg_int);
            display.i2c_packet_ptr[0] += arg_int;
            break;
        case U8X8_MSG_BYTE_END_TRANSFER:
            i2c_master_transmit(display.i2c_dev_handle[0], display.i2c_packet_buf[0], display.i2c_packet_ptr[0], pdMS_TO_TICKS(I2C_TX_TIMEOUT_MS));
            break;
        default: return 0;
    }
    return 1;
}

// 하드웨어 OLED 2 전용 콜백 (버스 0, 장치 1)
extern "C" uint8_t u8x8_byte_esp32_idf_1(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch(msg) {
        case U8X8_MSG_BYTE_INIT: break;
        case U8X8_MSG_BYTE_START_TRANSFER: display.i2c_packet_ptr[1] = 0; break;
        case U8X8_MSG_BYTE_SEND:
            memcpy(&display.i2c_packet_buf[1][display.i2c_packet_ptr[1]], arg_ptr, arg_int);
            display.i2c_packet_ptr[1] += arg_int;
            break;
        case U8X8_MSG_BYTE_END_TRANSFER:
            i2c_master_transmit(display.i2c_dev_handle[1], display.i2c_packet_buf[1], display.i2c_packet_ptr[1], pdMS_TO_TICKS(I2C_TX_TIMEOUT_MS));
            break;
        default: return 0;
    }
    return 1;
}

// 고속 SW I2C 콜백 (딜레이 제거)
extern "C" uint8_t u8x8_gpio_and_delay_esp32_c3_fast(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            pinMode(u8x8->pins[U8X8_PIN_I2C_CLOCK], OUTPUT_OPEN_DRAIN);
            pinMode(u8x8->pins[U8X8_PIN_I2C_DATA], OUTPUT_OPEN_DRAIN);
            // 내부 풀업 직접 활성화
            gpio_pullup_en((gpio_num_t)SW_SCL_PIN);
            gpio_pullup_en((gpio_num_t)SW_SDA_PIN);
            break;
        case U8X8_MSG_DELAY_100NANO: break;
        case U8X8_MSG_DELAY_10MICRO: break;
        case U8X8_MSG_DELAY_MILLI: break;
        case U8X8_MSG_GPIO_I2C_CLOCK:
            if (arg_int) GPIO.out_w1ts.val = (1 << SW_SCL_PIN);
            else         GPIO.out_w1tc.val = (1 << SW_SCL_PIN);
            break;
        case U8X8_MSG_GPIO_I2C_DATA:
            if (arg_int) GPIO.out_w1ts.val = (1 << SW_SDA_PIN);
            else         GPIO.out_w1tc.val = (1 << SW_SDA_PIN);
            break;
        default: u8x8_SetGPIOResult(u8x8, 1); break;
    }
    return 1;
}


DisplayEngine::DisplayEngine() {}

void DisplayEngine::begin() {
    main_task_h = xTaskGetCurrentTaskHandle();

    // 1. ESP-IDF I2C 마스터 버스 구성 (HW SDA 5, SCL 6)
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = (gpio_num_t)HW_SDA_PIN;
    bus_cfg.scl_io_num = (gpio_num_t)HW_SCL_PIN;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = I2C_GLITCH_FILTER;
    bus_cfg.intr_priority = I2C_INTR_PRIO; 
    bus_cfg.flags.enable_internal_pullup = true; // 내부 풀업 활성화 (HW SDA: 5, SCL: 6)

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus_handle));

    // 2. I2C 장치 등록 (0x3C, 0x3D)
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = 0x3C;
    dev_cfg.scl_speed_hz = I2C_SPEED_HZ; // 최고속도 구현
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &i2c_dev_handle[0]));
    
    dev_cfg.device_address = 0x3D;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &i2c_dev_handle[1]));

    // 3. 하드웨어 OLED (1, 2) - 콜백 설정 후 초기화
    u8g2_1.getU8x8()->byte_cb = u8x8_byte_esp32_idf_0;
    u8g2_2.getU8x8()->byte_cb = u8x8_byte_esp32_idf_1;
    u8g2_1.begin();
    // ★ begin()이 byte_cb를 리셋할 수 있으므로, begin() 후 반드시 재설정
    u8g2_1.getU8x8()->byte_cb = u8x8_byte_esp32_idf_0;
    u8g2_2.setI2CAddress(0x3D * 2);
    u8g2_2.begin();
    u8g2_2.getU8x8()->byte_cb = u8x8_byte_esp32_idf_1;
    u8g2_2.setI2CAddress(0x3D * 2);
    
    // 4. 소프트웨어 OLED (3, 4) 초기화
    u8g2_3.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;
    u8g2_4.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;
    u8g2_3.begin();
    u8g2_3.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;
    u8g2_4.setI2CAddress(0x3D * 2);
    u8g2_4.begin();
    u8g2_4.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;
    u8g2_4.setI2CAddress(0x3D * 2);

    canvas.setColorDepth(1);
    canvas.createSprite(CANVAS_WIDTH, CANVAS_HEIGHT);
    canvas.setBitmapColor(COLOR_WHITE, COLOR_BLACK);
    memset(shadow_buffer, 0, sizeof(shadow_buffer));

    // 4. 하드웨어 전송 전용 고순위 태스크 생성 (Core 0 고정)
    // Canvas가 준비된 후 태스크를 생성해야 안전합니다.
    xTaskCreatePinnedToCore(i2c_hw_task, "I2C_HW", HW_TASK_STACK, this, HW_TASK_PRIO, &hw_task_handle, HW_TASK_CORE);
}

void DisplayEngine::pushCanvas() {
    // [Step 0] 이전 프레임의 HW 전송이 진행 중이라면 여기서 대기 (동기화 및 데이터 보호)
    if (is_transmitting_hw && hw_task_handle) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(I2C_SYNC_TIMEOUT_MS));
        is_transmitting_hw = false;
    }

    uint32_t transmit_start = micros();
    frame_counter++;
    uint8_t* canvas_ptr = (uint8_t*)canvas.getBuffer();
    if (!canvas_ptr) return;

    // [리팩토링] 더티 체킹(Dirty Checking) 중복 로직을 람다(Lambda)로 캡슐화
    auto extractAndCheckDirty = [&](int s, int p, uint8_t* page_cache, int& first_tile, int& last_tile) -> bool {
        bool page_dirty = false;
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            int gx = (s * SCREEN_WIDTH) + x;
            int byte_idx = (gx >> 3); 
            int bit_mask = 0x80 >> (gx & 7);
            uint8_t out_byte = 0;
            for (int b = 0; b < 8; b++) {
                if (canvas_ptr[((p << 3) + b) * (CANVAS_WIDTH / 8) + byte_idx] & bit_mask) out_byte |= (1 << b);
            }
            page_cache[x] = out_byte;
            // 섀도우 버퍼(이전 데이터)와 비교
            if (page_cache[x] != shadow_buffer[s][p * SCREEN_WIDTH + x]) page_dirty = true;
        }

        first_tile = -1; last_tile = -1;
        if (page_dirty) {
            for (int t = 0; t < TILES_PER_PAGE; t++) {
                bool tile_dirty = false;
                for (int tx = 0; tx < 8; tx++) {
                    if (page_cache[t * 8 + tx] != shadow_buffer[s][p * SCREEN_WIDTH + t * 8 + tx]) {
                        tile_dirty = true; break;
                    }
                }
                if (tile_dirty) {
                    if (first_tile == -1) first_tile = t;
                    last_tile = t;
                }
            }
        }
        return first_tile != -1;
    };

    // [Step 1] 하드웨어용 데이터 준비 (화면 0, 1) - 이 시점에는 이전 전송이 완료되어 버퍼가 안전함
    bool any_hw_dirty = false;
    for (int s = 0; s < (NUM_SCREENS / 2); s++) {
        hw_dirty_mask[s] = 0;
        U8G2* screen = DisplayEngine::screens[s];
        uint8_t* u8g2_buf = screen->getBufferPtr();
        
        for (int p = 0; p < PAGES_PER_SCREEN; p++) {
            uint8_t page_cache[128];
            int first_tile, last_tile;
            if (extractAndCheckDirty(s, p, page_cache, first_tile, last_tile)) {
                hw_dirty_mask[s] |= (1 << p);
                hw_first_tile[s][p] = (uint8_t)first_tile;
                hw_tile_count[s][p] = (uint8_t)(last_tile - first_tile + 1);
                memcpy(&shadow_buffer[s][p * SCREEN_WIDTH], page_cache, SCREEN_WIDTH);
                memcpy(&u8g2_buf[p * SCREEN_WIDTH], page_cache, SCREEN_WIDTH);
                any_hw_dirty = true;
            }
        }
    }

    // [Step 2] HW 태스크 전송 비동기 시작
    if (hw_task_handle && any_hw_dirty) {
        is_transmitting_hw = true;
        xTaskNotifyGive(hw_task_handle);
    }

    // [Step 3] SW I2C 처리 (HW 태스크가 배경에서 돌 때 메인 태스크에서 비트뱅잉 수행)
    for (int s = (NUM_SCREENS / 2); s < NUM_SCREENS; s++) {
        uint8_t* u8g2_buf = DisplayEngine::screens[s]->getBufferPtr();
        for (int p = 0; p < PAGES_PER_SCREEN; p++) {
            uint8_t page_cache[128];
            int first_tile, last_tile;
            if (extractAndCheckDirty(s, p, page_cache, first_tile, last_tile)) {
                memcpy(&shadow_buffer[s][p * SCREEN_WIDTH], page_cache, SCREEN_WIDTH);
                memcpy(&u8g2_buf[p * SCREEN_WIDTH], page_cache, SCREEN_WIDTH);
                int tile_count = last_tile - first_tile + 1;
                DisplayEngine::screens[s]->updateDisplayArea(first_tile, p, tile_count, 1);
            }
        }
    }

    // [Step 4] HW 전송이 완료될 때까지 "기다리지 않고" 그대로 리턴하여 FFT 연산으로 넘어감
    last_transmit_time = micros() - transmit_start;
}

// 백그라운드 하드웨어 전송 태스크 (U8g2 완전 미사용 - 직접 SSD1306 프로토콜)
void DisplayEngine::i2c_hw_task(void* arg) {
    DisplayEngine* engine = (DisplayEngine*)arg;
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        for (int s = 0; s < (NUM_SCREENS / 2); s++) {
            uint8_t mask = engine->hw_dirty_mask[s];
            if (mask == 0) continue;
            i2c_master_dev_handle_t dev_h = engine->i2c_dev_handle[s];
            for (int p = 0; p < PAGES_PER_SCREEN; p++) {
                if (!(mask & (1 << p))) continue;
                uint8_t ft = engine->hw_first_tile[s][p];
                uint8_t tc = engine->hw_tile_count[s][p];
                uint8_t col = ft * 8;
                uint8_t len = tc * 8;
                // SSD1306 페이지/컬럼 주소 설정
                uint8_t cmd[] = {0x00, (uint8_t)(0xB0|p), (uint8_t)(col & 0x0F), (uint8_t)(0x10|(col>>4))};
                i2c_master_transmit(dev_h, cmd, 4, pdMS_TO_TICKS(I2C_CMD_TIMEOUT_MS));
                // 픽셀 데이터 전송
                uint8_t tx[129];
                tx[0] = 0x40;
                memcpy(&tx[1], &engine->shadow_buffer[s][p*SCREEN_WIDTH+col], len);
                i2c_master_transmit(dev_h, tx, len+1, pdMS_TO_TICKS(I2C_TX_TIMEOUT_MS));
            }
        }
        if (main_task_h) xTaskNotifyGive(main_task_h);
    }
}

// 1/4 전송 기능은 병합된 pushCanvas로 대체됨 (삭제)

void DisplayEngine::smartDelay(uint32_t ms) {
    uint32_t start_ms = millis();
    while (millis() - start_ms < ms) {
        pushCanvas();
        delay(1);
    }
}

DisplayEngine display;
