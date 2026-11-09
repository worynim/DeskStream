// worynim@gmail.com
/**
 * @file config_manager.h
 * @brief 영속적 설정 관리 클래스 정의
 * @details 설정 구조체와 필드 테이블만 이 프로젝트가 정의하고, NVS 로드·검증·지연 저장은
 *          ds::ConfigStore(DeskStreamCore)가 한다. 같은 필드 테이블로 웹 /api/config 도 처리된다.
 * @note [SYNC] 원본: ENG_Clock/config_manager.h — date_order 제거, script_type 추가 (PLAN §6.8)
 */
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <string.h>
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
    bool is_inverted;
    String font_name;
    uint8_t font_slot;
    uint8_t brightness;

    // POSIX TZ 문자열 (예: "CST-8"). setenv("TZ", ...)로 직접 쓰이므로 웹 입력 검증이 필수다.
    char timezone[ds::kTimezoneMaxLen];

    /**
     * @brief 문자판 (0=간체 简体, 1=번체 繁體) — PLAN §6.8
     * @details 간체/번체 차이는 点/點 · 时/時 · 两/兩 **3글자뿐**이라,
     *          변환 계층(chinese_time_core)에서 표 하나로 처리한다.
     *          여기서는 NVS에 숫자 하나로 보관할 뿐이다.
     */
    uint8_t script_type;

    // 기본값 설정 — 중국(간체권) 기준. 시간대는 CST-8.
    SystemSettings() :
        anim_mode(ANIMATION_TYPE_SCROLL_UP),
        display_mode(CLOCK_MODE_WORD),
        hour_format(HOUR_FORMAT_12H),
        is_flipped(true),
        chime_enabled(false),
        is_inverted(false),
        font_name("System Default"),
        font_slot(0),
        brightness(1),
        script_type(SCRIPT_SIMPLIFIED) {
        strncpy(timezone, DEFAULT_TIMEZONE, sizeof(timezone) - 1);
        timezone[sizeof(timezone) - 1] = '\0';
    }
};

/**
 * @brief 설정 로드·저장·지연 저장 (ds::ConfigStore 위의 얇은 래퍼)
 * @details 필드 테이블(config_manager.cpp kFields)이 NVS 키·검증 범위·웹 JSON 키·POST 적용 순서를 정한다.
 */
class ConfigManager {
public:
    /** NVS("clock")에서 로드한다. 값 검증 실패 필드는 기본값을 유지한다 */
    void begin();

    // 설정 가져오기 (참조 반환)
    SystemSettings& get() { return _settings; }

    // 변경 사항 알림 (5초 후 자동 저장 예약 — 그 사이 또 바뀌면 다시 미룬다)
    void setDirty() { _store.setDirty(); }

    // 즉시 저장 (전원 종료 전 등 특수 상황)
    void saveNow() { _store.saveNow(); }

    // 기본값으로 초기화
    void resetToDefaults();

    /** 웹 설정 API(ds::WebConfigApi)가 같은 필드 테이블을 쓰도록 노출한다 */
    ds::ConfigStore& store() { return _store; }

private:
    SystemSettings _settings;
    ds::ConfigStore _store;
};

// 전역 인스턴스
extern ConfigManager configManager;

#endif
