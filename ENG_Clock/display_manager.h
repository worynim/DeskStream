// worynim@gmail.com
/**
 * @file display_manager.h
 * @brief 고수준 디스플레이 및 UI 스테이지 관리 클래스 정의
 * @details 4개 OLED에 대한 통합 렌더링, 시계/IP/도움말 화면 전환 및 애니메이션 트리거 관리
 * @note [SYNC] 원본: Hangeul_Clock/display_manager.h — CharData 버퍼를 8→18로 확대,
 *       그리고 2줄 레이아웃(line 필드)을 반영
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
#include "layout_engine.h"   // LAYOUT_MAX_CHARS

/**
 * @brief 개별 화면의 애니메이션 연산을 위한 데이터 스냅샷
 */
struct ScreenAnimData {
    // 원본은 한글 최대 4자(8칸)였으나 영어는 2줄 × 9자 = 18자까지 필요하다.
    CharData oldChars[LAYOUT_MAX_CHARS];
    CharData newChars[LAYOUT_MAX_CHARS];
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
    String lastTexts[4];

    DisplayManager();

    void begin();
    void setFlipDisplay(bool flip);
    void setChime(bool enable);
    void setForceUpdate(bool force);
    void setFontSlot(uint8_t slot);
    void setInversion(bool invert);
    void setBrightness(uint8_t brightness);
    void refreshNow();
    String getSlotName(uint8_t slot);
    bool checkForceUpdate();
    void setDisplayMode(uint8_t mode);
    void setHourFormat(uint8_t format);
    void setAnimMode(uint8_t mode);
    void setDateOrder(uint8_t order);

    /**
     * @brief 설정된 POSIX TZ를 환경변수에 적용하고 NTP를 재동기화한다
     * @details lwIP는 UTC를 내부 보관하고 localtime() 호출 시점에 TZ를 적용한다.
     *          configTzTime()이 setenv("TZ", tz) + tzset()까지 해 준다.
     *          configTime(오프셋, ...)은 절대 쓰지 않는다 — 코어가 오프셋을
     *          POSIX TZ로 강제 변환해 설정값을 덮어써서 항상 UTC가 된다.
     *
     * @note 입력 검증은 config_manager(로드)와 web_manager(입력)가 이미 수행한다
     *       (둘 다 tz_util::isValidTimezone). 여기서는 검증된 값만 사용한다.
     */
    void applyTimezone();
    /**
     * @brief 현재 슬롯의 이름표를 바꾼다 (웹 /api/config 의 font_name)
     * @note 실제 쓰기는 setSlotName()이 한다. **업로드 경로는 이 함수를 쓰면 안 된다** —
     *       업로드 슬롯과 현재 슬롯이 다르면 이름표가 엉뚱한 폴더로 간다.
     */
    void setFontName(const String& name);

    /**
     * @brief 이름표를 **지정한 슬롯**에 쓴다 — 파일·캐시·(현재 슬롯이면) config까지
     * @details [A-2③ 수정 — 2026-10-06, 중국어판 §12.13 승계] 검증과 파일 쓰기가 여기
     *          한 곳에 모여 있다. 웹 업로드 경로(`?slot=N&font=…`)는 반드시 이 함수를 쓴다 —
     *          setFontName()은 장치의 **현재** 슬롯(font_slot)에 쓰므로, 업로드 슬롯과
     *          다르면 글리프는 /f1에, 이름표는 /f0에 남아 드롭다운이 "Empty Slot"이 된다.
     * @return false면 거부(빈 이름·길이 초과·금지문자·파일 열기 실패) — 호출자가 로그를 남긴다
     */
    bool setSlotName(uint8_t slot, const String& name);
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
    void playStartupMelody(); // 시작 멜로디 재생
    void playChimeMelody(); // 시보 멜로디 재생

private:
    bool _needsForceUpdate = false;
    String _slotNames[FONT_SLOT_COUNT];
    void (*on_yield_callback)() = nullptr;
    TimerHandle_t buzzerTimer = NULL;
    AnimationState _animState;
    
    // [수정할 사항 1] singleLine=true면 "어절 2개 이상 → 2줄" 규칙을 건너뛴다 (숫자 모드 "02 H")
    void drawCenterText(int idx, const String& text, bool singleLine);
    void renderAnimFrame(int screenIdx, int step); // 단일 프레임 렌더링 내부 함수
};

#endif
