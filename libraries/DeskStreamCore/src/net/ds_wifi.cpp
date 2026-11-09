// worynim@gmail.com
/**
 * @file ds_wifi.cpp
 * @brief WiFi 연결 서비스 구현
 */
#include "ds_wifi.h"
#include <WiFi.h>
#include <WiFiManager.h>

namespace ds {

bool WifiService::begin(const WifiConfig& cfg) {
    _cfg = cfg;

    // WiFi.begin()(무인자)은 저장된 AP로 연결만 걸고 즉시 반환한다
    //   (WiFiManager가 저장 시 자격증명을 NVS에 기록한다).
    WiFi.begin();
    const uint32_t start = millis();
    uint32_t tick = 0;
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < _cfg.connectTimeoutMs) {
        if (_cfg.onConnecting) _cfg.onConnecting(tick);
        delay(500);
        tick++;
    }

    // 미연결 → 설정 포털. startConfigPortal()은 autoConnect()와 달리 STA 재시도를 하지 않고,
    //   블로킹 모드이므로 portalTimeoutS가 그대로 적용된다.
    if (WiFi.status() != WL_CONNECTED) {
        WiFiManager wm;
        wm.setConfigPortalTimeout(_cfg.portalTimeoutS);
        wm.setAPCallback([this](WiFiManager*) {
            Serial.println("[WIFI] Config Mode Entered");
            if (_cfg.onPortal) _cfg.onPortal(_cfg.apName);
        });
        wm.startConfigPortal(_cfg.apName);
    }

    const bool ok = (WiFi.status() == WL_CONNECTED);
    if (!ok && _cfg.restartOnFail) ESP.restart();
    _lastRetryMs = millis();
    return ok;
}

void WifiService::loop() {
    // 코어 자동 재접속(기본 true)이 실패한 경우를 대비해 직접 재접속도 시도한다.
    if (WiFi.status() == WL_CONNECTED) return;
    const uint32_t now = millis();
    if ((uint32_t)(now - _lastRetryMs) >= _cfg.reconnectIntervalMs) {
        _lastRetryMs = now;
        WiFi.reconnect();
    }
}

bool WifiService::connected() const {
    return WiFi.status() == WL_CONNECTED;
}

void WifiService::resetSettings() {
    WiFiManager wm;
    wm.resetSettings();
}

}  // namespace ds
