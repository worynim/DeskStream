// worynim@gmail.com
/**
 * @file config_manager.cpp
 * @brief 영속적 설정 관리 클래스 구현
 * @details LittleFS 및 Preferences를 이용한 설정값 저장, 로드 및 지능형 지연 저장(Lazy Save) 기능 구현
 * @note [SYNC] ENG_Clock/config_manager.cpp — timezone 로드/저장 및 검증 추가
 */
#include "config_manager.h"
#include <string.h>

// 전역 인스턴스 정의
ConfigManager configManager;

ConfigManager::ConfigManager() : _isDirty(false), _flipMigrated(false) {
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
    // [2026-11-04] 방향 기본값 이전 (true → false) — 1회성.
    //   왜 옛 값이 뒤집혀 있었나:
    //     u8g2 초기화 시퀀스가 패널을 0xa1/0xc8(두 축 미러)로 두고, 생성자의 U8G2_R2가
    //     버퍼를 180° 돌린다. 이 둘은 **상쇄**되어 정방향이 된다.
    //     그런데 이 프로젝트는 begin()에서 setFlipMode(is_flipped)를 부르는데, true는
    //     0xa0/0xc0(미러 없음)를 보내 위 초기 미러를 **취소**시킨다 → R2의 180°가 남아 뒤집힘.
    //     같은 보드의 다른 프로젝트(Universal_Timer·Screen_To_OLEDs 등)는 setFlipMode를
    //     아예 부르지 않아 정방향이다. 그래서 기본값만 false로 맞추면 동작이 일치한다.
    //   왜 마이그레이션이 필요한가:
    //     기본값을 바꿔도 getBool("flip", …)의 **기본 인자**는 키가 없을 때만 쓰인다.
    //     이미 저장된 기기는 NVS에 flip=true가 남아 있어 그대로 뒤집힌 채다.
    //     flip_mig 표식이 없는 기기 = 옛 기본값이 박힌 기기로 보고 한 번만 교정한다.
    //   ⚠ 표식은 **신규 기기에도** 남긴다. 그래야 이 교정이 기기당 딱 한 번만 돌고,
    //     이후 사용자가 BTN1/웹으로 고른 FLIP 선택을 다음 부팅이 덮어쓰지 않는다.
    //     (표식을 옛 기기에만 남기면, 새 기기에서 FLIP을 켠 뒤 재부팅할 때
    //      isKey("flip")가 참이라 다시 교정 분기에 걸려 사용자 선택이 지워진다.)
    if (!_prefs.getBool("flip_mig", false)) {
        _settings.is_flipped = false;
        _flipMigrated = true;   // saveNow()가 표식을 함께 커밋한다
        if (_prefs.isKey("flip")) {
            Serial.println("[CONFIG] Flip default migrated (true -> false).");
        }
    } else {
        _settings.is_flipped = _prefs.getBool("flip", false);
    }
    _settings.chime_enabled = _prefs.getBool("chime", false);
    _settings.is_inverted = _prefs.getBool("inv", false);
    _settings.font_name = _prefs.getString("font_name", "System Default");
    _settings.font_slot = _prefs.getUChar("slot", 0);
    _settings.brightness = _prefs.getUChar("bright", 1);

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

    // 교정한 방향 기본값을 저장한다 — 사용자가 아무 조작을 하지 않아도 다음 부팅부터
    //   정상 방향이어야 한다. (표식 flip_mig도 saveNow()가 함께 커밋한다)
    if (_flipMigrated) setDirty();

    Serial.printf("[CONFIG] Settings loaded. TZ=%s\n", _settings.timezone);
}

void ConfigManager::setDirty() {
    _isDirty = true;
    if (_saveTimer != NULL) {
        // 타이머를 재시작/리셋 (이미 실행 중이면 5초 대기 시간이 초기화됨 — xTimerCreate의 5000ms와 일치)
        xTimerReset(_saveTimer, 0);
        Serial.println("[CONFIG] Save requested (Lazy save in 5s...)");
    }
}

void ConfigManager::saveNow() {
    // 마이그레이션은 사용자가 아무것도 바꾸지 않아도 저장돼야 한다.
    //   load()가 setDirty()를 부르므로 보통은 _isDirty가 서지만, 그 경로가
    //   나중에 바뀌더라도 표식이 누락되지 않도록 여기서도 조건에 포함한다.
    if (!_isDirty && !_flipMigrated) return;

    // [리뷰 §3.4] 검증은 **한 개라도 실패하면 전체를 건너뛴다.**
    //   이전엔 나머지 9개 필드를 먼저 써 두고 timezone만 건너뛰었는데, 그러면
    //   사용자는 "설정이 저장됐다"는 로그만 보고 재부팅하면 바뀐 값이 사라지는
    //   상태가 된다. 부분 저장은 조용한 데이터 손실로 이어진다.
    if (!isValidTimezone(_settings.timezone)) {
        Serial.println("[CONFIG] Refusing to persist: invalid timezone. Settings left unchanged.");
        return;
    }

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
    _prefs.putString("tz", _settings.timezone);

    // 마이그레이션 표식을 남긴다 — 다음 부팅부터는 교정을 다시 하지 않는다.
    //   이 표식이 없으면 매 부팅마다 사용자가 고른 FLIP 선택을 덮어쓴다.
    if (_flipMigrated) {
        _prefs.putBool("flip_mig", true);
        _flipMigrated = false;
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
