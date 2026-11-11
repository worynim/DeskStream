// worynim@gmail.com
/**
 * @file Wireless_Analyzer.ino
 * @brief 2.4GHz 무선 환경 분석기 — ESP32-C3 무선부로 주변 WiFi·BLE를 측정해 512x64 화면에 보여준다
 * @details 공유기에 접속하지 않는 단독 장치. 비동기 WiFi 스캔 결과를 AP 테이블에 모아(60초 유지)
 *          채널 스펙트럼 · 채널 혼잡도 막대 · AP 목록 · AP 추적으로 표시한다.
 *          혼잡도 모드에서는 스캔 사이에 채널 1~13을 순회하며 실제 활동량(초당 제어·데이터 프레임)을 재고(scheduler),
 *          추천 채널에 반영한다.
 *          추적 모드에서는 선택 AP의 채널만 반복 스캔해 갱신을 빠르게 하고, 부저로 신호 세기를 알린다(기본 꺼짐).
 *          BLE 화면에서는 WiFi 대신 BLE 액티브 스캔만 반복해 근처 기기 수·이름·제조사를 보여준다(주소는 표시·전송하지 않음).
 *          요약 화면은 WiFi 스캔 → 채널 순회 → BLE 스캔을 모두 돌려 타일 4개로 보여준다.
 *          마지막 모드·밝기·화면 뒤집기는 NVS에 저장해 다시 켜도 유지한다 (ds::ConfigStore).
 *          버튼: BTN1/BTN2 짧게 = 이전/다음 모드, BTN1 길게 = 화면 180° 뒤집기, BTN2 길게 = 스캔 일시정지/재개,
 *                BTN3/BTN4 짧게 = 목록·추적에서 이전/다음 AP 선택, BTN3 길게 = 추적 부저 켜기/끄기, BTN4 길게 = 밝기 단계.
 */
#include <Arduino.h>
#include <DeskStreamCore.h>
#include <string.h>

#include "analysis.h"
#include "ble_scan.h"
#include "ble_table.h"
#include "config.h"
#include "scheduler.h"
#include "traffic.h"
#include "ui.h"
#include "wifi_scan.h"

static ds::DisplayCore display;
static ds::Buttons buttons;
static ds::Buzzer buzzer;
static ds::ConfigStore store;

// NVS에 저장하는 설정 — 키·타입을 바꾸면 저장값이 사라진다 (AI_GUIDE §1-7)
struct Settings {
    uint8_t mode = 0;       ///< 마지막 모드 (wa::Mode)
    uint8_t bright = 3;     ///< 밝기 단계 (kBrightLevels 인덱스)
    bool flip = false;      ///< 화면 180° 뒤집기
};
static Settings settings;
static const ds::ConfigField kFields[] = {
    DS_CFG_U8(Settings, mode, "mode", "mode", 0, (uint8_t)wa::Mode::Count - 1, nullptr),
    DS_CFG_U8(Settings, bright, "bright", "bright", 0, WA_BRIGHT_STEPS - 1, nullptr),
    DS_CFG_BOOL(Settings, flip, "flip", "flip", nullptr),
};
static const uint8_t kBrightLevels[WA_BRIGHT_STEPS] = WA_BRIGHT_LEVELS;

static wa::ApTable apTable;
static wa::ChannelStat chStats[wa::kMaxChannel + 1];
static wa::ChannelTraffic chTraffic[wa::kMaxChannel + 1];
static wa::BleTable bleTable;
static wa::BleCountHistory bleHist;

// 선택 AP — 목록 커서이자 추적 대상. 테이블에서 지워져도 마지막 정보를 유지한다
static wa::ApInfo selInfo;
static bool hasSel = false;
static wa::TrackHistory trackHist;
static uint32_t lastValidMs = 0;   // 추적 기록에 마지막으로 유효한 RSSI가 들어간 시각
static uint32_t lastBeepMs = 0;

static wa::UiState ui = {wa::Mode::Spectrum, false, 0, nullptr, &trackHist, false, chTraffic, false, &bleTable, &bleHist, 0,
                         false};
