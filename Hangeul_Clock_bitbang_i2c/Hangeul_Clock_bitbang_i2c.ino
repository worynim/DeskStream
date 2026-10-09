// worynim@gmail.com
/**
 * @file Hangeul_Clock_bitbang_i2c.ino
 * @brief 한글 시계 메인 엔트리 포인트 (BitBang I2C · 4× 1.3" SSD1315)
 * @details 시스템 초기화(Setup), 메인 서비스 루프 제어 및 전역 서비스 통합 관리
 * @note [BB v1.0.0] Hangeul_Clock v2.9.3을 복사해 전송 계층만 BitBang으로 교체했다.
 *       앱 로직(버튼·시보·UI 스테이지·NTP)은 원본과 동일하다.
 *         · I2C   : HW/SW 이원화 → 4버스 동시 BitBang (i2c_platform.cpp)
 *         · OLED  : SSD1306 → SSD1315 (display_manager.h)
 *         · 핀맵  : SCL 10 공유 + SDA 5/6/7/8, 버튼 0/1/3/4, 부저 없음 (config.h)
 * @note [SYNC] ENG_Clock/ENG_Clock.ino — 고정 오프셋 NTP를 POSIX TZ 기반
 *       DisplayManager::applyTimezone()로 교체
 */
#include <Arduino.h>
#include <WiFiManager.h>
#include <time.h>
#include "LittleFS.h"
#include "config.h"
#include "display_manager.h"
#include "hangeul_time.h"
#include <WebServer.h>
#include "web_manager.h"
#include "input_manager.h"
#include "logger.h"
#include "bb_diagnostic.h"   // BB_DIAG_MODE=1 일 때만 실제 코드가 컴파일된다

/**
 * @brief WiFi 설정 모드(AP) 진입 시 호출되는 콜백
 */
void configModeCallback(WiFiManager *myWiFiManager) {
    Serial.println("[WIFI] Config Mode Entered");
    display.u8g2_1.clearBuffer();
    display.u8g2_1.setFont(u8g2_font_6x10_tf);
    display.u8g2_1.drawStr(0, 12, "WiFi Config Mode");
    // AP 이름은 config.h의 WIFI_SSID_AP와 같아야 한다 (두 곳에 적으면 어긋난다).
    //   한 줄에 "SSID: "까지 붙이면 128px를 넘어 잘리므로 이름만 따로 그린다.
    display.u8g2_1.drawStr(0, 30, WIFI_SSID_AP);
    display.u8g2_1.drawStr(0, 48, "IP: 192.168.4.1");
    // ⚠ sendBuffer()를 직접 부르면 안 된다 — 전송 계층이 캡처 모드로 바뀌는 도중에
    //   같은 버스를 직접 몰면 두 경로가 SCL/SDA를 다퉈 **4화면 전체**가 깨진다.
    //   pushParallel()은 diff → 캡처 → 재생을 한 흐름으로 처리하므로 안전하다.
    display.pushParallel();
}

int uiStage = 0; // 0: Clock, 1: IP, 2: Button Help

// --- 버튼 콜백 정의 ---
void btn1_short() {
    display.beep(50, 3000);
    display.setChime(!configManager.get().chime_enabled);
    if (uiStage == 2) display.showButtonHelp();
}

void btn1_long() {
    display.beep(150, 2000);
    display.setFlipDisplay(!configManager.get().is_flipped);
    display.clearAll();
    if (uiStage == 1) display.showLargeIP(WiFi.localIP());
    else if (uiStage == 2) display.showButtonHelp();
}

void btn2_short() {
    display.beep(50, 3000);
    if (!renderer.isCacheLoaded()) {
        display.showStatus("Font Required!");
        return;
    }
    uint8_t nextMode = (configManager.get().display_mode == CLOCK_MODE_HANGUL) ? CLOCK_MODE_NUMERIC : CLOCK_MODE_HANGUL;
    display.setDisplayMode(nextMode);
    display.clearAll();
    if (uiStage == 1) display.showLargeIP(WiFi.localIP());
    else if (uiStage == 2) display.showButtonHelp();
}

void btn2_long() {
    display.beep(150, 2000);
    uint8_t nextFormat = (configManager.get().hour_format == HOUR_FORMAT_12H) ? HOUR_FORMAT_24H : HOUR_FORMAT_12H;
    display.setHourFormat(nextFormat);
    display.clearAll();
    if (uiStage == 1) display.showLargeIP(WiFi.localIP());
    else if (uiStage == 2) display.showButtonHelp();
}

void btn3_short() {
    display.beep(50, 3000);
    uint8_t nextAnim = (configManager.get().anim_mode + 1) % ANIMATION_TYPE_COUNT;
    display.setAnimMode(nextAnim);
    if (uiStage == 2) display.showButtonHelp();
}

