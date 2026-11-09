// worynim@gmail.com
/**
 * @file agent_auth.cpp
 * @brief agent_auth.h 구현 — HMAC-SHA256 은 ESP32 코어의 mbedtls, 키 저장은 Preferences(NVS)
 */
#include "agent_auth.h"

#include <Preferences.h>
#include <mbedtls/md.h>
#include <stdio.h>
#include <string.h>

namespace am {

namespace {

constexpr size_t kKeyLen = 32;
constexpr size_t kSigHexLen = 32;   // HMAC 앞 16바이트
const char* const kNvsNs = "agentkey";

uint8_t key[kKeyLen];
bool hasKey = false;
char kid[9] = "";
char nonce[33] = "";
uint32_t counter = 0;

int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

void toHex(const uint8_t* in, size_t n, char* out) {
    static const char* d = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) {
        out[i * 2] = d[in[i] >> 4];
        out[i * 2 + 1] = d[in[i] & 15];
    }
    out[n * 2] = '\0';
}

void hmac(const char* a, const char* b, uint8_t out[32]) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, key, kKeyLen);
    mbedtls_md_hmac_update(&ctx, (const uint8_t*)a, strlen(a));
    mbedtls_md_hmac_update(&ctx, (const uint8_t*)b, strlen(b));
    mbedtls_md_hmac_finish(&ctx, out);
    mbedtls_md_free(&ctx);
}

void updateKid() {
    uint8_t mac[32];
    hmac("kid", "", mac);
    toHex(mac, 4, kid);
}

}  // namespace

void authBegin() {
    Preferences p;
    p.begin(kNvsNs, true);
    hasKey = p.getBytes("key", key, kKeyLen) == kKeyLen;
    p.end();
    if (hasKey) updateKid();
}

bool authHasKey() { return hasKey; }

const char* authKid() { return kid; }

bool authSetKey(const char* hex64) {
    if (strlen(hex64) != kKeyLen * 2) return false;
    uint8_t k[kKeyLen];
    for (size_t i = 0; i < kKeyLen; i++) {
        int hi = hexVal(hex64[i * 2]), lo = hexVal(hex64[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        k[i] = (uint8_t)(hi << 4 | lo);
    }
    memcpy(key, k, kKeyLen);
    hasKey = true;
    updateKid();
    Preferences p;
    p.begin(kNvsNs, false);
    p.putBytes("key", key, kKeyLen);
    p.end();
    return true;
}

void authSetNonce(const char* n) {
    strncpy(nonce, n, sizeof(nonce) - 1);
    nonce[sizeof(nonce) - 1] = '\0';
    counter = 0;
}

const char* authSignLine(const char* body, char* out, size_t cap) {
    size_t n = strlen(body);
    if (n < 2 || body[n - 1] != '}' || cap < n + 64) {
        out[0] = '\0';
        return out;
    }
    // 본문 = 원래 본문 + ,"c":N}
    char signedBody[320];
    snprintf(signedBody, sizeof(signedBody), "%.*s,\"c\":%lu}", (int)(n - 1), body, (unsigned long)++counter);
    if (!hasKey || !nonce[0]) {
        snprintf(out, cap, "%s", signedBody);
        return out;
    }
    uint8_t mac[32];
    hmac(nonce, signedBody, mac);
    char sig[kSigHexLen + 1];
    toHex(mac, kSigHexLen / 2, sig);
    size_t m = strlen(signedBody);
    snprintf(out, cap, "%.*s,\"sig\":\"%s\"}", (int)(m - 1), signedBody, sig);
    return out;
}

}  // namespace am
