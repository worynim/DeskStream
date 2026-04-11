#include <Arduino.h>
#include <WiFiManager.h>
#include <time.h>
#include "LittleFS.h"
#include "config.h"
#include "display_manager.h"
#include "hangeul_time.h"
#include <WebServer.h>
#include "web_manager.h"

DisplayManager display;
WebServer server(80);

/**
 * @brief WiFi 설정 모드(AP) 진입 시 호출되는 콜백
 */
void configModeCallback(WiFiManager *myWiFiManager) {
    Serial.println("[WIFI] Config Mode Entered");
    display.u8g2_1.clearBuffer();
    display.u8g2_1.setFont(u8g2_font_6x10_tf);
    display.u8g2_1.drawStr(0, 10, "WiFi Config Mode");
    display.u8g2_1.drawStr(0, 25, "SSID: Hangeul_Clock_Setup");
    display.u8g2_1.drawStr(0, 40, "IP: 192.168.4.1");
    display.u8g2_1.sendBuffer();
}

// 인터럽트 발생 여부를 기록하는 플래그
volatile bool btnInterruptFlags[4] = {false, false, false, false};

// ISR (Interrupt Service Routine) - 최소한의 처리만 보장
void IRAM_ATTR handleBtnInterrupt1() { btnInterruptFlags[0] = true; }
void IRAM_ATTR handleBtnInterrupt2() { btnInterruptFlags[1] = true; }
void IRAM_ATTR handleBtnInterrupt3() { btnInterruptFlags[2] = true; }
void IRAM_ATTR handleBtnInterrupt4() { btnInterruptFlags[3] = true; }

/**
 * @brief 인터럽트 대응 고성능 버튼 구조체
 */
struct Button {
    int pin;
    int id; // 0 ~ 3
    bool lastState;
    unsigned long fallTime;
    bool isPressed;
    bool isLongPressFired; 
    void (*onShortPress)();
    void (*onLongPress)();
    
    Button(int i, int p, void (*sp)(), void (*lp)()) : id(i), pin(p), lastState(HIGH), fallTime(0), isPressed(false), isLongPressFired(false), onShortPress(sp), onLongPress(lp) {}

    void init() { 
        pinMode(pin, INPUT_PULLUP); 
        if (id == 0) attachInterrupt(digitalPinToInterrupt(pin), handleBtnInterrupt1, CHANGE);
        else if (id == 1) attachInterrupt(digitalPinToInterrupt(pin), handleBtnInterrupt2, CHANGE);
        else if (id == 2) attachInterrupt(digitalPinToInterrupt(pin), handleBtnInterrupt3, CHANGE);
        else if (id == 3) attachInterrupt(digitalPinToInterrupt(pin), handleBtnInterrupt4, CHANGE);
    }

    void update() {
        // 인터럽트가 발생했거나 버튼이 현재 눌려 있는 경우(타이머 체크용) 업데이트 수행
        bool currentState = digitalRead(pin);
        unsigned long now = millis();

        if (lastState == HIGH && currentState == LOW) { // FALLING
            fallTime = now;
            isPressed = true;
            isLongPressFired = false;
        } 
        else if (currentState == LOW) { // STILL PRESSED
            if (isPressed && !isLongPressFired && (now - fallTime > 1000)) {
                isLongPressFired = true;
                if (onLongPress) onLongPress();
            }
        } 
        else if (lastState == LOW && currentState == HIGH) { // RISING
            if (isPressed && !isLongPressFired && (now - fallTime > 50)) { 
                if (onShortPress) onShortPress(); 
            }
            isPressed = false;
        }
        
        lastState = currentState;
        btnInterruptFlags[id] = false; // 플래그 초기화
    }
};

int uiStage = 0; // 0: Clock, 1: IP, 2: Button Help

// --- 버튼 콜백 정의 ---
void btn1_short() {
    display.beep(50, 3000);
    display.setChime(!display.chime_enabled);
    if (uiStage == 2) {
        display.showButtonHelp(); // 도움말 화면 즉시 갱신 (v1.3.52)
    }
}

void btn1_long() {
    display.beep(150, 2000);
    display.setFlipDisplay(!display.is_flipped);
    display.clearAll(); // 방향 전환 시 잔상 제거
    if (uiStage == 1) {
        display.showLargeIP(WiFi.localIP()); // IP 화면 즉시 갱신 (v1.3.51)
    } else if (uiStage == 2) {
        display.showButtonHelp(); // 도움말 화면 즉시 갱신
    }
}

void btn2_short() {
    display.beep(50, 3000);
    uint8_t nextMode = (display.display_mode == CLOCK_MODE_HANGUL) ? CLOCK_MODE_NUMERIC : CLOCK_MODE_HANGUL;
    display.setDisplayMode(nextMode);
    display.clearAll(); // 한글/숫자 전환 시 잔상 제거 (v1.3.48)
}

void btn2_long() {
    display.beep(150, 2000);
    uint8_t nextFormat = (display.hour_format == HOUR_FORMAT_12H) ? HOUR_FORMAT_24H : HOUR_FORMAT_12H;
    display.setHourFormat(nextFormat);
    display.clearAll();
}

