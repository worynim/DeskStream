// worynim@gmail.com
/**
 * @file ds_wifi.cpp
 * @brief WiFi 연결 서비스 구현 (자체 설정 포털 — 캡티브 DNS + 웹 페이지)
 */
#include "ds_wifi.h"
#include "ds_wifi_portal_util.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

namespace ds {

static const IPAddress kPortalIp(192, 168, 4, 1);
static constexpr int kMaxListedNetworks = 20;   // 포털 목록에 보여 줄 SSID 수 (신호 세기 순)

/** 이스케이프한 문자열을 덧붙인다 (SSID·AP 이름은 누구나 정할 수 있는 값이다) */
static void appendEscaped(String& html, const char* s) {
    char buf[kWifiSsidMaxLen * 6 + 1];   // 최악: 모든 글자가 "&quot;"(6바이트)
    if (htmlEscapeTo(s, buf, sizeof(buf))) html += buf;
}

static const char PAGE_HEAD[] PROGMEM =
    "<!doctype html><html lang=ko><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'><title>WiFi 설정</title><style>"
    "body{font-family:sans-serif;margin:0;padding:20px;background:#111;color:#eee;max-width:420px}"
    "h2{margin:0 0 4px}.dev{color:#999;margin:0 0 16px}.err{background:#5a1d1d;padding:10px;border-radius:6px}"
    ".net{display:block;width:100%;text-align:left;margin:4px 0;padding:10px;border:1px solid #333;"
    "border-radius:6px;background:#1c1c1c;color:#eee;font-size:15px}"
    "input,.go{width:100%;box-sizing:border-box;margin:6px 0;padding:12px;border-radius:6px;border:1px solid #444;"
    "background:#1c1c1c;color:#eee;font-size:16px}.go{background:#2e7d32;border:0;font-weight:bold}"
    "a{color:#8ab4f8}</style></head><body>";

/** 설정 페이지 — 주변 네트워크 목록(신호 세기 순, 중복 SSID는 가장 센 것 하나) + 입력 폼 */
static String buildPage(const char* apName, const String& error) {
    String html;
    html.reserve(3072);
    html += FPSTR(PAGE_HEAD);
    html += "<h2>WiFi 설정</h2><p class=dev>";
    appendEscaped(html, apName);
    html += "</p>";
    if (error.length()) { html += "<p class=err>"; html += error; html += "</p>"; }   // 이 파일의 고정 문구만 들어온다

    html += "<p>네트워크를 고르거나 이름을 직접 입력하세요.</p>";
    const int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) {
        html += "<p>주변 네트워크를 검색하는 중… <a href=/>새로고침</a></p>";
    } else if (n <= 0) {
        html += "<p>찾은 네트워크가 없습니다.</p>";
    } else {
        // 신호 세기 순으로 고르되 같은 SSID는 한 번만 (선택 정렬 — 최대 kMaxListedNetworks개)
        bool used[64] = {false};
        const int total = n < 64 ? n : 64;
        for (int shown = 0; shown < kMaxListedNetworks; shown++) {
            int best = -1;
            for (int i = 0; i < total; i++) {
                if (used[i] || WiFi.SSID(i).length() == 0) continue;
                if (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best)) best = i;
            }
            if (best < 0) break;
            const String ssid = WiFi.SSID(best);
            for (int i = 0; i < total; i++) if (WiFi.SSID(i) == ssid) used[i] = true;
            html += "<button type=button class=net onclick='f(this)' data-s=\"";
            appendEscaped(html, ssid.c_str());
            html += "\">";
            appendEscaped(html, ssid.c_str());
            html += WiFi.encryptionType(best) == WIFI_AUTH_OPEN ? "" : " 🔒";
            html += " <small>(" + String(WiFi.RSSI(best)) + " dBm)</small></button>";
        }
    }
    html += "<p><a href=/scan>↻ 다시 검색</a></p>"
            "<form method=post action=/save>"
            "<input id=s name=s maxlength=32 placeholder='네트워크 이름 (SSID)' required autocomplete=off>"
            "<input id=p name=p type=password maxlength=63 placeholder='비밀번호 (열린 네트워크면 비워 둠)'>"
            "<button class=go>연결</button></form>"
            "<script>function f(b){document.getElementById('s').value=b.dataset.s;"
            "document.getElementById('p').focus()}</script></body></html>";
    return html;
}

/** 연결 실패 이유 (포털 페이지에 보여 준다) */
static const char* failReason(wl_status_t st) {
    switch (st) {
        case WL_NO_SSID_AVAIL:  return "네트워크를 찾을 수 없습니다. 이름을 확인하고 공유기 가까이에서 다시 시도하세요.";
        case WL_CONNECT_FAILED: return "비밀번호가 틀렸거나 공유기가 연결을 거부했습니다.";
        default:                return "제한 시간 안에 연결되지 않았습니다. 다시 시도하세요.";
    }
}

