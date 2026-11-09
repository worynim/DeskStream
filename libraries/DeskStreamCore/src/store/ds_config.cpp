// worynim@gmail.com
/**
 * @file ds_config.cpp
 * @brief 필드 테이블 기반 설정 저장소 구현
 */
#include "ds_config.h"
#include <string.h>

namespace ds {

void ConfigStore::begin(const char* nvsNamespace, void* settings, const ConfigField* fields, uint8_t count,
                        uint32_t saveDelayMs, PrefsHook afterLoad, PrefsHook beforeSave) {
    _ns = nvsNamespace;
    _settings = settings;
    _fields = fields;
    _count = count;
    _beforeSave = beforeSave;
    if (!_timer) _timer = xTimerCreate("CfgSave", pdMS_TO_TICKS(saveDelayMs), pdFALSE, this, onTimer);

    Preferences prefs;
    // 첫 부팅은 네임스페이스가 없어 읽기 전용 열기가 실패한다 — 그때는 기본값을 그대로 쓴다.
    const bool opened = prefs.begin(_ns, true);
    if (!opened) Serial.printf("[CONFIG] NVS '%s' not found, using defaults.\n", _ns);

    for (uint8_t i = 0; opened && i < _count; i++) {
        const ConfigField& f = _fields[i];
        void* p = fieldPtr(f);
        switch (f.type) {
            case FieldType::U8: {
                const uint8_t v = prefs.getUChar(f.nvsKey, *(uint8_t*)p);
                if (isValid(f, &v)) *(uint8_t*)p = v;
                else Serial.printf("[CONFIG] '%s' out of range (%u), keeping default.\n", f.key, v);
                break;
            }
            case FieldType::BOOL:
                *(bool*)p = prefs.getBool(f.nvsKey, *(bool*)p);
                break;
            case FieldType::STR:
            case FieldType::CSTR: {
                // NVS에 저장된 값도 신뢰하지 않는다 (수동 편집·과거 버그로 망가진 값일 수 있다)
                const String cur = (f.type == FieldType::STR) ? *(String*)p : String((const char*)p);
                const String v = prefs.getString(f.nvsKey, cur);
                if (isValid(f, v.c_str())) write(f, v.c_str());
                else Serial.printf("[CONFIG] '%s' invalid in NVS, keeping default.\n", f.key);
                break;
            }
        }
    }

    // 마이그레이션 훅은 네임스페이스가 없어도 부른다 (열리지 않은 Preferences의 get*은 기본값을 돌려준다).
    //   그래야 신규 기기에도 "마이그레이션 완료" 표식이 첫 저장 때 함께 남는다.
    if (afterLoad) afterLoad(prefs);
    if (opened) prefs.end();
    Serial.printf("[CONFIG] '%s' loaded (%u fields).\n", _ns, (unsigned)_count);
}

void ConfigStore::setDirty() {
    _dirty = true;
    if (_timer) xTimerReset(_timer, 0);   // 이미 대기 중이면 대기 시간이 처음부터 다시 시작된다
}

bool ConfigStore::isValid(const ConfigField& f, const void* value) const {
    switch (f.type) {
        case FieldType::U8: {
            const int v = *(const uint8_t*)value;
            return v >= f.min && v <= f.max;
        }
        case FieldType::BOOL:
            return true;
        case FieldType::STR:
        case FieldType::CSTR: {
            const char* s = (const char*)value;
            const size_t maxLen = (f.type == FieldType::CSTR) ? (size_t)f.size - 1 : f.size;
            if (strlen(s) > maxLen) return false;
            return f.validate ? f.validate(s) : true;
        }
    }
    return false;
}

bool ConfigStore::equals(const ConfigField& f, const void* value) const {
    const void* p = fieldPtr(f);
    switch (f.type) {
        case FieldType::U8:   return *(const uint8_t*)p == *(const uint8_t*)value;
        case FieldType::BOOL: return *(const bool*)p == *(const bool*)value;
        case FieldType::STR:  return *(const String*)p == (const char*)value;
        case FieldType::CSTR: return strcmp((const char*)p, (const char*)value) == 0;
    }
    return false;
}

void ConfigStore::write(const ConfigField& f, const void* value) {
    void* p = fieldPtr(f);
    switch (f.type) {
        case FieldType::U8:   *(uint8_t*)p = *(const uint8_t*)value; break;
        case FieldType::BOOL: *(bool*)p = *(const bool*)value; break;
        case FieldType::STR:  *(String*)p = (const char*)value; break;
        case FieldType::CSTR:
            strncpy((char*)p, (const char*)value, f.size - 1);
            ((char*)p)[f.size - 1] = '\0';
            break;
    }
}

void ConfigStore::saveNow() {
    if (!_dirty) return;

    for (uint8_t i = 0; i < _count; i++) {
        const ConfigField& f = _fields[i];
        const void* p = fieldPtr(f);
        const void* v = (f.type == FieldType::STR) ? (const void*)((const String*)p)->c_str() : p;
        if (!isValid(f, v)) {
            Serial.printf("[CONFIG] Refusing to persist: '%s' invalid. Settings left unchanged.\n", f.key);
            return;
        }
    }

    Preferences prefs;
    if (!prefs.begin(_ns, false)) {
        Serial.println("[CONFIG] Error: Could not open Preferences for writing!");
        return;
    }
    for (uint8_t i = 0; i < _count; i++) {
        const ConfigField& f = _fields[i];
        const void* p = fieldPtr(f);
        switch (f.type) {
            case FieldType::U8:   prefs.putUChar(f.nvsKey, *(const uint8_t*)p); break;
            case FieldType::BOOL: prefs.putBool(f.nvsKey, *(const bool*)p); break;
            case FieldType::STR:  prefs.putString(f.nvsKey, *(const String*)p); break;
            case FieldType::CSTR: prefs.putString(f.nvsKey, (const char*)p); break;
        }
    }
    if (_beforeSave) _beforeSave(prefs);
    prefs.end();
    _dirty = false;
    Serial.println("[CONFIG] All settings committed to Flash memory.");
}

void ConfigStore::onTimer(TimerHandle_t t) {
    ConfigStore* self = (ConfigStore*)pvTimerGetTimerID(t);
    if (self) self->saveNow();
}

}  // namespace ds
