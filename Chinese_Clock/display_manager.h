// worynim@gmail.com
/**
 * @file display_manager.h
 * @brief 고수준 디스플레이 및 UI 스테이지 관리 클래스 정의
 * @details 4개 OLED에 대한 통합 렌더링, 시계/IP/도움말 화면 전환 및 애니메이션 트리거 관리
 * @note [SYNC] 원본: ENG_Clock/display_manager.h
 *       변경: (1) CharData(18자·2줄) → AnimCell(4자·1줄)
 *             (2) drawCenterText()의 singleLine 인자 제거 (중국어판은 항상 1줄)
 *             (3) setDateOrder() 제거 (날짜 순서 개념 없음 — PLAN §6.9)
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
 * @brief 애니메이션 대상 셀 1개
 * @details ENG판의 CharData에 해당한다. 다른 점은 두 가지뿐이다:
 *          - `line`이 없다 — 중국어판은 항상 1줄이므로 줄 번호가 정체성의 일부가 아니다.
 *          - `y`가 없다 — 세로 위치가 LINE_TOP_Y로 고정이다. **애니메이션 프레임에서의
 *            세로 이동은 그리기 호출 시 LINE_TOP_Y + 이동량으로 계산한다** (ENG판의 y_offset 승계).
 * @note String을 **값으로** 저장한다(포인터가 아니라). layoutLine()이 돌려주는 LayoutChar는
 *       원본 문자열을 가리키는 뷰이므로 그대로 저장하면 dangling pointer가 된다.
 *       소유하는 대가로 애니메이션 프레임 안에서 힙 할당이 0회다 (16프레임 × 4화면 재사용).
 */
struct AnimCell {
    String text;
    int16_t x;
};

/** @brief 개별 화면의 애니메이션 연산을 위한 데이터 스냅샷 */
struct ScreenAnimData {
    AnimCell oldChars[LAYOUT_MAX_CHARS];
    AnimCell newChars[LAYOUT_MAX_CHARS];
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
    void setHourFormat(uint8_t format);
    void setAnimMode(uint8_t mode);

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
    void setFontName(const String& name);

    /**
     * @brief **슬롯을 지정해서** 이름표를 쓴다 — "/f<slot>/name.txt"
     * @details setFontName()은 이름표를 "현재 슬롯(configManager.font_slot)"에 쓴다.
     *          그런데 글리프 업로드는 슬롯을 명시해서(?slot=N) 보내므로, 현재 슬롯이
     *          업로드 슬롯과 다르면 **이름표가 엉뚱한 폴더에** 남는다 — 글리프는 /f2로
     *          갔는데 이름표는 /f0에 쓰이는 식이다. 그 결과 웹 드롭다운에서 그 슬롯이
     *          "Empty Slot"으로 보인다 (2026-10-05 사용자 보고: 슬롯 0·2가 비어 보임).
     *          **업로드 경로는 반드시 이 함수를 쓴다.**
     * @param slot 0 ~ FONT_SLOT_COUNT-1 (범위 밖이면 false)
     * @param name 폰트 파일 이름. **setFontName()과 같은 검증**을 통과해야 한다
     *             (비어 있지 않고, 32바이트 미만, `"` `\` `/` 및 제어문자 없음).
     *             거부되면 false — 조용히 넘어가지 않는다(호출자가 로그를 남긴다).
     * @return 이름표가 그 슬롯에 있으면 true
     * @note 이미 같은 값이면 파일을 다시 쓰지 않는다(upload가 37회 부른다).
     *       **현재 슬롯에 쓸 때만** configManager.font_name도 갱신한다 — 다른 슬롯의
     *       이름을 바꾸는 것이 지금 화면에 뜬 폰트 표시를 흔들면 안 된다.
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

/**
     * @brief 현재 표시 방식 (简体/繁體/數字 중 하나)
     * @details 저장 필드 2개(script_type + display_mode)의 조합을 **하나의 값**으로 본다.
     *          BTN2 short 순환과 웹의 3단계 선택지가 이 값을 쓴다.
     */
    uint8_t presentation() const;

/**
     * @brief 표시 방식을 바꾼다 (简体 ↔ 繁體 ↔ 數字) — **슬롯은 건드리지 않는다**
     * @param p PRESENTATION_SIMPLIFIED / TRADITIONAL / NUMERIC
     * @details 한자 모드로는 그릴 글자가 있을 때만 들어간다 — 검증 근거는 cpp 참고.
     *          숫자 모드는 라틴 폴백 폰트만으로 그려지므로 **항상 허용**한다.
     *
     * @note 2026-10-05 사용자 보고로 바뀐 계약. 이전엔 문자판을 바꾸면 슬롯까지 옮겼고,
     *       그 결과 웹 UI의 "Storage slot"이 멋대로 바뀌어 보였으며, 대상 슬롯이 비었을
     *       때는 캐시가 통째로 비어 "숫자는 보이는데 时가 없다"가 되었다.
     */
    void setPresentation(uint8_t p);

private:
    /**
     * @brief 이 문자판에서만 쓰이는 글자 중 **지금 슬롯 폰트에 없는 첫째**를 찾는다
     * @return 없으면 nullptr (즉 그 문자판을 그릴 수 있다)
     * @details setPresentation()의 검증 하나를 담당한다. 슬롯을 바꾸지 않으므로 판정 기준은
     *          "지금 로드된 캐시에 있는가" 하나뿐 — 문자집합 상수도 FS 조회도 없다.
     */
    const char* missingGlyphFor(uint8_t script) const;

    bool _needsForceUpdate = false;
    String _slotNames[FONT_SLOT_COUNT];
    void (*on_yield_callback)() = nullptr;
    TimerHandle_t buzzerTimer = NULL;
    AnimationState _animState;

    /**
     * @brief 한 화면의 텍스트를 1줄 배치해 그리고 세로 중앙에 맞춘다
     * @details 배치 규칙은 순수 모듈 layout_engine의 layoutLine() 하나뿐이다.
     *          중국어판에는 `singleLine` 플래그가 없다 — PLAN §6.9가 ENG판의
     *          "어절 2개 이상 → 2줄" 규칙을 통째로 버린 것이 그 이유다.
     */
    void drawCenterText(int idx, const String& text);
    void renderAnimFrame(int screenIdx, int step); // 단일 프레임 렌더링 내부 함수
};

#endif