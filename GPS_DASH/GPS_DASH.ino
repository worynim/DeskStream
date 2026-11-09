// worynim@gmail.com
/**
 * @file GPS_DASH.ino
 * @brief GPS 정보를 4개의 OLED 디스플레이에 표시하는 메인 펌웨어
 * @details GPS 수신 데이터를 기반으로 속도, 위치, 시간을 각각의 OLED에 시각화하며, OTA 업데이트를 지원함
 */
#include <Arduino.h>
#include <Preferences.h>

// --- [디버그 설정] ---
bool DEBUG_GPS = 0;  // 1: GPS 원시 NMEA 데이터를 시리얼 모니터에 그대로 출력

#include "display_handler.h"
#include "gps_handler.h"
#include "ota_handler.h"

Preferences prefs;             // 비휘발성 설정 저장용 객체
bool isDemoMode = false;       // 현재 데모 모드 여부
bool isHelpMode = false;       // 현재 버튼 정보 도움말 표시 여부

// 버튼(BTN1~4 = OLED 1~4 제어)·부저 — 핀은 DeskStreamCore ds_board.h
static ds::Buttons buttons;
static ds::Buzzer buzzer;

// 각 OLED(1~4번)에 어떤 모드(Time, Speed, 등)를 띄울지 저장하는 배열
int currentDisplayModes[4] = {MODE_TIME, MODE_SPEED, MODE_COMPASS, MODE_SATS};

// --- [주행 통계 데이터] ---
unsigned long tripTimeSec = 0;      // 총 주행 시간 (초)
float tripDistanceKm = 0.0;         // 총 주행 거리 (km)
unsigned long lastStatsUpdate = 0;  // 통계 업데이트용 타이머

// --- [부저] ds::Buzzer가 타이머로 비차단 재생한다 (GPS 프레임 유실 방지) ---
static const ds::Note BOOT_MELODY[]   = {{2093, 80, 30}, {2637, 80, 30}, {3136, 80, 30}, {4186, 200, 0}};   // 띠리리링~
static const ds::Note LOCK_MELODY[]   = {{2093, 100, 50}, {3136, 150, 0}};   // Fix Acquired
static const ds::Note UNLOCK_MELODY[] = {{1047, 150, 50}, {1047, 100, 0}};   // Fix Lost

void playBootMelody()   { buzzer.playMelody(BOOT_MELODY, 4); }
void playLockMelody()   { buzzer.playMelody(LOCK_MELODY, 2); }
void playUnlockMelody() { buzzer.playMelody(UNLOCK_MELODY, 2); }

// --- [버튼 동작 정의] ---

// --- [설정 저장 및 로드] ---

/**
 * @brief 현재 화면 설정(모드, 반전)을 NVS에 저장
 */
void saveSettings() {
    prefs.begin("gps_dash", false);
    for (int i = 0; i < 4; i++) {
        char key[8];
        snprintf(key, sizeof(key), "m%d", i);
        prefs.putInt(key, currentDisplayModes[i]);
    }
    prefs.putInt("flip", currentFlipMode);
    prefs.end();
}

/**
 * @brief NVS에서 이전 설정을 읽어와 복구
 */
void loadSettings() {
    prefs.begin("gps_dash", true);
    for (int i = 0; i < 4; i++) {
        char key[8];
        snprintf(key, sizeof(key), "m%d", i);
        // 저장된 값이 없으면 현재 기본값 유지
        currentDisplayModes[i] = prefs.getInt(key, currentDisplayModes[i]);
    }
    currentFlipMode = prefs.getInt("flip", currentFlipMode);
    prefs.end();
    
    // 로드된 반전 모드를 하드웨어에 즉시 반영
    updateDisplayFlip();
}

// --- [비휘발성 설정 지연 저장 시스템] ---
// NVS Flash 쓰기 수명 보호: 마지막 변경 후 3초 뒤 한 번만 저장
bool _settingsDirty = false;
unsigned long _lastSettingsChange = 0;

void markSettingsDirty() {
    _settingsDirty = true;
    _lastSettingsChange = millis();
}

