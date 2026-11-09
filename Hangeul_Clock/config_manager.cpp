// worynim@gmail.com
/**
 * @file config_manager.cpp
 * @brief 설정 필드 테이블과 방향 기본값 마이그레이션
 * @details 필드 테이블 한 장이 (1) NVS 키·검증 규칙 (2) 웹 /api/config 의 JSON 키·적용 순서를
 *          함께 정한다. 로드·지연 저장·웹 파싱은 DeskStreamCore(ds::ConfigStore·ds::WebConfigApi).
 */
#include "config_manager.h"
#include "display_manager.h"   // 웹에서 값이 바뀔 때 화면에 반영하는 setter
#include <string.h>

// 전역 인스턴스 정의
ConfigManager configManager;

// === 웹 변경 시 적용 함수 ===
// 값 쓰기·setDirty·화면 반영은 DisplayManager의 setter가 한다 (버튼 경로와 같은 함수).
static void onAnim(const void* v)     { display.setAnimMode(*(const uint8_t*)v); }
static void onMode(const void* v)     { display.setDisplayMode(*(const uint8_t*)v); }
static void onFormat(const void* v)   { display.setHourFormat(*(const uint8_t*)v); }
static void onChime(const void* v)    { display.setChime(*(const bool*)v); }
static void onFlip(const void* v)     { display.setFlipDisplay(*(const bool*)v); }
static void onInvert(const void* v)   { display.setInversion(*(const bool*)v); }
static void onSlot(const void* v)     { display.setFontSlot(*(const uint8_t*)v); }
static void onBright(const void* v)   { display.setBrightness(*(const uint8_t*)v); }
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
 *       `{font_slot, font_name}`을 함께 보내는데, 슬롯을 먼저 옮겨야 이름표가 그 슬롯에 붙는다
 *       (web_pages.h uploadFont() 주석의 계약).
 * @note font_name 최대 길이 128은 NVS·JSON 파싱 상한이다. 입력 이름표 상한(FONT_NAME_MAX_LEN 64)은
 *       setSlotName()이 검사한다 — 여기 상한이 더 작으면 저장 검증이 다른 필드까지 막는다.
 */
static const ds::ConfigField kFields[] = {
    DS_CFG_U8(SystemSettings, anim_mode, "anim_mode", "anim", 0, ANIMATION_TYPE_COUNT - 1, onAnim),
    DS_CFG_U8(SystemSettings, display_mode, "display_mode", "mode", CLOCK_MODE_HANGUL, CLOCK_MODE_NUMERIC, onMode),
    DS_CFG_U8(SystemSettings, hour_format, "hour_format", "format", HOUR_FORMAT_12H, HOUR_FORMAT_24H, onFormat),
    DS_CFG_BOOL(SystemSettings, chime_enabled, "chime_enabled", "chime", onChime),
    DS_CFG_BOOL(SystemSettings, is_flipped, "is_flipped", "flip", onFlip),
    DS_CFG_BOOL(SystemSettings, is_inverted, "is_inverted", "inv", onInvert),
    DS_CFG_U8(SystemSettings, font_slot, "font_slot", "slot", 0, FONT_SLOT_COUNT - 1, onSlot),
    DS_CFG_U8(SystemSettings, brightness, "brightness", "bright", 1, 255, onBright),
    DS_CFG_CSTR(SystemSettings, timezone, "timezone", "tz", ds::isValidTimezone, onTimezone),
    DS_CFG_STR(SystemSettings, font_name, "font_name", "font_name", 128, nullptr, onFontName, ds::FIELD_APPLY_ALWAYS),
};

// 이번 부팅에서 방향 기본값 교정(true → false)이 일어났는가 — afterLoad가 세우고 beforeSave가 소비한다.
static bool s_flipMigrated = false;

/**
 * @brief [2026-11-04] 방향 기본값 이전 (true → false) — 기기당 1회
 * @details 왜 옛 값이 뒤집혀 있었나:
 *            u8g2 초기화 시퀀스가 패널을 0xa1/0xc8(두 축 미러)로 두고, U8G2_R2가 버퍼를
 *            180° 돌린다. 이 둘은 **상쇄**되어 정방향이 된다. 그런데 setFlipMode(true)는
 *            0xa0/0xc0(미러 없음)를 보내 초기 미러를 **취소**시킨다 → R2의 180°가 남아 뒤집힘.
 *          왜 마이그레이션이 필요한가:
 *            기본값을 바꿔도 이미 저장된 기기는 NVS에 flip=true가 남아 있다.
 *            flip_mig 표식이 없는 기기 = 옛 기본값이 박힌 기기로 보고 한 번만 교정한다.
 *          ⚠ 표식은 **신규 기기에도** 남긴다 — 그래야 교정이 기기당 딱 한 번만 돌고, 이후 사용자가
 *            BTN1/웹으로 고른 FLIP 선택을 다음 부팅이 덮어쓰지 않는다. (NVS 네임스페이스가 아직 없는
 *            첫 부팅에도 이 훅이 불리므로, 첫 저장 때 표식이 함께 남는다.)
 */
static void afterLoad(Preferences& prefs) {
    if (prefs.getBool("flip_mig", false)) return;
    configManager.get().is_flipped = false;
    s_flipMigrated = true;
    if (prefs.isKey("flip")) Serial.println("[CONFIG] Flip default migrated (true -> false).");
    configManager.setDirty();   // 사용자가 아무 조작을 하지 않아도 표식이 커밋되게 한다
}

static void beforeSave(Preferences& prefs) {
    if (!s_flipMigrated) return;
    prefs.putBool("flip_mig", true);
    s_flipMigrated = false;
}

void ConfigManager::begin() {
    Serial.println("[CONFIG] Initializing ConfigManager...");
    _store.begin("clock", &_settings, kFields, sizeof(kFields) / sizeof(kFields[0]),
                 5000, afterLoad, beforeSave);
    Serial.printf("[CONFIG] TZ=%s\n", _settings.timezone);
}

void ConfigManager::resetToDefaults() {
    _settings = SystemSettings(); // 구조체 초기화 (기본 생성자 호출)
    setDirty();
    Serial.println("[CONFIG] Settings reset to default values.");
}
