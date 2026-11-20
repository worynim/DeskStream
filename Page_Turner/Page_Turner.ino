// worynim@gmail.com
/**
 * @file Page_Turner.ino
 * @brief DeskStream BLE 페이지 터너 — 세로로 쥐고 엄지로 웹소설·웹툰·쇼츠를 넘긴다 (설계: DESIGN.md)
 *
 * BTN1: 이전·위 (메뉴: 값 줄이기)      BTN2: 다음·아래 (메뉴: 값 늘리기), Camera는 딜레이 셔터
 * BTN3: 다음 모드 (메뉴: 취소), 길게 = 기기 선택 (페어링 대기 중 짧게 = 이전 슬롯으로)
 * BTN4: 설정 메뉴 열기 (메뉴: 다음 항목, 마지막에서 저장)
 * 기기 선택: BTN1~3 = 슬롯 1~3 고르기, 같은 버튼 한 번 더 = 그 슬롯 사용, 길게 = 그 슬롯에 새 기기 / BTN4 = 취소
 * 무입력 1분 = 화면 끄기(첫 버튼은 화면만 켬), 5분 = 딥슬립(BTN1·BTN2로 깨움)
 */
#include <Arduino.h>
#include <DeskStreamCore.h>
#include <NimBLEDevice.h>
#include <esp_sleep.h>

#include "config.h"
#include "hid_ble.h"
#include "menu.h"
#include "model.h"
#include "power_policy.h"
#include "settings_store.h"
#include "shutter_timer.h"
#include "view.h"

static ds::DisplayCore display;
static ds::Buttons buttons;
static ds::Buzzer buzzer;

static SettingsMenu menu;
static ShutterTimer shutter;
static int mode = 0;
static uint32_t lastInputMs = 0;
static bool screenOn = true;
static bool dirty = true;
static uint8_t pairingShown = 0;   // Pairing 화면에 보인 남은 초
static bool devMenu = false;       // 기기(슬롯) 선택 화면
static int devSel = -1;            // 고른 슬롯, -1 = 아직 안 고름

static void beep(int ms, int hz) {
    if (valueOf(beepSetting())) buzzer.beep(ms, hz);
}

// 모든 버튼 콜백의 첫 줄: 입력 시각을 갱신하고, 화면이 꺼져 있었으면 화면만 켜고 true (이 누름은 소비)
static bool wakeOnly() {
    lastInputMs = millis();
    if (screenOn) return false;
    screenOn = true;
    view::setScreensOn(true);
    dirty = true;
    return true;
}

// ---------- 신호 ----------
static void sendSignal(bool up) {
    const Mode& m = modeAt(mode);
    const Setting* s = m.settings;
    hid::Job j{m.signal, up, 1, 0, false};
    if (m.signal == Signal::Wheel) {
        j.count = valueOf(s[0]);
        j.gapMs = valueOf(s[1]);
        j.parkCursor = valueOf(s[2]);
    } else if (m.signal == Signal::Arrow) {
        j.count = valueOf(s[0]);
        j.gapMs = valueOf(s[1]);
    }
    if (hid::post(j)) beep(30, 3500);   // 미연결·전송 중이면 버리고 소리도 내지 않는다
}

static void toggleShutterTimer() {
    if (shutter.running()) {
        shutter.cancel();
        beep(40, 1500);
    } else {
        shutter.start(millis(), valueOf(modeAt(mode).settings[0]));
        beep(20, 2000);
    }
    dirty = true;
}

// ---------- 기기(슬롯) 선택 ----------
// 슬롯 버튼(BTN1~3) 짧게: 처음엔 고르기, 고른 슬롯을 한 번 더 누르면 그 슬롯 사용
static void devPress(int s) {
    if (devSel != s) {
        devSel = s;
        beep(15, 3000);
    } else {
        devMenu = false;
        hid::selectSlot(s, millis());
        pairingShown = 0;
        beep(60, 4000);
    }
    dirty = true;
}

// 슬롯 버튼 길게: 그 슬롯에 새 기기 페어링 대기 (기존 기기는 새 기기가 본딩될 때 교체)
static void devHold(int s) {
    devMenu = false;
    hid::repairSlot(s, millis());
    pairingShown = 0;
    beep(60, 2500);
    dirty = true;
}

static void onBtn1Long() {
    if (!wakeOnly() && devMenu) devHold(0);
}

static void onBtn2Long() {
    if (!wakeOnly() && devMenu) devHold(1);
}

// ---------- 버튼 ----------
static void onBtn1() {
    if (wakeOnly()) return;
    if (devMenu) {
        devPress(0);
        return;
    }
    if (hid::pairing()) return;
    if (menu.isOpen()) {
        menu.dec();
        beep(15, 3000);
        dirty = true;
        return;
    }
    sendSignal(true);
}

static void onBtn2() {
    if (wakeOnly()) return;
    if (devMenu) {
        devPress(1);
        return;
    }
    if (hid::pairing()) return;
    if (menu.isOpen()) {
        menu.inc();
        beep(15, 3000);
        dirty = true;
        return;
    }
    if (modeAt(mode).signal == Signal::Camera) toggleShutterTimer();
    else sendSignal(false);
}