void checkDeferredSave() {
    if (_settingsDirty && millis() - _lastSettingsChange > 3000) {
        saveSettings();
        _settingsDirty = false;
        Serial.println("[NVS] Deferred save completed.");
    }
}

// 모드 전환 공통 함수
void nextMode(int displayIndex) {
    if (isHelpMode) {
        isHelpMode = false; // 도움말 모드 전환 중지 및 원복
        return;
    }
    currentDisplayModes[displayIndex] = (currentDisplayModes[displayIndex] + 1) % DISPLAY_MODE_COUNT; 
    markSettingsDirty(); // 모드 변경 - 지연 저장 (Flash 수명 보호)
}

// 1번 버튼 (OLED 1)
void btn1_short() { nextMode(0); }
void btn1_long()  { 
    currentFlipMode = (currentFlipMode + 1) % 4;
    updateDisplayFlip(); 
    markSettingsDirty(); // 반전 모드 변경 - 지연 저장
    Serial.printf("[FLIP] Switched to Mode: %d (Saved)\n", currentFlipMode);
}

// 2번 버튼 (OLED 2)
void btn2_short() { nextMode(1); }
void btn2_long()  { 
    isHelpMode = !isHelpMode;
    Serial.printf("[HELP] Mode: %s\n", isHelpMode ? "ON" : "OFF");
}

// 3번 버튼 (OLED 3)
void btn3_short() { nextMode(2); }
void btn3_long()  { 
    tripTimeSec = 0;
    tripDistanceKm = 0.0;
    Serial.println("[TRIP] Stats have been RESET.");
}

// 4번 버튼 (OLED 4)
void btn4_short() { nextMode(3); }
void btn4_long()  {
    isDemoMode = !isDemoMode;
    Serial.printf("[DEMO] Mode: %s\n", isDemoMode ? "ON" : "OFF");
}

// --- [버튼] ds::Buttons (짧게 > 50ms, 길게 > 1000ms) ---
// 짧게: 50ms·3kHz 비프 후 동작 / 길게: 200ms·2kHz 비프 후 동작
static void s0() { buzzer.beep(50, 3000); btn1_short(); }
static void s1() { buzzer.beep(50, 3000); btn2_short(); }
static void s2() { buzzer.beep(50, 3000); btn3_short(); }
static void s3() { buzzer.beep(50, 3000); btn4_short(); }
static void l0() { buzzer.beep(200, 2000); btn1_long(); }
static void l1() { buzzer.beep(200, 2000); btn2_long(); }
static void l2() { buzzer.beep(200, 2000); btn3_long(); }
static void l3() { buzzer.beep(200, 2000); btn4_long(); }

void setup() {
    Serial.begin(115200);

    // ESP32-C3 USB CDC(HWCDC): 호스트가 포트를 읽지 않아 TX 링(256B)이 차면
    // HWCDC::write()가 호출당 최대 2초(100ms × 20회)를 블로킹해 loop()가 멈춘다.
    // 버튼·부저 등 실시간 처리가 지연되지 않도록 TX 타임아웃을 0(비차단)으로.
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(0);
#endif
    
    // 버튼 초기화
    buttons.begin();
    buttons.setCallbacks(0, s0, l0);
    buttons.setCallbacks(1, s1, l1);
    buttons.setCallbacks(2, s2, l2);
    buttons.setCallbacks(3, s3, l3);

    // 부저 초기화 및 시작 멜로디 재생 (비차단)
    buzzer.begin();
    playBootMelody();
    
    // 모듈별 초기화
    initDisplay();  // OLED 및 I2C 초기화 (display_handler.h)
    initGPS();      // GPS 초기화 (gps_handler.h)
    initOTA();      // WiFi AP 및 OTA 서버 초기화 (ota_handler.h)

    // 비휘발성 메모리 설정 로드
    loadSettings();

    // GPS 로그 콜백 등록 (extern 의존성 제거)
    setGPSLogCallback([](const char* msg) { addWebLog(msg); });
    
    // GPS Fix/Lost 콜백 등록
    setGPSFixCallbacks(playLockMelody, playUnlockMelody);

    Serial.println("[SYSTEM] GPS OLED DASHBOARD READY.");
    Serial.println("[SYSTEM] PRESS BUTTON (PIN 9) TO TOGGLE DEMO MODE.");
}

