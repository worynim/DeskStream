// worynim@gmail.com
/**
 * @file ds_config.h
 * @brief 필드 테이블 기반 설정 저장소 (NVS Preferences + 지연 저장)
 * @details 프로젝트는 설정 구조체와 **필드 테이블**만 정의한다. 테이블 하나로
 *            · NVS 로드(값 검증 실패 시 구조체 기본값 유지)
 *            · 지연 저장(setDirty 후 saveDelayMs — 연속 변경을 한 번에 커밋)
 *            · 웹 설정 API(ds_web_config: GET JSON 직렬화, POST 파싱·검증·적용)
 *          가 모두 동작한다.
 *
 *          사용 예:
 *          @code
 *          struct Settings { uint8_t brightness = 1; bool flip = false; char tz[48] = "KST-9"; };
 *          static Settings g;
 *          static const ds::ConfigField kFields[] = {
 *              DS_CFG_U8(Settings, brightness, "brightness", "bright", 1, 255, nullptr),
 *              DS_CFG_BOOL(Settings, flip, "is_flipped", "flip", nullptr),
 *              DS_CFG_CSTR(Settings, tz, "timezone", "tz", ds::isValidTimezone, nullptr),
 *          };
 *          store.begin("myproj", &g, kFields, 3);
 *          @endcode
 * @note 출처: Hangeul_Clock/config_manager.* (v2.10.0)의 지연 저장·검증 규칙을 일반화.
 */
#ifndef DS_CONFIG_H
#define DS_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>
#include <stddef.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

namespace ds {

enum class FieldType : uint8_t {
    U8,     ///< uint8_t, min~max 범위 검사
    BOOL,   ///< bool
    STR,    ///< Arduino String, size = 최대 바이트 길이
    CSTR,   ///< char[size] (NUL 포함)
};

/** 웹 POST에서 값이 그대로여도 onChange를 부른다 (setter가 스스로 중복을 거르는 필드) */
constexpr uint8_t FIELD_APPLY_ALWAYS = 0x01;
/** NVS에만 저장하고 웹 API(GET/POST)에는 노출하지 않는다 — 웹이 다른 키로 묶어 다루는 필드 */
constexpr uint8_t FIELD_NO_WEB = 0x02;

struct ConfigField {
    const char* key;        ///< 웹 JSON 키
    const char* nvsKey;     ///< NVS 키 (15자 이하)
    FieldType type;
    uint16_t offset;        ///< 설정 구조체 안의 위치 (offsetof)
    uint16_t size;          ///< STR: 최대 길이 / CSTR: 버퍼 크기
    int16_t min, max;       ///< U8 허용 범위 (포함)
    /** STR/CSTR 추가 검증 (nullptr = 길이만 검사) */
    bool (*validate)(const char* value);
    /**
     * 웹에서 값이 바뀌었을 때의 처리 (nullptr = 저장소가 값을 쓰고 setDirty).
     * 지정하면 **값 쓰기까지 이 함수가 한다** — 하드웨어 반영 등 부수효과가 있는 필드용.
     * value 타입: U8 → const uint8_t*, BOOL → const bool*, STR/CSTR → const char*
     */
    void (*onChange)(const void* value);
    uint8_t flags;
};

#define DS_CFG_U8(S, m, key, nvs, lo, hi, onChg) \
    { key, nvs, ds::FieldType::U8, (uint16_t)offsetof(S, m), 1, lo, hi, nullptr, onChg, 0 }
#define DS_CFG_U8_F(S, m, key, nvs, lo, hi, onChg, flg) \
    { key, nvs, ds::FieldType::U8, (uint16_t)offsetof(S, m), 1, lo, hi, nullptr, onChg, flg }
#define DS_CFG_BOOL(S, m, key, nvs, onChg) \
    { key, nvs, ds::FieldType::BOOL, (uint16_t)offsetof(S, m), 1, 0, 1, nullptr, onChg, 0 }
#define DS_CFG_STR(S, m, key, nvs, maxLen, valid, onChg, flg) \
    { key, nvs, ds::FieldType::STR, (uint16_t)offsetof(S, m), maxLen, 0, 0, valid, onChg, flg }
#define DS_CFG_CSTR(S, m, key, nvs, valid, onChg) \
    { key, nvs, ds::FieldType::CSTR, (uint16_t)offsetof(S, m), (uint16_t)sizeof(((S*)0)->m), 0, 0, valid, onChg, 0 }

class ConfigStore {
public:
    /** NVS 열린 상태에서 로드 직후/저장 직전에 부르는 훅 (마이그레이션용) */
    typedef void (*PrefsHook)(Preferences& prefs);

    /**
     * @brief 필드 테이블로 NVS에서 로드한다
     * @param settings 기본값이 채워진 설정 구조체 (로드 실패·검증 실패 시 그 값이 남는다)
     */
    void begin(const char* nvsNamespace, void* settings, const ConfigField* fields, uint8_t count,
               uint32_t saveDelayMs = 5000, PrefsHook afterLoad = nullptr, PrefsHook beforeSave = nullptr);

    /** 변경 알림 — saveDelayMs 뒤 저장 (그 사이 또 바뀌면 다시 미룬다) */
    void setDirty();

    /**
     * @brief 지금 저장한다
     * @details 필드가 **하나라도** 검증에 실패하면 전체를 저장하지 않는다 — 부분 저장은
     *          "저장됐다는 로그만 보고 재부팅하면 값이 사라지는" 조용한 데이터 손실이 된다.
     */
    void saveNow();

    /** 값이 필드 규칙(범위·길이·validate)을 만족하는가 (value 타입은 ConfigField::onChange와 같다) */
    bool isValid(const ConfigField& f, const void* value) const;

    /** 필드의 현재 값이 value와 같은가 */
    bool equals(const ConfigField& f, const void* value) const;

    /** 필드에 값을 쓴다 (검증하지 않는다 — 호출 전에 isValid) */
    void write(const ConfigField& f, const void* value);

    void* fieldPtr(const ConfigField& f) const { return (uint8_t*)_settings + f.offset; }
    const ConfigField* fields() const { return _fields; }
    uint8_t fieldCount() const { return _count; }

private:
    static void onTimer(TimerHandle_t t);

    const char* _ns = nullptr;
    void* _settings = nullptr;
    const ConfigField* _fields = nullptr;
    uint8_t _count = 0;
    PrefsHook _beforeSave = nullptr;
    TimerHandle_t _timer = nullptr;
    volatile bool _dirty = false;
};

}  // namespace ds

#endif
