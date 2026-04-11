#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <U8g2lib.h>
#include <Wire.h>
#include "config.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include "driver/i2c_master.h"
#include "LittleFS.h"
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <vector>
#include <map>
#include <pgmspace.h>

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

struct CharData {
    String c;
    int x;
};

// 부저 정지를 위한 콜백 전역/정적 함수
static void buzzerTimerCallback(TimerHandle_t xTimer) {
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW); // 마그네틱 부저용 잔류 전류 차단
}

// 시보 표시용 종 모양 아이콘 (8x8)
static const uint8_t bell_icon[] = { 0x18, 0x3C, 0x3C, 0x3C, 0xFF, 0xDB, 0x18, 0x00 };

struct CachedChar {
    String hex;
    uint8_t data[512]; 
    uint16_t dataSize = 0; // 실제 로드된 데이터 크기 (256: 32px, 512: 64px)
};

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
    String lastTexts[4];
    
    // 애니메이션 및 표시 상태 관리
    uint8_t anim_mode = ANIMATION_TYPE_SCROLL_UP;
    uint8_t display_mode = CLOCK_MODE_HANGUL;
    uint8_t hour_format = HOUR_FORMAT_12H;
    bool is_flipped = false;
    bool chime_enabled = false;
    String font_name = "System Default";
    Preferences prefs;

    // 외부 루프 양보 콜백 (애니메이션 중 버튼 처리용)
    void (*on_yield_callback)() = nullptr;

    // 비동기 부저 제어 (FreeRTOS 타이머)
    TimerHandle_t buzzerTimer = NULL;

    // 비트맵 캐시
    std::vector<CachedChar> bitmapCache;

    DisplayManager() : 
        u8g2_1(U8G2_R2, 255, 255, U8X8_PIN_NONE), u8g2_2(U8G2_R2, 255, 255, U8X8_PIN_NONE),
        u8g2_3(U8G2_R2, SW_SCL_PIN, SW_SDA_PIN, U8X8_PIN_NONE), u8g2_4(U8G2_R2, SW_SCL_PIN, SW_SDA_PIN, U8X8_PIN_NONE) {
        screens[0] = &u8g2_1; screens[1] = &u8g2_2; screens[2] = &u8g2_3; screens[3] = &u8g2_4;
    }

    void begin() {
        applyFlip(); // screens 배열 할당 최우선 수행 (Crash 방지)
        main_task_handle = xTaskGetCurrentTaskHandle();
        memset(shadow_buffer, 0xFF, sizeof(shadow_buffer));
        
        // I2C 버스 및 디바이스 초기화
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
        
        // 각 화면 시작
        u8g2_1.getU8x8()->byte_cb = u8x8_byte_esp32_idf_0; u8g2_1.begin();
        u8g2_2.getU8x8()->byte_cb = u8x8_byte_esp32_idf_1; u8g2_2.setI2CAddress(0x3D * 2); u8g2_2.begin();
        u8g2_3.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast; u8g2_3.begin();
        u8g2_4.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast; u8g2_4.setI2CAddress(0x3D * 2); u8g2_4.begin();
        
        // 설정 로드 (사용자 피드백 반영: 플립 기본값 true)
        prefs.begin("clock", false);
        anim_mode = prefs.getUChar("anim", ANIMATION_TYPE_SCROLL_UP);
        display_mode = prefs.getUChar("mode", CLOCK_MODE_HANGUL);
        hour_format = prefs.getUChar("format", HOUR_FORMAT_12H);
        is_flipped = prefs.getBool("flip", true); 
        chime_enabled = prefs.getBool("chime", false);
        font_name = prefs.getString("font_name", "System Default");
        
        xTaskCreatePinnedToCore(i2c_hw_task, "I2C_HW", HW_TASK_STACK, this, HW_TASK_PRIO, &hw_task_handle, HW_TASK_CORE);
        
        for (int i = 0; i < 4; i++) {
            screens[i]->setFlipMode(is_flipped); // 하드웨어-소프트웨어 플립 일치
            screens[i]->setBitmapMode(1);        // 투명 비트맵 모드 활성화 (v1.3.57) - 글자 겹침 허용
            screens[i]->clearBuffer();
            lastTexts[i] = "";
        }
        pushParallel();

        pinMode(BUZZER_PIN, OUTPUT);
        digitalWrite(BUZZER_PIN, LOW);
        buzzerTimer = xTimerCreate("BuzzerTimer", pdMS_TO_TICKS(50), pdFALSE, (void*)0, buzzerTimerCallback);

        loadBitmapCache();
    }

    void setFlipDisplay(bool flip) {
        is_flipped = flip;
        saveConfig(); // 모든 설정을 Preferences에 영구 저장
        for (int i = 0; i < 4; i++) {
            screens[i]->setFlipMode(is_flipped);
        }
    }

    void applyFlip() {
        // 배선 순서는 항상 I2C 1(HW), 2(HW), 3(SW), 4(SW) 고정
        screens[0] = &u8g2_1; screens[1] = &u8g2_2; screens[2] = &u8g2_3; screens[3] = &u8g2_4;
    }

    void setChime(bool enable) {
        chime_enabled = enable;
        saveConfig();
        extern unsigned long forceUpdateTrigger;
        forceUpdateTrigger = 1; // 기기에서 즉시 아이콘 반영
    }

    void setDisplayMode(uint8_t mode) {
        display_mode = mode;
        saveConfig();
    }

    void setHourFormat(uint8_t format) {
        hour_format = format;
        saveConfig();
    }

    void setAnimMode(uint8_t mode) {
        anim_mode = mode;
        saveConfig();
    }

    void setFontName(String name) {
        font_name = name;
        saveConfig();
    }

    void saveConfig() {
        prefs.putUChar("anim", anim_mode);
        prefs.putUChar("mode", display_mode);
        prefs.putUChar("format", hour_format);
        prefs.putBool("flip", is_flipped);
        prefs.putBool("chime", chime_enabled);
        prefs.putString("font_name", font_name);
        Serial.println("Config Saved to Preferences");
    }

    void loadBitmapCache() {
        bitmapCache.clear();
        File root = LittleFS.open("/");
        if (!root) return;
        File file = root.openNextFile();
        while (file) {
            String name = file.name();
            if (name.startsWith("c_") && name.endsWith(".bin")) {
                CachedChar cc;
                cc.hex = name.substring(2, name.length() - 4);
                memset(cc.data, 0, 512); // 노이즈 방지 초기화
                cc.dataSize = file.read(cc.data, 512); // 로드된 크기 기록 (v1.3.55)
                bitmapCache.push_back(cc);
            }
            file.close();
            file = root.openNextFile();
        }
        Serial.printf("[SYSTEM] Bitmap Cache Loaded: %d chars\n", bitmapCache.size());
    }

    // [잔상 제거] 모든 화면을 즉시 지우고 전송 버퍼를 초기화 (v1.3.48)
    void clearAll() {
        for (int i = 0; i < NUM_SCREENS; i++) {
            screens[i]->clearBuffer();
            screens[i]->sendBuffer(); // 하드웨어 즉시 물리 소거
            lastTexts[i] = "";        // 캐시 데이터 초기화
        }
        // 다음 pushParallel 시 모든 픽셀을 강제로 다시 그리게 함
        memset(shadow_buffer, 0xFF, sizeof(shadow_buffer));
    }
    void beep(int duration = 50, int freq = 3000) {
        if (buzzerTimer == NULL) return;
        tone(BUZZER_PIN, freq);
        xTimerChangePeriod(buzzerTimer, pdMS_TO_TICKS(duration), 0);
        xTimerStart(buzzerTimer, 0);
    }

    void setYieldCallback(void (*cb)()) {
        on_yield_callback = cb;
    }



    void drawSingleChar(int idx, const String& charStr, int x, int y_offset) {
        U8G2* u8g2 = screens[idx];
        String hexStr = "";
        for (int k = 0; k < charStr.length(); k++) {
            char buf[3]; sprintf(buf, "%02X", (unsigned char)charStr[k]); hexStr += buf;
        }

        // 1. 캐시에서 검색
        for (const auto& cc : bitmapCache) {
            if (cc.hex == hexStr) {
                // 규격 자동 판별 (v1.3.55)
                if (cc.dataSize <= 256) {
                    // 구형 32px 모드: x 위치 그대로 32px(4바이트) 출력
                    u8g2->drawBitmap(x, y_offset, 4, 64, cc.data);
                } else {
                    // 신규 64px 모드: -16px 오프셋으로 64px(8바이트) 출력 (겹침 허용)
                    u8g2->drawBitmap(x - 16, y_offset, 8, 64, cc.data);
                }
                return;
            }
        }

        // 2. 캐시 미스 시 표준 폰트 사용
        u8g2->setFont(HANGEUL_FONT);
        u8g2->drawUTF8(x + (32 - u8g2->getUTF8Width(charStr.c_str())) / 2, TEXT_Y_POS + y_offset, charStr.c_str());
    }

    void getCharData(const String& text, CharData outChars[8], int& count, bool centered, int screenIdx) {
        count = 0;
        if (text == "") return;
        int i = 0;
        while (i < text.length() && count < 8) {
            int len = 1; unsigned char c = (unsigned char)text[i];
            if (c < 0x80) len = 1; else if ((c & 0xE0) == 0xC0) len = 2; else if ((c & 0xE0) == 0xE0) len = 3; else if ((c & 0xF8) == 0xF0) len = 4;
            outChars[count].c = text.substring(i, i + len);
            i += len; count++;
        }
        
        if (centered) {
            int totalW = count * 32;
            int startX = (128 - totalW) / 2;
            for (int j = 0; j < count; j++) outChars[j].x = startX + (j * 32);
        } else {
            // 시, 분, 초 단위 고정 정렬
            outChars[count - 1].x = 96;
            int numChars = count - 1;
            if (numChars > 0) {
                int startX = (96 - (numChars * 32)) / 2;
                for (int j = 0; j < numChars; j++) {
                    outChars[j].x = startX + (j * 32);
                }
            }
        }
    }

    void drawCenterText(int idx, const String& text, bool centered) {
        screens[idx]->clearBuffer();
        CharData chars[8]; int count;
        getCharData(text, chars, count, centered, idx);
        for (int j = 0; j < count; j++) {
            drawSingleChar(idx, chars[j].c, chars[j].x, 0);
        }
        
        // [OVERLAY] 시보 벨 아이콘: 글자를 다 그린 후 마지막에 그 위에 덧그림
        if (centered && chime_enabled) {
            screens[idx]->drawXBM(0, 0, 8, 8, bell_icon);
        }
        // pushParallel()에서 전송하므로 개별 sendBuffer() 삭제 (v1.3.45)
    }

    void updateAll(String inTexts[4], bool force = false) {
        String texts[4];
        bool changed[4] = {false, false, false, false};
        bool anyChanged = false;
        
        // 사용자 지시 반영: 플립 ON 시 화면 순서 뒤집기
        for (int i = 0; i < 4; i++) {
            texts[i] = is_flipped ? inTexts[3 - i] : inTexts[i];
            // force가 true이면 내용 변경 여부와 상관없이 무조건 changed를 true로 설정
            if (force || texts[i] != lastTexts[i]) { changed[i] = true; anyChanged = true; }
        }
        if (!anyChanged) return;

        if (anim_mode == ANIMATION_TYPE_NONE) {
            for (int i = 0; i < 4; i++) {
                // 텍스트가 변했거나 강제 갱신(force)인 경우 그리기 수행
                if (changed[i]) { 
                    // 시선 기준 맨 왼쪽(User Normal=Flip ON 일 때 i=3)에 종 아이콘 및 중앙 정렬 적용
                    bool isTitleScreen = is_flipped ? (i == 3) : (i == 0);
                    drawCenterText(i, texts[i], isTitleScreen); 
                    lastTexts[i] = texts[i]; 
                }
            }
            pushParallel();
        } else if (anim_mode == ANIMATION_TYPE_SCROLL_UP) {
            CharData oldChars[4][8], newChars[4][8];
            int oldCount[4], newCount[4];
            for (int i = 0; i < 4; i++) {
                if (changed[i]) {
                    bool isCentered = is_flipped ? (i == 3) : (i == 0);
                    getCharData(lastTexts[i], oldChars[i], oldCount[i], isCentered, i);
                    getCharData(texts[i], newChars[i], newCount[i], isCentered, i);
                }
            }

            for (int step = 0; step <= 16; step++) {
                int offset = step * 4;
                for (int i = 0; i < 4; i++) {
                    if (!changed[i]) continue;
                    screens[i]->clearBuffer();
                    
                    // 1. 기존/신규 글자 먼저 렌더링 (배경)
                    for (int j = 0; j < newCount[i]; j++) {
                        String nc = newChars[i][j].c; int nx = newChars[i][j].x;
                        bool isStatic = false; String oc = "";
                        for (int k = 0; k < oldCount[i]; k++) {
                            if (oldChars[i][k].x == nx) {
                                oc = oldChars[i][k].c;
                                if (oc == nc) isStatic = true;
                                break;
                            }
                        }
                        if (isStatic) {
                            drawSingleChar(i, nc, nx, 0);
                        } else {
                            if (step < 16 && oc != "") drawSingleChar(i, oc, nx, -offset);
                            drawSingleChar(i, nc, nx, 64 - offset);
                        }
                    }
                    for (int k = 0; k < oldCount[i]; k++) {
                        int ox = oldChars[i][k].x; bool stillHasPos = false;
                        for (int j = 0; j < newCount[i]; j++) {
                            if (newChars[i][j].x == ox) { stillHasPos = true; break; }
                        }
                        if (!stillHasPos && step < 16) drawSingleChar(i, oldChars[i][k].c, ox, -offset);
                    }

                    // 2. [OVERLAY] 시보 아이콘을 글자 위에 마지막으로 덧그림
                    bool isIconScreen = is_flipped ? (i == 3) : (i == 0);
                    if (isIconScreen && chime_enabled) {
                        screens[i]->drawXBM(0, 0, 8, 8, bell_icon);
                    }
                }
                pushParallel();
                if (step < 16) {
                    unsigned long startDelay = millis();
                    while(millis() - startDelay < ANIMATION_STEP_DELAY_MS) {
                        if (on_yield_callback) on_yield_callback();
                        delay(1);
                    }
                }
            }
            for (int i = 0; i < 4; i++) if (changed[i]) lastTexts[i] = texts[i];
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

    void showLargeIP(IPAddress ip) {
        for (int i = 0; i < 4; i++) {
            screens[i]->clearBuffer();
            
            // 사용자 지시 반영: 플립 ON 시 IP 마디 순서 뒤집기
            int ip_idx = is_flipped ? (3 - i) : i;
            String segment = String(ip[ip_idx]);
            int charCount = segment.length();
            int totalW = charCount * 32;
            int startX = (128 - totalW) / 2;

            for (int j = 0; j < charCount; j++) {
                drawSingleChar(i, segment.substring(j, j + 1), startX + (j * 32), 0);
            }
            if (i < 3) screens[i]->drawBox(120, 56, 4, 4);

            // 제목 기준 화면도 데이터 순서에 맞춰 스왑
            bool isTitleScreen = is_flipped ? (i == 3) : (i == 0);
            if (isTitleScreen) {
                screens[i]->setFont(u8g2_font_4x6_tf);
                screens[i]->drawStr(0, 7, "SETTING ADDR");
            }
        }
        pushParallel(); // 태스크 병렬 전송 활용 (v1.3.45)
    }

    void showButtonHelp() {
        const char* titles[4] = {"BTN 1", "BTN 2", "BTN 3", "BTN 4"};
        
        char chimeStr[20], animStr[32];
        sprintf(chimeStr, "S:CHIME(%s)", chime_enabled ? "ON" : "OFF");
        sprintf(animStr, "S:ANIMATION MODE %d", anim_mode);
        const char* shorts[4] = {chimeStr, "S:NUM <> HAN", animStr, "S:NEXT PAGE"};
        
        char flipStr[16];
        sprintf(flipStr, "L:FLIP (%s)", is_flipped ? "ON" : "OFF");
        const char* longs[4]  = {flipStr,  "L:12/24", "L:-",  "L:-"};

        for (int i = 0; i < 4; i++) {
            screens[i]->clearBuffer();
            screens[i]->setFont(u8g2_font_7x14_tf);
            
            // 버튼 설명은 물리적 위치(i)와 1:1 매칭 (반전시키지 않음)
            screens[i]->drawStr(0, 15, titles[i]);
            
            // 기능 설명
            screens[i]->drawStr(0, 35, shorts[i]);
            screens[i]->drawStr(0, 55, longs[i]);
        }
        pushParallel(); // 태스크 병렬 전송 활용 (v1.3.45)
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