static bool dirty = true;
static uint32_t modeShownMs = 0;   // 모드 이름 알림을 띄운 시각

static void select(const wa::ApInfo& ap) {
    if (!hasSel || memcmp(selInfo.bssid, ap.bssid, 6) != 0) trackHist.clear();   // 대상이 바뀌면 추적 기록 새로
    selInfo = ap;
    hasSel = true;
    ui.sel = &selInfo;
    dirty = true;
}

/** RSSI 순 목록에서 선택을 delta 칸 옮긴다 (끝에서 멈춤) */
static void stepSelection(int delta) {
    uint8_t idx[wa::kApMax];
    const int n = apTable.sortedByRssi(idx, wa::kApMax);
    if (n == 0) return;
    int pos = 0;
    for (int k = 0; k < n; k++)
        if (hasSel && memcmp(apTable.at(idx[k]).bssid, selInfo.bssid, 6) == 0) pos = k;
    pos += delta;
    pos = pos < 0 ? 0 : (pos >= n ? n - 1 : pos);
    select(apTable.at(idx[pos]));
}

static void stepMode(int delta) {
    const int n = (int)wa::Mode::Count;
    ui.mode = (wa::Mode)(((int)ui.mode + delta + n) % n);
    if (ui.mode == wa::Mode::Track) trackHist.clear();   // 추적 기록은 추적 모드에 있는 동안만 쌓는다
    modeShownMs = millis();
    settings.mode = (uint8_t)ui.mode;
    store.setDirty();
    dirty = true;
}

static void applyFlip() {
    display.setFlip(settings.flip);   // 패널 180° — 화면 순서는 ui가 반대로 그린다
    ui.flipped = settings.flip;
    dirty = true;
}

static void toggleFlip() {
    settings.flip = !settings.flip;
    applyFlip();
    store.setDirty();
}

static void stepBrightness() {
    settings.bright = (settings.bright + 1) % WA_BRIGHT_STEPS;
    display.setBrightness(kBrightLevels[settings.bright]);
    store.setDirty();
}

static bool selectable() { return ui.mode == wa::Mode::List || ui.mode == wa::Mode::Track; }

static void pollButtons() {
    buttons.update();
    if (buttons.events(0) & ds::BUTTON_SHORT) stepMode(-1);
    if (buttons.events(0) & ds::BUTTON_LONG) toggleFlip();
    if (buttons.events(1) & ds::BUTTON_SHORT) stepMode(+1);
    if (buttons.events(3) & ds::BUTTON_LONG) stepBrightness();
    if (buttons.events(1) & ds::BUTTON_LONG) {
        ui.paused = !ui.paused;
        dirty = true;
    }
    if (selectable() && (buttons.events(2) & ds::BUTTON_SHORT)) stepSelection(-1);
    if (selectable() && (buttons.events(3) & ds::BUTTON_SHORT)) stepSelection(+1);
    if (ui.mode == wa::Mode::Track && (buttons.events(2) & ds::BUTTON_LONG)) {
        ui.buzzer = !ui.buzzer;
        dirty = true;
    }
}

/**
 * 시리얼 디버그 (화면 확인용)
 *   'd' : 4화면 버퍼를 "DUMP <화면> <1024바이트 hex>" 줄로 출력
 *   'm' : 다음 모드 (BTN2 짧게와 같음)
 *   'n' : 다음 AP 선택 (BTN4 짧게와 같음, 목록·추적 모드)
 *   'f' : 화면 180° 뒤집기 (BTN1 길게와 같음)
 *   't' : 채널별 실측 활동량(초당 제어·데이터 프레임)·측정 횟수 출력
 */
static void pollSerial() {
    while (Serial.available()) {
        const int ch = Serial.read();
        if (ch == 'm') stepMode(+1);
        if (ch == 'n' && selectable()) stepSelection(+1);
        if (ch == 'f') toggleFlip();
        if (ch == 't') {
            for (int c = wa::kMinChannel; c <= wa::kMaxChannel; c++)
                Serial.printf("TRAFFIC ch%2d fps=%6.1f sweeps=%u\n", c, chTraffic[c].fps, chTraffic[c].sweeps);
        }
        if (ch != 'd') continue;
        for (int s = 0; s < ds::kNumScreens; s++) {
            const uint8_t* buf = display.screen(s).getBufferPtr();
            Serial.printf("DUMP %d ", s);
            for (int i = 0; i < ds::kScreenBufBytes; i++) Serial.printf("%02x", buf[i]);
            Serial.println();
        }
    }
}

