// worynim@gmail.com
/**
 * @file ds_wifi.h
 * @brief WiFi 연결 서비스 (저장된 AP 비차단 접속 → 실패 시 자체 설정 포털 → 끊김 재접속)
 * @details
 *   1. WiFi.begin()(무인자)으로 NVS에 저장된 AP에 연결만 걸고, connectTimeoutMs 동안 진행 콜백을 부르며 기다린다.
 *   2. 못 붙으면 **설정 포털**을 연다 — 열린 AP(apName) + 캡티브 DNS(모든 도메인 → 192.168.4.1) + 웹 페이지.
 *      휴대폰이 AP에 붙으면 설정 페이지가 자동으로 뜨고(안 뜨면 브라우저로 192.168.4.1), 주변 SSID 목록에서
 *      고르거나 직접 입력한 뒤 비밀번호를 넣으면 그 자리에서 연결을 시도한다. 실패하면 페이지에 이유를 보여 준다.
 *   3. 자격증명은 WiFi.begin(ssid, pass)가 ESP32 NVS(nvs.net80211)에 저장한다 — WiFiManager가 쓰던 곳과 같아서
 *      **이미 설정된 기기는 그대로 연결된다.**
 * @note 예전엔 WiFiManager 라이브러리로 포털을 띄웠다(웹서버가 있는 프로젝트 기준 약 88KB). 쓰는 기능이
 *       "SSID·비밀번호 받아 연결"뿐이라 필요한 만큼만 직접 구현했다 (DeskStreamCore v0.6.0).
 * @note 출처: Hangeul_Clock.ino setup()의 WiFi 시퀀스 + time_sync.cpp의 재접속 (v2.10.0)
 */
#ifndef DS_WIFI_H
#define DS_WIFI_H

#include <Arduino.h>

namespace ds {

struct WifiConfig {
    const char* apName = "DeskStream_Setup";   ///< 설정 포털 SSID (암호 없는 열린 AP)
    uint32_t connectTimeoutMs = 10000;         ///< 저장된 AP 접속 대기 / 포털에서 입력한 AP 접속 대기
    uint16_t portalTimeoutS = 120;             ///< 포털 대기 (0 = 무제한). 넘으면 실패
    bool restartOnFail = true;                 ///< 포털까지 실패하면 재부팅 (false면 오프라인으로 계속)
    uint32_t reconnectIntervalMs = 60000;      ///< 끊긴 동안 WiFi.reconnect() 간격
    /** 접속 대기 중 500ms마다 호출 (tick = 0,1,2…) — 진행 점 표시용 */
    void (*onConnecting)(uint32_t tick) = nullptr;
    /** 설정 포털(AP)을 열 때 호출 — 화면에 AP 이름·IP 안내용 */
    void (*onPortal)(const char* apName) = nullptr;
};

class WifiService {
public:
    /** @return 연결 성공 여부 (restartOnFail이면 실패 시 반환하지 않고 재부팅) */
    bool begin(const WifiConfig& cfg);

    /** 메인 루프에서 호출 — 끊겨 있으면 reconnectIntervalMs마다 재접속을 시도한다 */
    void loop();

    bool connected() const;

    /** NVS에 저장된 WiFi 자격증명을 지운다 (공장 초기화용) */
    static void resetSettings();

private:
    /** 설정 포털을 열고 연결되거나 시간이 다 될 때까지 머문다. 연결되면 true */
    bool runPortal();

    WifiConfig _cfg;
    uint32_t _lastRetryMs = 0;
};

}  // namespace ds

#endif
