// worynim@gmail.com
/**
 * @file config_manager.cpp
 * @brief 영속적 설정 관리 클래스 구현
 * @details LittleFS 및 Preferences를 이용한 설정값 저장, 로드 및 지능형 지연 저장(Lazy Save) 기능 구현
 * @note [SYNC] 원본: Hangeul_Clock/config_manager.cpp — timezone 로드/저장 및 검증 추가
 */
#include "config_manager.h"
#include <string.h>

// 전역 인스턴스 정의
ConfigManager configManager;

ConfigManager::ConfigManager() : _isDirty(false) {
    // 5,000ms(5초) 지연 저장용 소프트웨어 타이머 생성
    _saveTimer = xTimerCreate("SaveTimer", pdMS_TO_TICKS(5000), pdFALSE, (void*)this, _timerCallback);
}

void ConfigManager::begin() {
    Serial.println("[CONFIG] Initializing ConfigManager...");
    load();
}

void ConfigManager::load() {
    if (!_prefs.begin("clock", true)) { // Read-only mode for loading
        Serial.println("[CONFIG] Failed to open Preferences in RO mode, using defaults.");
        return;
    }

    _settings.anim_mode = _prefs.getUChar("anim", ANIMATION_TYPE_SCROLL_UP);
    _settings.display_mode = _prefs.getUChar("mode", CLOCK_MODE_WORD);
    _settings.hour_format = _prefs.getUChar("format", HOUR_FORMAT_12H);
    _settings.is_flipped = _prefs.getBool("flip", true);
    _settings.chime_enabled = _prefs.getBool("chime", false);
    _settings.is_inverted = _prefs.getBool("inv", false);
    _settings.font_name = _prefs.getString("font_name", "System Default");
    _settings.font_slot = _prefs.getUChar("slot", 0);
    _settings.brightness = _prefs.getUChar("bright", 1);
    _settings.date_order = _prefs.getUChar("dord", DATE_ORDER_DAY_MONTH);
    // 값이 2개뿐인 enum이라 범위를 벗어나면 기본값으로 되돌린다 (NVS 수동 편집 대비).
    if (_settings.date_order > DATE_ORDER_MONTH_DAY) _settings.date_order = DATE_ORDER_DAY_MONTH;

    // NVS에 저장된 값도 신뢰하지 않는다. 스프라이스/수동 편집된 NVS일 수 있다.
    char tz[TIMEZONE_MAX_LEN];
    strncpy(tz, _prefs.getString("tz", DEFAULT_TIMEZONE).c_str(), sizeof(tz) - 1);
    tz[sizeof(tz) - 1] = '\0';
    if (isValidTimezone(tz)) {
        strncpy(_settings.timezone, tz, sizeof(_settings.timezone) - 1);
        _settings.timezone[sizeof(_settings.timezone) - 1] = '\0';
    } else {
        Serial.println("[CONFIG] Stored timezone invalid, falling back to default.");
        strncpy(_settings.timezone, DEFAULT_TIMEZONE, sizeof(_settings.timezone) - 1);
        _settings.timezone[sizeof(_settings.timezone) - 1] = '\0';
    }

    _prefs.end();
    Serial.printf("[CONFIG] Settings loaded. TZ=%s\n", _settings.timezone);
}

void ConfigManager::setDirty() {
    _isDirty = true;
    if (_saveTimer != NULL) {
        // 타이머를 재시작/리셋 (이미 실행 중이면 3초 대기 시간이 초기화됨)
        xTimerReset(_saveTimer, 0);
        Serial.println("[CONFIG] Save requested (Lazy save in 3s...)");
    }
}

void ConfigManager::saveNow() {
    if (!_isDirty) return;

    if (!_prefs.begin("clock", false)) { // Read-write mode
        Serial.println("[CONFIG] Error: Could not open Preferences for writing!");
        return;
    }

    _prefs.putUChar("anim", _settings.anim_mode);
    _prefs.putUChar("mode", _settings.display_mode);
    _prefs.putUChar("format", _settings.hour_format);
    _prefs.putBool("flip", _settings.is_flipped);
    _prefs.putBool("chime", _settings.chime_enabled);
    _prefs.putBool("inv", _settings.is_inverted);
    _prefs.putString("font_name", _settings.font_name);
    _prefs.putUChar("slot", _settings.font_slot);
    _prefs.putUChar("bright", _settings.brightness);
    _prefs.putUChar("dord", _settings.date_order);
    // setenv()로 나가는 값이므로 검증 통과한 문자열만 저장한다.
    if (isValidTimezone(_settings.timezone)) {
        _prefs.putString("tz", _settings.timezone);
    } else {
        Serial.println("[CONFIG] Refusing to persist invalid timezone.");
    }

    _prefs.end();
    _isDirty = false;
    Serial.println("[CONFIG] All settings committed to Flash memory.");
}

void ConfigManager::resetToDefaults() {
    _settings = SystemSettings(); // 구조체 초기화 (기본 생성자 호출)
    setDirty();
    Serial.println("[CONFIG] Settings reset to default values.");
}

void ConfigManager::_timerCallback(TimerHandle_t xTimer) {
    // 타이머 ID에서 클래스 인스턴스 포인터를 복구
    ConfigManager* instance = (ConfigManager*)pvTimerGetTimerID(xTimer);
    if (instance != nullptr) {
        instance->saveNow();
    }
}
