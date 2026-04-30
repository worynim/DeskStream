#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"
#include "i2c_platform.h"
#include "engine.h"

// 4개의 디스플레이 인스턴스 (Hangeul_Clock과 동일한 병렬 제어 구성)
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_1(U8G2_R0, /* clock=*/ 255, /* data=*/ 255, /* reset=*/ U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_2(U8G2_R0, /* clock=*/ 255, /* data=*/ 255, /* reset=*/ U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_3(U8G2_R0, /* clock=*/ SW_SCL_PIN, /* data=*/ SW_SDA_PIN, /* reset=*/ U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_4(U8G2_R0, /* clock=*/ SW_SCL_PIN, /* data=*/ SW_SDA_PIN, /* reset=*/ U8X8_PIN_NONE);

U8G2* screens[NUM_SCREENS] = {&u8g2_1, &u8g2_2, &u8g2_3, &u8g2_4};

// U8g2 공유 버퍼 문제 방지를 위한 개별 메모리 버퍼 할당
uint8_t u8g2_buffers[NUM_SCREENS][1024];

// UI State variables
bool isPlaying = true;
int fpsMode = 1; // 0=5fps, 1=10fps, 2=20fps
int frameDelay = 100; // approx 10fps

uint32_t lastDebounceTime[4] = {0, 0, 0, 0};
bool lastFlickerState[4] = {HIGH, HIGH, HIGH, HIGH};
bool lastStableState[4] = {HIGH, HIGH, HIGH, HIGH};

void playStartupMelody() {
    int melody[] = {2093, 2637, 3136, 4186}; // Do-Mi-Sol-Do
    for (int i = 0; i < 4; i++) {
        tone(BUZZER_PIN, melody[i], 100);
        delay(120);
    }
    noTone(BUZZER_PIN);
}

void handleButtons() {
    uint32_t now = millis();
    int pins[] = {BTN1_PIN, BTN2_PIN, BTN3_PIN, BTN4_PIN};
    
    for (int i = 0; i < 4; i++) {
        bool reading = digitalRead(pins[i]);
        
        // 핀 상태가 변경되었으면 타이머 초기화 (바운싱 중)
        if (reading != lastFlickerState[i]) {
            lastDebounceTime[i] = now;
        }
        
        // 상태가 50ms 이상 유지되었으면 안정된 상태로 간주
        if ((now - lastDebounceTime[i]) > 50) {
            if (reading != lastStableState[i]) {
                lastStableState[i] = reading;
                
                // 안정된 상태가 LOW(눌림) 일 때 액션 실행
                if (lastStableState[i] == LOW) {
                    if (i == 0) { // BTN1: Play/Pause
                        isPlaying = !isPlaying;
                        tone(BUZZER_PIN, isPlaying ? 2000 : 1000, 50);
                    } else if (i == 1) { // BTN2: Step (only if paused)
                        if (!isPlaying) {
                            engine.computeNextGeneration();
                            mapEngineToBuffers();
                            pushParallel();
                            tone(BUZZER_PIN, 1500, 30);
                        }
                    } else if (i == 2) { // BTN3: Speed Toggle
                        fpsMode = (fpsMode + 1) % 3;
                        if (fpsMode == 0) frameDelay = 200; // 5fps
                        else if (fpsMode == 1) frameDelay = 100;  // 10fps
                        else frameDelay = 50;  // 20fps
                        tone(BUZZER_PIN, 1000 + fpsMode * 500, 50);
                    } else if (i == 3) { // BTN4: Reset Pattern
                        engine.randomize();
                        if (!isPlaying) {
                            mapEngineToBuffers();
                            pushParallel();
                        }
                        tone(BUZZER_PIN, 800, 100);
                    }
                }
            }
        }
        lastFlickerState[i] = reading;
    }
}

void pushParallel() {
    i2cPlatform.waitForSync(I2C_SYNC_TIMEOUT_MS);

    bool any_hw_dirty = false;
    for (int s = 0; s < 2; s++) {
        uint8_t* buf = screens[s]->getBufferPtr();
        for (int p = 0; p < PAGES_PER_SCREEN; p++) {
            bool page_dirty = false; int first_tile = -1, last_tile = -1;
            for (int t = 0; t < TILES_PER_PAGE; t++) {
                bool tile_dirty = false;
                for (int tx = 0; tx < 8; tx++) {
                    int idx = p * SCREEN_WIDTH + t * 8 + tx;
                    if (buf[idx] != i2cPlatform.getShadowData(s, idx)) { 
                        tile_dirty = true; 
                        i2cPlatform.setShadowData(s, idx, buf[idx]); 
                    }
                }
                if (tile_dirty) { if (first_tile == -1) first_tile = t; last_tile = t; page_dirty = true; }
            }
            if (page_dirty) { 
                i2cPlatform.preparePageUpdate(s, p, first_tile, last_tile - first_tile + 1);
                any_hw_dirty = true; 
            }
        }
    }
    if (any_hw_dirty) { i2cPlatform.notifyTransmission(); }
    
    for (int s = 2; s < 4; s++) {
        uint8_t* buf = screens[s]->getBufferPtr();
        for (int p = 0; p < PAGES_PER_SCREEN; p++) {
            int first_tile = -1, last_tile = -1; bool page_dirty = false;
            for (int t = 0; t < TILES_PER_PAGE; t++) {
                bool tile_dirty = false;
                for (int tx = 0; tx < 8; tx++) {
                    int idx = p * SCREEN_WIDTH + t * 8 + tx;
                    if (buf[idx] != i2cPlatform.getShadowData(s, idx)) { 
                        tile_dirty = true; 
                        i2cPlatform.setShadowData(s, idx, buf[idx]); 
                    }
                }
                if (tile_dirty) { if (first_tile == -1) first_tile = t; last_tile = t; page_dirty = true; }
            }
            if (page_dirty) screens[s]->updateDisplayArea(first_tile, p, last_tile - first_tile + 1, 1);
        }
    }
}

void mapEngineToBuffers() {
    const uint8_t* current_gen = engine.getCurrentBuffer();
    
    for (int s = 0; s < NUM_SCREENS; s++) {
        uint8_t* u8g2_buf = screens[s]->getBufferPtr();
        int x_offset = s * 128; 
        
        for (int p = 0; p < 8; p++) {
            for (int x = 0; x < 128; x++) {
                int global_x = x_offset + x;
                uint8_t page_data = 0;
                int byte_idx_base = (global_x >> 3);
                uint8_t bit_mask = (0x80 >> (global_x & 7));
                
                int y_base = p * 8;
                if (current_gen[(y_base    ) * 64 + byte_idx_base] & bit_mask) page_data |= 0x01;
                if (current_gen[(y_base + 1) * 64 + byte_idx_base] & bit_mask) page_data |= 0x02;
                if (current_gen[(y_base + 2) * 64 + byte_idx_base] & bit_mask) page_data |= 0x04;
                if (current_gen[(y_base + 3) * 64 + byte_idx_base] & bit_mask) page_data |= 0x08;
                if (current_gen[(y_base + 4) * 64 + byte_idx_base] & bit_mask) page_data |= 0x10;
                if (current_gen[(y_base + 5) * 64 + byte_idx_base] & bit_mask) page_data |= 0x20;
                if (current_gen[(y_base + 6) * 64 + byte_idx_base] & bit_mask) page_data |= 0x40;
                if (current_gen[(y_base + 7) * 64 + byte_idx_base] & bit_mask) page_data |= 0x80;
                
                u8g2_buf[p * 128 + x] = page_data;
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[System] Game of Life - Booting...");

    // 버튼 및 부저 초기화
    pinMode(BTN1_PIN, INPUT_PULLUP);
    pinMode(BTN2_PIN, INPUT_PULLUP);
    pinMode(BTN3_PIN, INPUT_PULLUP);
    pinMode(BTN4_PIN, INPUT_PULLUP);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    // I2C 플랫폼 초기화 (병렬 전송 Task 포함)
    Serial.println("[System] Initializing I2C Platform...");
    i2cPlatform.begin();

    // HW I2C 디스플레이 콜백 및 주소 설정 (u8g2_1, u8g2_2)
    u8g2_1.getU8x8()->byte_cb = u8x8_byte_esp32_idf_0;
    u8g2_2.getU8x8()->byte_cb = u8x8_byte_esp32_idf_1;
    u8g2_1.setI2CAddress(I2C_ADDR_HW_0 << 1);
    u8g2_2.setI2CAddress(I2C_ADDR_HW_1 << 1);

    // SW I2C 디스플레이 콜백 및 주소 설정 (u8g2_3, u8g2_4)
    u8g2_3.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;
    u8g2_4.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;
    u8g2_3.setI2CAddress(I2C_ADDR_HW_0 << 1);
    u8g2_4.setI2CAddress(I2C_ADDR_HW_1 << 1);

    Serial.println("[System] Initializing Displays...");
    const char* splash_texts[4] = {"Conway's", "GAME", "OF", "LIFE"};
    for (int i = 0; i < NUM_SCREENS; i++) {
        screens[i]->getU8g2()->tile_buf_ptr = u8g2_buffers[i]; // 독립 버퍼 매핑
        screens[i]->begin();
        screens[i]->setFlipMode(1); // 글자 및 화면 상하반전 (Hardware Flip)
        screens[i]->clearBuffer();
        if (i == 0) screens[i]->setFont(u8g2_font_ncenB14_tr);
        else screens[i]->setFont(u8g2_font_logisoso24_tr);
        
        int strWidth = screens[i]->getStrWidth(splash_texts[i]);
        int x = (128 - strWidth) / 2;
        int y = (i == 0) ? 40 : 45;
        
        screens[i]->setCursor(x, y);
        screens[i]->print(splash_texts[i]);
    }
    pushParallel(); // 병렬 전송으로 4개 화면 동시 출력
    
    playStartupMelody();
    delay(2000); // 스플래시 화면 2초 대기

    Serial.println("[System] Initializing Game Engine...");
    engine.init();

    Serial.println("[System] Setup Complete!");
}

void loop() {
    unsigned long loop_start = millis();

    handleButtons();

    if (isPlaying) {
        // unsigned long start_time = micros();
        
        engine.computeNextGeneration();
        mapEngineToBuffers();
        pushParallel();
        
        // unsigned long duration = micros() - start_time;
        // Serial.print("Generation & Render in: ");
        // Serial.print(duration);
        // Serial.println(" us");
    }
    
    // FPS 조절 대기 로직 (버튼 반응성을 위해 짧게 나눠서 대기)
    while (millis() - loop_start < frameDelay) {
        handleButtons();
        delay(5);
    }
}
