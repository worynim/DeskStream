// worynim@gmail.com
/**
 * @file ds_web_config.cpp
 * @brief 웹 설정 API 구현
 */
#include "ds_web_config.h"
#include "../util/ds_json_lite.h"

namespace ds {

String jsonEscape(const String& s) {
    String out;
    out.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
        const char c = s[i];
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if ((unsigned char)c < 0x20) { char u[7]; snprintf(u, sizeof(u), "\\u%04x", (unsigned char)c); out += u; }
        else out += c;
    }
    return out;
}

void WebConfigApi::begin(WebServer& server, ConfigStore& store, ExtraJson extra, AfterPost after) {
    _server = &server;
    _store = &store;
    _extra = extra;
    _after = after;
    server.on("/api/config", HTTP_GET, [this]() { handleGet(); });
    server.on("/api/config", HTTP_POST, [this]() { handlePost(); });
}

void WebConfigApi::handleGet() {
    String json = "{";
    bool first = true;
    for (uint8_t i = 0; i < _store->fieldCount(); i++) {
        const ConfigField& f = _store->fields()[i];
        if (f.flags & FIELD_NO_WEB) continue;
        const void* p = _store->fieldPtr(f);
        if (!first) json += ',';
        first = false;
        json += '"'; json += f.key; json += "\":";
        switch (f.type) {
            case FieldType::U8:   json += String((int)*(const uint8_t*)p); break;
            case FieldType::BOOL: json += *(const bool*)p ? "true" : "false"; break;
            case FieldType::STR:  json += '"'; json += jsonEscape(*(const String*)p); json += '"'; break;
            case FieldType::CSTR: json += '"'; json += jsonEscape(String((const char*)p)); json += '"'; break;
        }
    }
    if (_extra) _extra(json);
    json += '}';
    _server->send(200, "application/json", json);
}

void WebConfigApi::handlePost() {
    if (!_server->hasArg("plain")) {
        // 본문 없는 POST에 응답하지 않으면 웹 UI fetch()가 "응답 없음"을 저장 실패로 표시한다
        _server->send(400, "text/plain", "Missing body");
        return;
    }
    const String body = _server->arg("plain");
    const char* b = body.c_str();

    for (uint8_t i = 0; i < _store->fieldCount(); i++) {
        const ConfigField& f = _store->fields()[i];
        if (f.flags & FIELD_NO_WEB) continue;
        uint8_t u8 = 0;
        bool bv = false;
        char sbuf[129];
        const void* value = nullptr;

        switch (f.type) {
            case FieldType::U8: {
                long v;
                if (!jsonGetInt(b, f.key, &v)) continue;
                if (v < f.min || v > f.max) { Serial.printf("[WEB] Rejected %s=%ld\n", f.key, v); continue; }
                u8 = (uint8_t)v;
                value = &u8;
                break;
            }
            case FieldType::BOOL:
                if (!jsonGetBool(b, f.key, &bv)) continue;
                value = &bv;
                break;
            case FieldType::STR:
            case FieldType::CSTR: {
                if (!jsonFindValue(b, f.key)) continue;
                if (!jsonGetString(b, f.key, sbuf, sizeof(sbuf)) || !_store->isValid(f, sbuf)) {
                    Serial.printf("[WEB] Rejected invalid %s\n", f.key);
                    continue;
                }
                value = sbuf;
                break;
            }
        }

        if (!(f.flags & FIELD_APPLY_ALWAYS) && _store->equals(f, value)) continue;
        if (f.onChange) {
            f.onChange(value);
        } else {
            _store->write(f, value);
            _store->setDirty();
        }
    }

    if (_after) _after(b);
    _server->send(200, "text/plain", "OK");
}

}  // namespace ds
