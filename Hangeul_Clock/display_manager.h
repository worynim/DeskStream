#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <U8g2lib.h>
#include <Wire.h>
#include "config.h"
#include <pgmspace.h>
#include <vector>
#include <map>
#include <utility>
#include "i2c_platform.h"
#include "config_manager.h"

class DisplayManager;
extern DisplayManager display;

// 외부 인터페이스 함수 포인터 선언 (i2c_platform.h로 이관됨)

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
    uint8_t u8g2_buffers[NUM_SCREENS][SCREEN_WIDTH * PAGES_PER_SCREEN]; 
    TaskHandle_t main_task_handle = nullptr;
    String lastTexts[4];

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

    // static void i2c_hw_task(void* arg); // i2c_platform.h로 이관됨

    void showLargeIP(IPAddress ip);
    void showButtonHelp();
    void showStatus(const String& msg);
    void recoverI2CBus(); // I2C 버스 및 디스플레이 복구 로직
    void playStartupMelody(); // 시작 멜로디 재생
    void addLog(const String& msg); // 누적 로그 표시
    void updateLastLog(const String& msg); // 마지막 로그 수정

private:
    uint32_t hw_i2c_error_count = 0;
    const uint32_t I2C_ERROR_THRESHOLD = 50; 
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
    String log_lines[4];
    int log_count = 0;
};

#endif
