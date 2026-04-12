/**
 * @file display_manager.h
 * @brief 고수준 디스플레이 및 UI 스테이지 관리 클래스 정의
 * @details 4개 OLED에 대한 통합 렌더링, 시계/IP/도움말 화면 전환 및 애니메이션 트리거 관리
 */
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

/**
 * @brief 개별 화면의 애니메이션 연산을 위한 데이터 스냅샷
 */
struct ScreenAnimData {
    CharData oldChars[8];
    CharData newChars[8];
    int oldCount;
    int newCount;
    bool changed;
};

/**
 * @brief 전체 디스크레이 애니메이션 상태 제어 구조체
 */
struct AnimationState {
    bool active = false;
    uint8_t currentStep = 0;
    unsigned long lastUpdateMs = 0;
    ScreenAnimData screens[4];
};

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
    void updateTick(); // 비차단 애니메이션 진행을 위한 티커
    bool isAnimating() const { return _animState.active; }

    // 헬퍼 및 유틸리티
    void pushParallel();
    void showLargeIP(IPAddress ip);
    void showButtonHelp();
    void showStatus(const String& msg);
    void recoverI2CBus(); // I2C 버스 및 디스플레이 복구 로직
    void playStartupMelody(); // 시작 멜로디 재생

private:
    uint32_t hw_i2c_error_count = 0;
    const uint32_t I2C_ERROR_THRESHOLD = 50; 
    bool _needsForceUpdate = false;
    void (*on_yield_callback)() = nullptr;
    TimerHandle_t buzzerTimer = NULL;
    AnimationState _animState;
    
    void drawCenterText(int idx, const String& text, bool centered);
    void renderAnimFrame(int screenIdx, int step); // 단일 프레임 렌더링 내부 함수
};

#endif