/** 스캔이 끝난 뒤 — 선택 AP 정보 갱신(없으면 가장 센 AP 선택), 추적 모드면 기록 추가 */
static void onScanDone(uint32_t now) {
    if (!hasSel) stepSelection(0);
    const int i = hasSel ? apTable.find(selInfo.bssid) : -1;
    if (i >= 0) selInfo = apTable.at(i);

    const uint8_t scanned = wa::wifiScanLastChannel();
    if (ui.mode == wa::Mode::Track && hasSel && (scanned == 0 || scanned == selInfo.channel)) {
        const bool seen = i >= 0 && selInfo.lastSeenMs == now;
        trackHist.push(seen ? selInfo.rssi : wa::kNoSample);
        if (seen) lastValidMs = now;
    }
}

/** 추적 모드 부저 — 마지막 RSSI가 셀수록 짧은 간격으로 삑 */
static void updateBuzzer(uint32_t now) {
    if (ui.mode != wa::Mode::Track || !ui.buzzer || ui.paused || trackHist.size() == 0) return;
    if (now - lastValidMs > WA_TRACK_STALE_MS) return;
    const int8_t v = trackHist.at(trackHist.size() - 1);
    if (v == wa::kNoSample) return;
    if (now - lastBeepMs >= wa::beepIntervalMs(v)) {
        buzzer.beep(WA_BEEP_MS, WA_BEEP_HZ);
        lastBeepMs = now;
    }
}

void setup() {
    Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(0);   // 시리얼 모니터를 안 열어도 loop()가 멈추지 않게
#endif
    display.begin();
    buttons.begin();
    buzzer.begin();
    store.begin("wanalyzer", &settings, kFields, sizeof(kFields) / sizeof(kFields[0]));
    ui.mode = (wa::Mode)settings.mode;
    display.setBrightness(kBrightLevels[settings.bright]);
    applyFlip();
    const uint32_t bootMs = millis();
    wa::uiDrawBoot(display, WA_FW_VERSION, settings.flip);
    wa::wifiScanBegin();
    wa::bleScanBegin();
    wa::computeChannelStats(apTable, chStats);
    while (millis() - bootMs < WA_BOOT_MS) delay(10);   // 부팅 화면을 보여줄 시간
    modeShownMs = millis();   // 첫 모드 이름도 띄운다
}

void loop() {
    pollButtons();
    pollSerial();
    const uint32_t now = millis();
    const bool bleMode = ui.mode == wa::Mode::Ble, summary = ui.mode == wa::Mode::Summary;
    const wa::SchedConfig cfg = {ui.paused, !bleMode,
                                 (uint8_t)((ui.mode == wa::Mode::Track && hasSel) ? selInfo.channel : 0),
                                 ui.mode == wa::Mode::Channels || summary, bleMode || summary};
    const uint8_t ev = wa::schedulerPoll(apTable, chTraffic, bleTable, cfg, now);
    if (ev & wa::kEvBleDone) {
        bleHist.push((int8_t)bleTable.count());
        ui.bleScans++;
    }
    if (ev & wa::kEvScanDone) {
        wa::computeChannelStats(apTable, chStats);
        ui.scans = wa::wifiScanCount();
        onScanDone(now);
    }
    if (ev) dirty = true;
    updateBuzzer(now);
    const bool toast = now - modeShownMs < WA_MODE_TOAST_MS;
    if (toast != ui.modeToast) {   // 알림이 뜨거나 사라질 때 다시 그린다
        ui.modeToast = toast;
        dirty = true;
    }
    static uint32_t lastDraw = 0;
    if (dirty && now - lastDraw >= WA_FRAME_MS) {
        dirty = false;
        lastDraw = now;
        wa::uiDraw(display, apTable, chStats, ui);
    }
    delay(5);
}
