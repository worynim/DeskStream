// worynim@gmail.com
/**
 * @file ds_wifi.h
 * @brief WiFi 연결 서비스 (저장된 AP 비차단 접속 → 실패 시 WiFiManager 설정 포털 → 끊김 재접속)
 * @details WiFiManager::autoConnect()를 쓰지 않는다. autoConnect()는 내부
 *          waitForConnectResult()가 코어 기본값 60초를 블로킹으로 기다려, 그동안 화면 진행
 *          표시도 멈춰 기기가 죽은 것처럼 보였다. 대신 WiFi.begin()(무인자)으로 드라이버/NVS에
 *          저장된 AP에 연결만 걸고, connectTimeoutMs 동안 진행 콜백을 부르며 기다린다.
 *          그래도 못 붙으면 저장된 SSID를 건너뛰고 **곧바로** 설정 포털(AP)로 간다.
 * @note 출처: Hangeul_Clock.ino setup()의 WiFi 시퀀스 + time_sync.cpp의 재접속 (v2.10.0)
 */
#ifndef DS_WIFI_H
#define DS_WIFI_H

#include <Arduino.h>

namespace ds {

struct WifiConfig {
    const char* apName = "DeskStream_Setup";   ///< 설정 포털 SSID
    uint32_t connectTimeoutMs = 10000;         ///< 저장된 AP 접속 대기 (넘으면 포털)
    uint16_t portalTimeoutS = 120;             ///< 포털 대기 (넘으면 실패)
    bool restartOnFail = true;                 ///< 포털까지 실패하면 재부팅 (false면 오프라인으로 계속)
    uint32_t reconnectIntervalMs = 60000;      ///< 끊긴 동안 WiFi.reconnect() 간격
    /** 접속 대기 중 500ms마다 호출 (tick = 0,1,2…) — 진행 점 표시용 */
    void (*onConnecting)(uint32_t tick) = nullptr;
    /** 설정 포털(AP)에 들어갈 때 호출 — 화면에 AP 이름·IP 안내용 */
    void (*onPortal)(const char* apName) = nullptr;
};

class WifiService {
public:
    /** @return 연결 성공 여부 (restartOnFail이면 실패 시 반환하지 않고 재부팅) */
    bool begin(const WifiConfig& cfg);

    /** 메인 루프에서 호출 — 끊겨 있으면 reconnectIntervalMs마다 재접속을 시도한다 */
    void loop();

    bool connected() const;

    /** WiFiManager에 저장된 자격증명을 지운다 (공장 초기화용) */
    static void resetSettings();

private:
    WifiConfig _cfg;
    uint32_t _lastRetryMs = 0;
};

}  // namespace ds

#endif
