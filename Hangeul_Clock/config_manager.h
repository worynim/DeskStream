/**
 * @file config_manager.h
 * @brief 영속적 설정 관리 클래스 정의
 * @details 사용자 설정값의 저장(Flash), 로드 및 지능형 지연 저장(Lazy Save) 로직을 관리
 */
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include "config.h"

/**
 * @brief 시스템의 모든 비휘발성 설정을 담는 구조체
 */
struct SystemSettings {
    uint8_t anim_mode;
    uint8_t display_mode;
    uint8_t hour_format;
    bool is_flipped;
    bool chime_enabled;
    String font_name;

    // 기본값 설정
    SystemSettings() : 
        anim_mode(ANIMATION_TYPE_SCROLL_UP),
        display_mode(CLOCK_MODE_HANGUL),
        hour_format(HOUR_FORMAT_12H),
        is_flipped(true),
        chime_enabled(false),
        font_name("System Default") {}
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

    void load(); // 저장소에서 데이터 로드 (private)

    // 타이머 콜백에서 접근하기 위한 정적 함수
    static void _timerCallback(TimerHandle_t xTimer);
};

// 전역 인스턴스
extern ConfigManager configManager;

#endif
