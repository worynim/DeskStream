/**
 * @file config_manager.cpp
 * @brief 영속적 설정 관리 클래스 구현
 * @details LittleFS 및 Preferences를 이용한 설정값 저장, 로드 및 지능형 지연 저장(Lazy Save) 기능 구현
 */
#include "config_manager.h"

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
    _settings.display_mode = _prefs.getUChar("mode", CLOCK_MODE_HANGUL);
    _settings.hour_format = _prefs.getUChar("format", HOUR_FORMAT_12H);
    _settings.is_flipped = _prefs.getBool("flip", true);
    _settings.chime_enabled = _prefs.getBool("chime", false);
    _settings.is_inverted = _prefs.getBool("inv", false);
    _settings.font_name = _prefs.getString("font_name", "System Default");
    _settings.font_slot = _prefs.getUChar("slot", 0);

    _prefs.end();
    Serial.println("[CONFIG] Settings loaded from Preferences.");
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