void btn3_long() {
    display.beep(150, 2000);
    uint8_t nextSlot = (configManager.get().font_slot + 1) % FONT_SLOT_COUNT;
    display.setFontSlot(nextSlot);

    // 도움말 페이지에서는 페이지 갱신으로 현재 슬롯 표시, 그 외에는 상태 메시지 출력
    if (uiStage == 2) {
        display.showButtonHelp();
    } else {
        char msg[16];
        sprintf(msg, "Font Slot %d", nextSlot);
        display.showStatus(msg);
    }
}

void btn4_short() {
    display.beep(50, 3000);
    uiStage = (uiStage + 1) % UI_STAGE_COUNT;
    display.clearAll();
    if (uiStage == 1) display.showLargeIP(WiFi.localIP());
    else if (uiStage == 2) display.showButtonHelp();
}

void btn4_long() {
    display.beep(150, 2000);
    bool nextInv = !configManager.get().is_inverted;
    display.setInversion(nextInv);
    display.showStatus(nextInv ? "Invert: ON" : "Invert: OFF");
}

/**
 * @brief 시스템 양보 및 입력 동기화 (애니메이션 루프 등에서 빈번히 호출됨)
 */
void on_yield() {
    webManager.handleClient();
    inputManager.update();
}

void setup() {
    Serial.begin(115200);

    // 버튼 입력 준비 (공장 초기화·진단 트리거 판정에 함께 쓴다)
    pinMode(BTN1_PIN, INPUT_PULLUP);
    pinMode(BB_DIAG_BUTTON_PIN, INPUT_PULLUP);
    delay(300); // 핀 및 하드웨어 안정화 대기

    // 진단 모드 — BTN4를 누른 채 부팅하면 들어간다(돌아오지 않는다).
    //   컴파일 스위치가 아니라 **런타임 트리거**다 — 스위치가 되돌아가 시계 대신
    //   진단이 도는 상황을 원천적으로 없앤다 (config.h BB_DIAG_BUTTON_PIN 주석 참조).
    if (bbDiagnosticRequested()) {
        Serial.printf("[SYSTEM] Hangeul_Clock_bitbang_i2c v%s — DIAGNOSTIC\n", FW_VERSION);
        runBitBangDiagnostics();   // 돌아오지 않는다
    }

    // 이 줄이 시리얼에 보이면 **정상 시계 펌웨어**가 돌고 있다는 뜻이다.
    Serial.printf("[SYSTEM] Hangeul_Clock_bitbang_i2c v%s OLED=%s frame=%s screenRev=%d\n",
                  FW_VERSION, OLED_DRIVER_NAME,
                  (BB_FRAME_PATH == BB_FRAME_PATH_U8G2) ? "U8G2" : "CONCURRENT",
                  (int)BB_SCREEN_ORDER_REVERSED);

    // 1. 시스템 공장 초기화 감지 (최우선 순위)
    
    if (digitalRead(BTN1_PIN) == LOW) {
        display.begin(); // OLED 피드백을 위해 먼저 초기화
        display.showStatus("System Resetting...");
        logger.addLog("!!! FACTORY RESET !!!");
        
        // 파일 시스템 드라이버 활성화
        LittleFS.begin(true);
        
        logger.addLog("LittleFS Formatting...");
        if (LittleFS.format()) {
            logger.addLog("Format Success!");
        } else {
            logger.addLog("Format Failed!");
        }
        LittleFS.end();
        
        logger.addLog("WiFi Resetting...");
        WiFiManager wm; // 로컬 객체로 생성하여 초기화
        wm.resetSettings();
        
        logger.addLog("Reset Complete!");
        delay(2000);
        ESP.restart();
    }

    // 2. 일반 부팅 시퀀스
    if (!LittleFS.begin(true)) Serial.println("[SYSTEM] LittleFS Mount Failed");

    // 입력 및 로깅 시스템 초기화
    inputManager.begin();
    inputManager.setCallbacks(0, btn1_short, btn1_long);
    inputManager.setCallbacks(1, btn2_short, btn2_long);
    inputManager.setCallbacks(2, btn3_short, btn3_long);
    inputManager.setCallbacks(3, btn4_short, btn4_long);

    // 디스플레이 초기화 및 시작 피드백
    display.begin();
    display.playStartupMelody(); 
    
    // 부팅 로그 — 브링업에서 "어느 펌웨어가 도는가"를 화면과 시리얼 양쪽에서 확인한다.
    //   화면(6x10 폰트)은 21자가 상한이라 두 줄로 나눈다.
    logger.addLog("Hangeul Clock BB");
    logger.addLog("v" FW_VERSION " BitBang");   // v1.0.5
    Serial.printf("[SYSTEM] Hangeul_Clock_bitbang_i2c v%s\n", FW_VERSION);
    
    // 비트맵 캐시 로딩 (이 작업이 무거워서 이전에는 버튼 감지를 방해함)
    display.loadBitmapCache();
    display.setYieldCallback(on_yield);

    // WiFi 연결 시퀀스
    WiFiManager wm;
    wm.setAPCallback(configModeCallback);
    wm.setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT);
    
    logger.addLog("WiFi Connecting");
    
    int dotCount = 0;
    while (WiFi.status() != WL_CONNECTED && dotCount < 10) {
        String dots = "WiFi Connecting";
        for (int j = 0; j <= dotCount % 5; j++) dots += ".";
        logger.updateLastLog(dots);
        if (wm.autoConnect(WIFI_SSID_AP)) break;
        delay(500);
        dotCount++;
    }

    if (WiFi.status() != WL_CONNECTED) ESP.restart();

    logger.addLog("WiFi Connected!");
    webManager.begin();

    // 5. 시간 동기화 (NTP + POSIX TZ)
    // configManager.begin()이 display.begin() 내부에서 이미 끝났으므로
    // 이 시점의 configManager.get().timezone은 NVS에서 로드·검증된 값이다.
    display.applyTimezone();
    logger.addLog("Syncing Time");
    
    struct tm timeinfo;
    int retry = 0;
    while (!getLocalTime(&timeinfo) && retry < 15) { 
        String dots = "Syncing Time";
        for (int j = 0; j <= retry % 5; j++) dots += ".";
        logger.updateLastLog(dots);
        delay(1000); 
        retry++; 
    }
    
    if (retry < 15) logger.addLog("Time Sync OK!");
    else logger.addLog("Time Sync Fail!");
    
    delay(1000);
    display.clearAll(); // [Step 5.1] 부팅 로그 종료 후 시계 시작 전 잔상 완벽 소거
}