void btn3_short() { 
    display.beep(50, 3000); 
    uint8_t nextAnim = (display.anim_mode + 1) % 6;
    display.setAnimMode(nextAnim);
    // 애니메이션 모드 전환은 다음 렌더링 시 자동 반영되므로 강제 clear 지양
}
void btn3_long()  { }

void btn4_short() {
    display.beep(50, 3000);
    uiStage = (uiStage + 1) % 3; // 0(시계) -> 1(IP) -> 2(도움말) -> 0(시계)
    
    display.clearAll(); // 화면 전환(시계 <-> IP <-> 도움말) 시 잔상 즉시 제거 (v1.3.48)

    if (uiStage == 1) {
        display.showLargeIP(WiFi.localIP());
    } else if (uiStage == 2) {
        display.showButtonHelp();
    }
}
void btn4_long() { }

Button btns[4] = {
    Button(0, BTN1_PIN, btn1_short, btn1_long),
    Button(1, BTN2_PIN, btn2_short, btn2_long),
    Button(2, BTN3_PIN, btn3_short, btn3_long),
    Button(3, BTN4_PIN, btn4_short, btn4_long)
};

// 애니메이션 중 등 빈번하게 호출되어 사용자 인터프리트 보장
void on_yield() {
    server.handleClient();
    for (int i = 0; i < 4; i++) {
        // 인터럽트 플래그가 섰거나 버튼이 눌린 상태면 업데이트
        if (btnInterruptFlags[i] || btns[i].isPressed) {
            btns[i].update();
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n[SYSTEM] Starting Hangeul Clock v1.3.4 (Interrupt Driven)");

    if (!LittleFS.begin(true)) Serial.println("[SYSTEM] LittleFS Mount Failed");

    // 1. 버튼 및 인터럽트 설정
    for (int i = 0; i < 4; i++) btns[i].init();

    // 2. 디스플레이 초기화
    display.begin();
    display.setYieldCallback(on_yield);
    display.showStatus("Initializing...");

    // 3. WiFiManager 및 NTP 설정
    WiFiManager wm;
    wm.setAPCallback(configModeCallback);
    wm.setConfigPortalTimeout(120);
    
    // 부팅 시 BTN1 강제 리셋 기능
    if (digitalRead(BTN1_PIN) == LOW) {
        display.showStatus("WiFi Resetting...");
        wm.resetSettings();
        delay(1000);
    }

    display.showStatus("WiFi Connecting...");
    if (!wm.autoConnect(WIFI_SSID_AP)) ESP.restart();

    display.showStatus("WiFi Connected!");
    startWebServer();

    configTime(TIMEZONE_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER1, NTP_SERVER2);
    display.showStatus("Syncing Time...");
    
    struct tm timeinfo;
    int retry = 0;
    while (!getLocalTime(&timeinfo) && retry < 10) { delay(1000); retry++; }
    
    if (retry < 10) display.showStatus("Time Sync OK!");
    else display.showStatus("Time Sync Fail!");
    
    delay(1000);
}

// 강제 화면 갱신 트리거
unsigned long forceUpdateTrigger = 0;

void handleClockUpdate(bool force = false) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        display.showStatus("Time Sync Error!");
        return;
    }

    // [시보] 정시 알림 로직
    if (display.chime_enabled && timeinfo.tm_min == 0 && timeinfo.tm_sec == 0) {
        static int lastChimeHour = -1;
        if (lastChimeHour != timeinfo.tm_hour) {
            display.beep(500, 1000);
            lastChimeHour = timeinfo.tm_hour;
        }
    }

    int h = timeinfo.tm_hour, m = timeinfo.tm_min, s = timeinfo.tm_sec, d = timeinfo.tm_mday;
    String texts[4];
    bool isHangul = (display.display_mode == CLOCK_MODE_HANGUL);
    bool is24H = (display.hour_format == HOUR_FORMAT_24H);

    // 텍스트 생성 (v1.4.0 최적화 구조)
    if (is24H) texts[0] = isHangul ? HangeulTimeConverter::getDay(d) : HangeulTimeConverter::getNumericDay(d);
    else texts[0] = HangeulTimeConverter::getAmPm(h);

    texts[1] = isHangul ? HangeulTimeConverter::getHour(h, is24H) : HangeulTimeConverter::getNumericHour(h, is24H);
    texts[2] = isHangul ? HangeulTimeConverter::getMinute(m) : HangeulTimeConverter::getNumericMinute(m);
    texts[3] = isHangul ? HangeulTimeConverter::getSecond(s) : HangeulTimeConverter::getNumericSecond(s);

    display.updateAll(texts, force);
}

void loop() {
    on_yield(); 

    static unsigned long lastUpdate = 0;
    unsigned long now = millis();

    // IP/도움말 모드가 아니고, (1초가 지났거나 강제 트리거가 발생했을 때) 갱신
    if (uiStage == 0 && (forceUpdateTrigger || (now - lastUpdate >= UPDATE_INTERVAL_MS))) {
        handleClockUpdate(forceUpdateTrigger > 0);
        forceUpdateTrigger = 0;
        lastUpdate = now;
    }
}