void loop() {
    // 1. 시스템 태스크 처리 (OTA 등)
    handleOTA();

    // 2. NVS 지연 저장 업데이트 (부저는 ds::Buzzer 타이머가 처리)
    checkDeferredSave();

    // 3. OTA 진행 중일 경우 디스플레이 전유
    if (is_ota_mode) {
        static unsigned long lastOtaDisp = 0;
        if (millis() - lastOtaDisp > 100) {
            lastOtaDisp = millis();
            displayOTAStatus(ota_progress);
        }
        return; 
    }

    // 4. 하드웨어 버튼 입력 처리 (Short/Long 프레스)
    buttons.update();

    // 5. [중요] GPS 데이터 수신 처리 (타이머 없이 루프마다 실행)
    // 데이터 소스가 5Hz이더라도 수신은 최대한 자주 호출해야 시리얼 버퍼가 넘치지 않습니다.
    // 항상 백그라운드에서 버퍼를 비워주어 오버플로를 방지합니다.
    updateGPS();

    // 6. 주행 통계 업데이트 (1초 주기)
    if (millis() - lastStatsUpdate >= 1000) {
        float currentSpeed = isDemoMode ? 60.0 : getGPSSpeed(); // km/h
        
        // 속도가 1.0km/h 이상일 때만 주행 중으로 간주
        if (currentSpeed > 1.0) {
            tripTimeSec++;
            // 거리 계산: 거리 = 속도 * 시간 (1초 = 1/3600 시간)
            tripDistanceKm += (currentSpeed / 3600.0);
        }
        lastStatsUpdate = millis();
    }

    // 7. 대시보드 화면 갱신 (10Hz)
    // GPS 데이터는 5Hz로 들어오지만, 10Hz로 화면을 그려야 보간(Interpolation) 애니메이션이 부드럽습니다.
    static unsigned long lastRefresh = 0;
    if (millis() - lastRefresh > 100) {
        lastRefresh = millis();
        
        DashboardData data;

        if (isDemoMode) {
            // --- [DEMO MODE] 시뮬레이션 ---
            static float fSpd = 0, fAng = 0;
            fSpd = 50.0 + sin(millis() / 3000.0) * 30.0;
            fAng += 2.1; if (fAng >= 360) fAng -= 360;
            
            data.timeStr = getGPSTime(true);
            data.speedVal = fSpd;
            data.courseVal = fAng;
            data.usedSats = 8;
            data.visibleSats = 12;
            data.hdopStr = "DOP:1.2";
            data.statusStr = "DEMO MODE";
            // 데모용 위도/경도/고도 (남산타워 뷰포인트 부근 시뮬레이션)
            data.latVal = 37.5522 + (sin(millis() / 4000.0) * 0.0005);
            data.lonVal = 126.9880 + (cos(millis() / 4000.0) * 0.0005);
            data.altVal = 260.0 + (sin(millis() / 2000.0) * 5.0);
            
            getGPSCalendar(true, data.calYear, data.calMonth, data.calDay);
        } else {
            // --- [GPS MODE] 실제 데이터 ---
            data.timeStr = getGPSTime(false);
            data.speedVal = getGPSSpeed();
            data.courseVal = getGPSCourse();
            data.usedSats = getGPSSatCount();
            data.visibleSats = getGPSSatVisible();
            data.hdopStr = getGPSHDOP();
            data.statusStr = getGPSSatStatus();
            data.latVal = getGPSLatitude();
            data.lonVal = getGPSLongitude();
            data.altVal = getGPSAltitude();
            
            getGPSCalendar(false, data.calYear, data.calMonth, data.calDay);
        }
        
        // 공통 주행 데이터 주입
        data.tripTimeSec = tripTimeSec;
        data.tripDistance = tripDistanceKm;
        data.showHelp = isHelpMode;

        updateDashboard(data, currentDisplayModes);
    }
}