bool WifiService::begin(const WifiConfig& cfg) {
    _cfg = cfg;

    // WiFi.begin()(무인자)은 NVS에 저장된 AP로 연결만 걸고 즉시 반환한다.
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    const uint32_t start = millis();
    uint32_t tick = 0;
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < _cfg.connectTimeoutMs) {
        if (_cfg.onConnecting) _cfg.onConnecting(tick);
        delay(500);
        tick++;
    }

    // 미연결 → 저장된 SSID 재시도 없이 곧바로 설정 포털
    const bool ok = (WiFi.status() == WL_CONNECTED) || runPortal();
    if (!ok && _cfg.restartOnFail) ESP.restart();
    _lastRetryMs = millis();
    return ok;
}

bool WifiService::runPortal() {
    Serial.printf("[WIFI] Config portal: AP '%s' (http://192.168.4.1)\n", _cfg.apName);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(kPortalIp, kPortalIp, IPAddress(255, 255, 255, 0));
    WiFi.softAP(_cfg.apName);

    // 캡티브 DNS: 모든 도메인을 이 기기로 돌려 휴대폰의 "로그인 필요" 페이지가 뜨게 한다
    DNSServer dns;
    dns.setErrorReplyCode(DNSReplyCode::NoError);
    dns.start(53, "*", kPortalIp);

    WebServer web(80);
    String error;
    String reqSsid, reqPass;
    bool requested = false;

    WiFi.scanNetworks(true);   // 비동기 검색 — 페이지를 열 때 결과가 준비돼 있도록

    web.on("/", HTTP_GET, [&]() {
        web.send(200, "text/html; charset=utf-8", buildPage(_cfg.apName, error));
    });
    web.on("/scan", HTTP_GET, [&]() {
        WiFi.scanDelete();
        WiFi.scanNetworks(true);
        web.sendHeader("Location", "/", true);
        web.send(302, "text/plain", "");
    });
    web.on("/save", HTTP_POST, [&]() {
        const String ssid = web.arg("s");
        const String pass = web.arg("p");
        if (!wifiSsidValid(ssid.c_str())) {
            error = "네트워크 이름은 1~32바이트여야 합니다.";
        } else if (!wifiPassValid(pass.c_str())) {
            error = "비밀번호는 8~63자여야 합니다 (열린 네트워크면 비워 두세요).";
        } else {
            error = "";
            reqSsid = ssid;
            reqPass = pass;
            requested = true;
            String html;
            html += FPSTR(PAGE_HEAD);
            html += "<meta http-equiv=refresh content='20;url=/'><h2>연결하는 중…</h2><p>'";
            appendEscaped(html, ssid.c_str());
            html += "'에 연결하고 있습니다. 기기 화면을 확인하세요.</p>"
                    "<p>연결되면 설정용 WiFi가 꺼집니다. 실패하면 이 페이지가 다시 열리며 이유를 알려 줍니다.</p></body></html>";
            web.send(200, "text/html; charset=utf-8", html);
            return;
        }
        web.sendHeader("Location", "/", true);
        web.send(303, "text/plain", "");
    });
    // 그 밖의 모든 주소(휴대폰 OS의 연결 확인 주소 포함)는 설정 페이지로 보낸다
    web.onNotFound([&]() {
        web.sendHeader("Location", "http://192.168.4.1/", true);
        web.send(302, "text/plain", "");
    });
    web.begin();

    if (_cfg.onPortal) _cfg.onPortal(_cfg.apName);

    const uint32_t start = millis();
    bool connected = false;
    while (!connected) {
        dns.processNextRequest();
        web.handleClient();
        delay(2);

        if (requested) {
            requested = false;
            delay(300);   // "연결하는 중" 응답이 휴대폰에 도착할 시간
            Serial.printf("[WIFI] Portal: connecting to '%s'\n", reqSsid.c_str());
            // WiFi.begin(ssid, pass)는 자격증명을 NVS에 저장한다 (다음 부팅부터 WiFi.begin()으로 연결)
            WiFi.begin(reqSsid.c_str(), reqPass.length() ? reqPass.c_str() : nullptr);
            const uint32_t t0 = millis();
            while (WiFi.status() != WL_CONNECTED && (millis() - t0) < _cfg.connectTimeoutMs) {
                dns.processNextRequest();
                web.handleClient();
                delay(10);
            }
            if (WiFi.status() == WL_CONNECTED) {
                connected = true;
            } else {
                error = failReason(WiFi.status());
                Serial.printf("[WIFI] Portal: failed (%d)\n", (int)WiFi.status());
                WiFi.disconnect(false, false);   // 실패한 시도 중단 (AP는 유지)
                WiFi.scanDelete();
                WiFi.scanNetworks(true);
            }
        }

        if (_cfg.portalTimeoutS && (millis() - start) > (uint32_t)_cfg.portalTimeoutS * 1000UL) break;
    }

    web.stop();
    dns.stop();
    WiFi.scanDelete();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    Serial.println(connected ? "[WIFI] Portal: connected" : "[WIFI] Portal: timeout");
    return connected;
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
    // 저장된 AP 정보(NVS)를 지운다 — disconnect(wifioff, eraseap)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
}

}  // namespace ds
