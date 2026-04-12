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
#include "renderer.h"

class DisplayManager;
extern DisplayManager display;

// 외부 인터페이스 함수 포인터 선언 (i2c_platform.h로 이관됨)

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
    void loadBitmapCache();
    void clearAll();
    void beep(int duration = 50, int freq = 3000);
    void setYieldCallback(void (*cb)());
    void updateAll(String inTexts[4], bool force = false);

    // 헬퍼 및 유틸리티
    void pushParallel();
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
    
    String log_lines[4];
    int log_count = 0;
    
    void drawCenterText(int idx, const String& text, bool centered);
};

#endif
