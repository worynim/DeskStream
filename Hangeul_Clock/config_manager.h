// worynim@gmail.com
/**
 * @file config_manager.h
 * @brief 영속적 설정 관리 클래스 정의
 * @details 사용자 설정값의 저장(Flash), 로드 및 지능형 지연 저장(Lazy Save) 로직을 관리
 * @note [SYNC] ENG_Clock/config_manager.h — timezone 필드 추가
 */
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <string.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include "config.h"
#include "tz_util.h"

/**
 * @brief 시스템의 모든 비휘발성 설정을 담는 구조체
 */
struct SystemSettings {
    uint8_t anim_mode;
    uint8_t display_mode;
    uint8_t hour_format;
    bool is_flipped;
    bool chime_enabled;
    bool is_inverted;
    String font_name;
    uint8_t font_slot;
    uint8_t brightness;

    // POSIX TZ 문자열 (예: "KST-9", "EST5EDT,M3.2.0,M11.1.0").
    // setenv("TZ", ...)로 직접 사용되므로 웹 입력 검증이 필수다.
    char timezone[TIMEZONE_MAX_LEN];

    // 기본값 설정
    // [2026-11-04] is_flipped 기본값을 true → false로 교정했다.
    //   이 값은 하드웨어 flip(setFlipMode)과 화면 배정(어느 OLED가 제목인가) 두 곳에 쓰인다.
    //   R2가 이미 180°를 적용하므로, setFlipMode(false)(= u8g2 초기 미러 0xa1/0xc8 유지)라야
    //   상쇄되어 정방향이 된다. 같은 보드의 다른 프로젝트(Universal_Timer 등)는 setFlipMode를
    //   아예 부르지 않아 초기값 0xa1/0xc8 그대로 = 정방향이다. 즉 Hangeul_Clock만 true 때문에
    //   180° 뒤집혀 있었다. 자세한 근거는 config_manager.cpp의 load() 주석 참조.
    SystemSettings() :
        anim_mode(ANIMATION_TYPE_SCROLL_UP),
        display_mode(CLOCK_MODE_HANGUL),
        hour_format(HOUR_FORMAT_12H),
        is_flipped(false),
        chime_enabled(false),
        is_inverted(false),
        font_name("System Default"),
        font_slot(0),
        brightness(1) {
        strncpy(timezone, DEFAULT_TIMEZONE, sizeof(timezone) - 1);
        timezone[sizeof(timezone) - 1] = '\0';
    }
};

/**
 * @brief 설정 로드, 저장 및 지연 저장(Lazy Save)을 관리하는 클래스
 */
class ConfigManager {
public:
    ConfigManager();
    
    // 초기화 및 데이터 로드
    void begin();
    
    // 설정 가져오기 (참조 반환)
    SystemSettings& get() { return _settings; }
    
    // 변경 사항 알림 (3초 후 자동 저장 예약)
    void setDirty();
    
    // 즉시 저장 (전원 종료 전 등 특수 상황)
    void saveNow();
    
    // 기본값으로 초기화
    void resetToDefaults();

private:
    SystemSettings _settings;
    Preferences _prefs;
    TimerHandle_t _saveTimer;
    bool _isDirty;

    // 이번 부팅에서 방향 기본값 교정(true → false)이 일어났는가.
    //   load()에서 세우고 saveNow()가 소비한다 — false로 남은 값과 "아직 마이그레이션 안 된
    //   옛 기기"를 구분하려면 NVS에 표식(flip_mig)을 남겨야 하므로 저장 시점까지 들고 간다.
    bool _flipMigrated;

    void load(); // 저장소에서 데이터 로드 (private)

    // 타이머 콜백에서 접근하기 위한 정적 함수
    static void _timerCallback(TimerHandle_t xTimer);
};

// 전역 인스턴스
extern ConfigManager configManager;

#endif