static void onBtn3() {
    if (wakeOnly()) return;
    if (devMenu) {
        devPress(2);
        return;
    }
    if (hid::pairing()) {   // 페어링 대기 취소 → 이전 슬롯으로 (돌아갈 슬롯이 있을 때만)
        hid::cancelPairing();
        beep(40, 1500);
        dirty = true;
        return;
    }
    if (menu.isOpen()) {
        menu.cancel();
        beep(40, 1500);
    } else {
        shutter.cancel();
        mode = (mode + 1) % kNumModes;
        store::saveSoon(mode);
        beep(20, 2500);
    }
    dirty = true;
}

static void onBtn3Long() {
    if (wakeOnly() || menu.isOpen()) return;
    if (devMenu) {
        devHold(2);
        return;
    }
    shutter.cancel();
    devMenu = true;
    devSel = -1;
    beep(60, 2500);
    dirty = true;
}

static void onBtn4() {
    if (wakeOnly()) return;
    if (devMenu) {   // 기기 선택 취소
        devMenu = false;
        beep(40, 1500);
        dirty = true;
        return;
    }
    if (hid::pairing()) return;
    if (!menu.isOpen()) {
        shutter.cancel();
        menu.open(modeAt(mode));
        beep(20, 3000);
    } else if (menu.next()) {
        store::saveSoon(mode);
        beep(60, 4000);
    } else {
        beep(20, 3000);
    }
    dirty = true;
}

// ---------- 절전 ----------
static bool anyButtonDown() {
    for (int i = 0; i < ds::kNumButtons; i++) {
        if (digitalRead(ds::kButtonPins[i]) == LOW) return true;
    }
    return false;
}

static void enterSleep() {
    store::saveNow(mode);
    view::setScreensOn(true);
    view::drawSleep();
    delay(1000);
    while (anyButtonDown()) delay(10);   // 누른 채 들어가면 곧바로 깨므로 놓을 때까지 기다린다
    view::setScreensOn(false);
    // 딥슬립 GPIO 깨우기는 GPIO 0~5만 가능 → BTN1(1)·BTN2(4). 풀업은 esp_deep_sleep_start가 켠다
    esp_deep_sleep_enable_gpio_wakeup((1ULL << ds::kButtonPins[0]) | (1ULL << ds::kButtonPins[1]),
                                      ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_deep_sleep_start();
}

static void render(uint32_t now) {
    if (devMenu) {
        bool paired[hid::kNumSlots];
        for (int s = 0; s < hid::kNumSlots; s++) paired[s] = hid::slotPaired(s);
        view::drawDevices(devSel, hid::activeSlot(), paired);
    } else if (hid::pairing()) {
        view::drawPairing(hid::activeSlot(), hid::pairingSecondsLeft(now));
    } else if (menu.isOpen()) {
        view::drawMenu(menu);
    } else {
        view::drawNormal(mode, hid::activeSlot(), hid::connected(), shutter);
    }
}

void setup() {
    Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(0);
#endif
    ds::DisplayConfig cfg;
    cfg.rotation = U8G2_R1;   // 기본 R2(가로)에서 90° 돌려 세로로 쥐었을 때 OLED1이 위
    display.begin(cfg);
    display.setBrightness(kBrightness);
    view::begin(display);
    view::drawSplash();

    // 딥슬립에서 버튼으로 깨어났으면 그 누름이 신호가 되지 않도록 놓을 때까지 기다린다
    for (int i = 0; i < ds::kNumButtons; i++) pinMode(ds::kButtonPins[i], INPUT_PULLUP);
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
        while (anyButtonDown()) delay(10);
    }

    store::begin();
    mode = store::lastMode();

    buttons.begin();
    buttons.setCallbacks(0, onBtn1, onBtn1Long);
    buttons.setCallbacks(1, onBtn2, onBtn2Long);
    buttons.setCallbacks(2, onBtn3, onBtn3Long);
    buttons.setCallbacks(3, onBtn4, nullptr);
    buzzer.begin();

    hid::begin();
    lastInputMs = millis();
    Serial.println("Page_Turner " PT_VERSION " ready");
}

void loop() {
    const uint32_t now = millis();
    buttons.update();

    switch (shutter.tick(now)) {
        case ShutterTimer::Tick::Second:
            beep(20, 2000);
            dirty = true;
            break;
        case ShutterTimer::Tick::Fire:
            if (hid::post({Signal::Camera, true, 1, 0, false})) beep(60, 4000);   // 못 보냈으면 "찍힘" 소리를 내지 않는다
            dirty = true;
            break;
        case ShutterTimer::Tick::None:
            break;
    }

    hid::loop(now);
    if (hid::takeStateChanged()) dirty = true;
    if (hid::pairing()) {
        uint8_t sec = hid::pairingSecondsLeft(now);
        if (sec != pairingShown) {
            pairingShown = sec;
            dirty = true;
        }
    }

    switch (powerStage(now, lastInputMs)) {
        case PowerStage::Sleep:
            enterSleep();   // 돌아오지 않는다 (깨면 재부팅)
            break;
        case PowerStage::ScreenOff:
            if (screenOn) {
                menu.cancel();   // 메뉴 중이면 저장하지 않고 닫는다
                devMenu = false;
                screenOn = false;
                view::setScreensOn(false);
            }
            break;
        case PowerStage::Active:
            break;
    }

    if (dirty && screenOn) {
        dirty = false;
        render(now);
    }
    delay(2);
}
