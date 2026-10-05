// worynim@gmail.com
/**
 * @file config_manager.cpp
 * @brief 영속적 설정 관리 클래스 구현
 * @details Preferences(NVS)를 이용한 설정값 저장·로드 및 지능형 지연 저장
 * @note [SYNC] 원본: ENG_Clock/config_manager.cpp — dord 제거, script_type 추가·검증
 */
#include "config_manager.h"
#include <string.h>

ConfigManager configManager;

ConfigManager::ConfigManager() : _isDirty(false) {
    // 5,000ms 지연 저장용 소프트웨어 타이머 (Lazy Save)
    _saveTimer = xTimerCreate("SaveTimer", pdMS_TO_TICKS(5000), pdFALSE, (void*)this, _timerCallback);
}

void ConfigManager::begin() {
    Serial.println("[CONFIG] Initializing ConfigManager...");
    load();
}

void ConfigManager::load() {
    if (!_prefs.begin("clock", true)) {   // 읽기 전용으로 로드
        Serial.println("[CONFIG] Failed to open Preferences in RO mode, using defaults.");
        return;
    }

    _settings.anim_mode     = _prefs.getUChar("anim", ANIMATION_TYPE_SCROLL_UP);
    _settings.display_mode  = _prefs.getUChar("mode", CLOCK_MODE_WORD);
    _settings.hour_format   = _prefs.getUChar("format", HOUR_FORMAT_12H);
    _settings.is_flipped    = _prefs.getBool("flip", true);
    _settings.chime_enabled = _prefs.getBool("chime", false);
    _settings.is_inverted   = _prefs.getBool("inv", false);
    _settings.font_name     = _prefs.getString("font_name", "System Default");
    _settings.font_slot     = _prefs.getUChar("slot", 0);
    _settings.brightness    = _prefs.getUChar("bright", 1);
    _settings.script_type   = _prefs.getUChar("script", SCRIPT_SIMPLIFIED);

    // 값이 2개뿐인 enum이라 범위를 벗어나면 기본값으로 되돌린다 (NVS 수동 편집 대비).
    //   chinese_time의 toScript()도 방어적으로 정규화하지만, 여기서 한 번 막아두면
    //   NVS가 손상된 상태가 다음 저장에서 정상값으로 덮여써진다.
    if (_settings.script_type != SCRIPT_SIMPLIFIED && _settings.script_type != SCRIPT_TRADITIONAL) {
        Serial.printf("[CONFIG] Invalid script_type %u, falling back to Simplified.\n",
                      (unsigned)_settings.script_type);
        _settings.script_type = SCRIPT_SIMPLIFIED;
    }

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
    Serial.printf("[CONFIG] Loaded. TZ=%s Script=%s\n", _settings.timezone,
                  _settings.script_type == SCRIPT_TRADITIONAL ? "Traditional" : "Simplified");
}

void ConfigManager::setDirty() {
    _isDirty = true;
    if (_saveTimer != NULL) {
        xTimerReset(_saveTimer, 0);
        Serial.println("[CONFIG] Save requested (Lazy save in 5s...)");
    }
}

void ConfigManager::saveNow() {
    if (!_isDirty) return;

    // [ENG 리뷰 §1.4 계승] 검증은 **한 개라도 실패하면 전체를 건너뛴다.**
    //   일부만 새 값인 상태가 남으면 사용자는 "다른 설정은 반영됐는데 시간대만 옛 값인"
    //   상황을 만난다. 전체를 건너뛰고 _isDirty를 유지하므로 다음 setDirty()에서 재시도된다.
    if (!isValidTimezone(_settings.timezone)) {
        Serial.println("[CONFIG] Refusing to persist: invalid timezone. Settings left unchanged.");
        return;
    }
    if (_settings.script_type != SCRIPT_SIMPLIFIED && _settings.script_type != SCRIPT_TRADITIONAL) {
        Serial.println("[CONFIG] Refusing to persist: invalid script_type. Settings left unchanged.");
        return;
    }

    if (!_prefs.begin("clock", false)) {   // 읽기-쓰기 모드
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
    _prefs.putUChar("script", _settings.script_type);
    _prefs.putString("tz", _settings.timezone);

    _prefs.end();
    _isDirty = false;
    Serial.println("[CONFIG] All settings committed to Flash memory.");
}

void ConfigManager::resetToDefaults() {
    _settings = SystemSettings();   // 기본 생성자 호출
    setDirty();
    Serial.println("[CONFIG] Settings reset to default values.");
}

void ConfigManager::_timerCallback(TimerHandle_t xTimer) {
    ConfigManager* instance = (ConfigManager*)pvTimerGetTimerID(xTimer);
    if (instance != nullptr) {
        instance->saveNow();
    }
}