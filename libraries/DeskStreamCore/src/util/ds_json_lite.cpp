// worynim@gmail.com
/**
 * @file ds_json_lite.cpp
 * @brief 평평한 JSON 객체 키 값 파서 구현
 */
#include "ds_json_lite.h"
#include <string.h>

namespace ds {

static const char* skipWs(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

const char* jsonFindValue(const char* body, const char* key) {
    if (!body || !key) return nullptr;
    const size_t klen = strlen(key);
    for (const char* p = strchr(body, '"'); p; p = strchr(p + 1, '"')) {
        if (p > body && p[-1] == '\\') continue;                 // 문자열 안의 \" — 키가 아니다
        if (strncmp(p + 1, key, klen) != 0 || p[1 + klen] != '"') continue;
        const char* q = skipWs(p + 2 + klen);
        if (*q != ':') continue;                                   // 값 위치의 같은 문자열 — 키가 아니다
        return skipWs(q + 1);
    }
    return nullptr;
}

bool jsonGetInt(const char* body, const char* key, long* out) {
    const char* p = jsonFindValue(body, key);
    if (!p) return false;
    bool neg = false;
    if (*p == '-') { neg = true; p++; }
    if (*p < '0' || *p > '9') return false;
    long v = 0;
    while (*p >= '0' && *p <= '9') {
        if (v > 100000000L) return false;                          // 설정값으로는 터무니없는 크기
        v = v * 10 + (*p++ - '0');
    }
    *out = neg ? -v : v;
    return true;
}

bool jsonGetBool(const char* body, const char* key, bool* out) {
    const char* p = jsonFindValue(body, key);
    if (!p) return false;
    if (strncmp(p, "true", 4) == 0)  { *out = true;  return true; }
    if (strncmp(p, "false", 5) == 0) { *out = false; return true; }
    return false;
}

bool jsonGetString(const char* body, const char* key, char* out, size_t cap) {
    const char* p = jsonFindValue(body, key);
    if (!p || *p != '"' || cap == 0) return false;
    p++;
    size_t n = 0;
    while (*p && *p != '"') {
        char c = *p++;
        if (c == '\\') {
            c = *p++;
            if (c != '"' && c != '\\' && c != '/') return false;   // 지원하지 않는 이스케이프
        }
        if (n + 1 >= cap) return false;                            // 잘림 → 거부
        out[n++] = c;
    }
    if (*p != '"') return false;                                    // 닫는 따옴표 없음
    out[n] = '\0';
    return true;
}

}  // namespace ds
