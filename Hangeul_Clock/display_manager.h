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

// 외부 인터페이스 함수 포인터 선언
extern "C" uint8_t u8x8_byte_esp32_idf_0(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
extern "C" uint8_t u8x8_byte_esp32_idf_1(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
extern "C" uint8_t u8x8_gpio_and_delay_esp32_c3_fast(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr);

struct CharData {
    String c;
    int x;
};

struct CachedChar {
    String hex;
    std::vector<uint8_t> data;
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
    
    uint8_t anim_mode = ANIMATION_TYPE_SCROLL_UP;
    uint8_t display_mode = CLOCK_MODE_HANGUL;
    uint8_t hour_format = HOUR_FORMAT_12H;
    bool is_flipped = false;
    bool chime_enabled = false;
    String font_name = "System Default";
    Preferences prefs;

    DisplayManager();

    void begin();
    void setFlipDisplay(bool flip);
    void applyFlip();
    void setChime(bool enable);
    void setForceUpdate(bool force);
    bool checkForceUpdate();
    void setDisplayMode(uint8_t mode);
    void setHourFormat(uint8_t format);
    void setAnimMode(uint8_t mode);
    void setFontName(String name);
    void saveConfig();
    void loadBitmapCache();
    void clearAll();
    void beep(int duration = 50, int freq = 3000);
    void setYieldCallback(void (*cb)());
    void updateAll(String inTexts[4], bool force = false);

    // 내부 헬퍼 메서드
    const CachedChar* findChar(const String& s);
    String getHexKey(const String& s);
    void drawSingleChar(int idx, const String& charStr, int x, int y_offset = 0);
    void drawDitheredChar(int idx, const String& charStr, int x, int density);
    void drawZoomedChar(int idx, const String& charStr, int x, int scale_percent);
    void drawScaledChar(int idx, const String& charStr, int x, int h);
    void getCharData(const String& text, CharData outChars[8], int& count, bool centered, int screenIdx);
    void drawCenterText(int idx, const String& text, bool centered);
    void pushParallel();

    static void i2c_hw_task(void* arg);

    void showLargeIP(IPAddress ip);
    void showButtonHelp();
    void showStatus(const String& msg);

private:
    bool _needsForceUpdate = false;
    void (*on_yield_callback)() = nullptr;
    TimerHandle_t buzzerTimer = NULL;
    std::vector<CachedChar> bitmapCache;
    std::map<String, int> cacheIndex;
    const uint8_t bayer_matrix[4][4] = {
        { 0,  8,  2, 10},
        {12,  4, 14,  6},
        { 3, 11,  1,  9},
        {15,  7, 13,  5}
    };
};

#endif
