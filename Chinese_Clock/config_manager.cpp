// worynim@gmail.com
/**
 * @file config_manager.cpp
 * @brief 설정 필드 테이블
 * @details 필드 테이블 한 장이 (1) NVS 키·검증 규칙 (2) 웹 /api/config 의 JSON 키·적용 순서를
 *          함께 정한다. 로드·지연 저장·웹 파싱은 DeskStreamCore(ds::ConfigStore·ds::WebConfigApi).
 */
#include "config_manager.h"
#include "display_manager.h"   // 웹에서 값이 바뀔 때 화면에 반영하는 setter
#include <string.h>

ConfigManager configManager;

// === 웹 변경 시 적용 함수 ===
// 값 쓰기·setDirty·화면 반영은 DisplayManager의 setter가 한다 (버튼 경로와 같은 함수).
static void onAnim(const void* v)     { display.setAnimMode(*(const uint8_t*)v); }
static void onFormat(const void* v)   { display.setHourFormat(*(const uint8_t*)v); }
static void onSlot(const void* v)     { display.setFontSlot(*(const uint8_t*)v); }
static void onBright(const void* v)   { display.setBrightness(*(const uint8_t*)v); }
static void onChime(const void* v)    { display.setChime(*(const bool*)v); }
static void onFlip(const void* v)     { display.setFlipDisplay(*(const bool*)v); }
static void onInvert(const void* v)   { display.setInversion(*(const bool*)v); }
static void onFontName(const void* v) { display.setFontName(String((const char*)v)); }   // 길이·문자 검증은 setSlotName()
static void onTimezone(const void* v) {
    // 검증(ds::isValidTimezone)은 저장소가 이미 했다. TZ 적용·SNTP 재시작은 time_sync 단일 경로.
    SystemSettings& s = configManager.get();
    strncpy(s.timezone, (const char*)v, sizeof(s.timezone) - 1);
    s.timezone[sizeof(s.timezone) - 1] = '\0';
    configManager.setDirty();
    display.applyTimezone();
}

/**
 * 필드 테이블 — **순서가 웹 POST 적용 순서다.**
 * @note font_slot은 font_name보다 먼저 와야 한다. 폰트 업로드 후 웹 UI가
 *       `{font_slot, font_name}`을 함께 보내는데, 슬롯을 먼저 옮겨야 이름표가 그 슬롯에 붙는다.
 * @note display_mode·script_type은 웹에 따로 내지 않는다(FIELD_NO_WEB). 웹은 둘을 묶은
 *       `presentation`(简体/繁體/數字) 하나로 다루고, 그 키는 web_manager가 setPresentation()으로 처리한다.
 *       따로 내면 웹이 조합을 되살려야 하고, 펌웨어와 웹에 같은 변환 규칙이 두 벌 생긴다.
 * @note font_name 최대 길이 128은 NVS·JSON 파싱 상한이다. 입력 이름표 상한(FONT_NAME_MAX_LEN 64)은
 *       setSlotName()이 검사한다.
 * @note is_flipped 기본값은 true다(SystemSettings 생성자) — 중국어판은 한글판의 방향 마이그레이션을 받지 않았다.
 */
static const ds::ConfigField kFields[] = {
    DS_CFG_U8(SystemSettings, anim_mode, "anim_mode", "anim", 0, ANIMATION_TYPE_COUNT - 1, onAnim),
    DS_CFG_U8(SystemSettings, hour_format, "hour_format", "format", HOUR_FORMAT_12H, HOUR_FORMAT_24H, onFormat),
    DS_CFG_U8(SystemSettings, font_slot, "font_slot", "slot", 0, FONT_SLOT_COUNT - 1, onSlot),
    DS_CFG_U8(SystemSettings, brightness, "brightness", "bright", 1, 255, onBright),
    DS_CFG_BOOL(SystemSettings, chime_enabled, "chime_enabled", "chime", onChime),
    DS_CFG_BOOL(SystemSettings, is_flipped, "is_flipped", "flip", onFlip),
    DS_CFG_BOOL(SystemSettings, is_inverted, "is_inverted", "inv", onInvert),
    DS_CFG_U8_F(SystemSettings, display_mode, "display_mode", "mode", CLOCK_MODE_WORD, CLOCK_MODE_NUMERIC, nullptr, ds::FIELD_NO_WEB),
    // 문자판 (0=간체 简体, 1=번체 繁體) — 범위 밖 NVS 값은 로드 때 간체로 되돌아간다
    DS_CFG_U8_F(SystemSettings, script_type, "script_type", "script", SCRIPT_SIMPLIFIED, SCRIPT_TRADITIONAL, nullptr, ds::FIELD_NO_WEB),
    DS_CFG_CSTR(SystemSettings, timezone, "timezone", "tz", ds::isValidTimezone, onTimezone),
    DS_CFG_STR(SystemSettings, font_name, "font_name", "font_name", 128, nullptr, onFontName, ds::FIELD_APPLY_ALWAYS),
};

void ConfigManager::begin() {
    Serial.println("[CONFIG] Initializing ConfigManager...");
    _store.begin("clock", &_settings, kFields, sizeof(kFields) / sizeof(kFields[0]));
    Serial.printf("[CONFIG] Loaded. TZ=%s Script=%s\n", _settings.timezone,
                  _settings.script_type == SCRIPT_TRADITIONAL ? "Traditional" : "Simplified");
}

void ConfigManager::resetToDefaults() {
    _settings = SystemSettings();   // 기본 생성자 호출
    setDirty();
    Serial.println("[CONFIG] Settings reset to default values.");
}
