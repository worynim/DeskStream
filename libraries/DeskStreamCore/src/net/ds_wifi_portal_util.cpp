// worynim@gmail.com
/**
 * @file ds_wifi_portal_util.cpp
 * @brief WiFi 설정 포털 입력 검증·HTML 이스케이프 구현
 */
#include "ds_wifi_portal_util.h"
#include <string.h>

namespace ds {

static bool hasControlChar(const char* s) {
    for (; *s; s++) {
        if ((unsigned char)*s < 0x20 || *s == 0x7F) return true;
    }
    return false;
}

bool wifiSsidValid(const char* ssid) {
    if (!ssid) return false;
    const size_t n = strlen(ssid);
    return n >= 1 && n <= kWifiSsidMaxLen && !hasControlChar(ssid);
}

bool wifiPassValid(const char* pass) {
    if (!pass) return false;
    const size_t n = strlen(pass);
    if (n == 0) return true;   // 열린 네트워크
    return n >= kWifiPassMinLen && n <= kWifiPassMaxLen && !hasControlChar(pass);
}

bool htmlEscapeTo(const char* in, char* out, size_t cap) {
    if (!out || cap == 0) return false;
    size_t n = 0;
    for (; in && *in; in++) {
        const char* rep = nullptr;
        switch (*in) {
            case '&':  rep = "&amp;";  break;
            case '<':  rep = "&lt;";   break;
            case '>':  rep = "&gt;";   break;
            case '"':  rep = "&quot;"; break;
            case '\'': rep = "&#39;";  break;
            default:   break;
        }
        const size_t len = rep ? strlen(rep) : 1;
        if (n + len >= cap) { out[n] = '\0'; return false; }
        if (rep) { memcpy(out + n, rep, len); n += len; }
        else out[n++] = *in;
    }
    out[n] = '\0';
    return true;
}

}  // namespace ds