void handleClockUpdate(bool force = false) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        display.showStatus("Time Sync Error!");
        return;
    }

    // [시보] 정시 알림 로직
    if (configManager.get().chime_enabled && timeinfo.tm_min == 0 && timeinfo.tm_sec == 0) {
        static int lastChimeHour = -1;
        if (lastChimeHour != timeinfo.tm_hour) {
            display.playChimeMelody();
            lastChimeHour = timeinfo.tm_hour;
        }
    }

    int h = timeinfo.tm_hour, m = timeinfo.tm_min, s = timeinfo.tm_sec, d = timeinfo.tm_mday;
    String texts[4];
    // 커스텀 폰트 캐시가 로드되지 않은 경우 강제로 숫자 모드 사용 (기본 폰트 글자 부족 대응)
    bool isHangul = (configManager.get().display_mode == CLOCK_MODE_HANGUL) && renderer.isCacheLoaded();
    bool is24H = (configManager.get().hour_format == HOUR_FORMAT_24H);

    // 텍스트 생성 (v1.4.0 최적화 구조)
    if (is24H) texts[0] = isHangul ? HangeulTimeConverter::getDay(d) : HangeulTimeConverter::getNumericDay(d);
    else texts[0] = HangeulTimeConverter::getAmPm(h);

    texts[1] = isHangul ? HangeulTimeConverter::getHour(h, is24H) : HangeulTimeConverter::getNumericHour(h, is24H);
    texts[2] = isHangul ? HangeulTimeConverter::getMinute(m) : HangeulTimeConverter::getNumericMinute(m);
    texts[3] = isHangul ? HangeulTimeConverter::getSecond(s) : HangeulTimeConverter::getNumericSecond(s);

    // [Step 5.3] '정각' 중복 표시 방지
    //   getMinute(0)과 getSecond(0)이 **둘 다** "정각"을 반환한다. 0분0초에 그대로 두면
    //   화면2와 화면3에 "정각"이 동시에 뜨므로, 화면3을 비워 화면2가 담당하게 한다.
    //   → getMinute()의 "정각" 분기가 바뀌면 이 조건도 같이 바꿔야 한다. (두 곳이 짝)
    //   참고: 화면3은 0분0초 여부와 무관하게 매초 애니메이션을 돌린다(UPDATE_INTERVAL_MS).
    //   따라서 빈 문자열로 바꾸어도 전환 횟수는 늘지 않는다 — 깜빡임이 생기는 것은 아니다.
    if (isHangul && m == 0 && s == 0) {
        texts[3] = "";
    }

    display.updateAll(texts, force);
}

// 루프 시작
void loop() {
    on_yield(); 
    display.updateTick(); 

    static unsigned long lastUpdate = 0;
    unsigned long now = millis();
    
    bool needsUpdate = display.checkForceUpdate();

    // IP/도움말 모드가 아니고, (1초가 지났거나 강제 트리거가 발생했을 때) 갱신
    if (uiStage == 0 && (needsUpdate || (now - lastUpdate >= UPDATE_INTERVAL_MS))) {
        handleClockUpdate(needsUpdate);
        lastUpdate = now;
    }
}
