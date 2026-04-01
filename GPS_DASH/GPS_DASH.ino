/**
 * @file GPS_DASH.ino
 * @author Antigravity
 * @brief GPS 정보를 4개의 OLED 디스플레이에 표시하는 프로그램 (심플 버전)
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

// 버튼 핀 정의
const int BTN1_PIN = 1;  // OLED 1 제어용
const int BTN2_PIN = 4;  // OLED 2 제어용
const int BTN3_PIN = 10; // OLED 3 제어용
const int BTN4_PIN = 9;  // OLED 4 제어용
const int BUZZER_PIN = 7; // 부저 제어용

// 각 OLED(1~4번)에 어떤 모드(Time, Speed, 등)를 띄울지 저장하는 배열
int currentDisplayModes[4] = {MODE_TIME, MODE_SPEED, MODE_COMPASS, MODE_SATS};

// --- [주행 통계 데이터] ---
unsigned long tripTimeSec = 0;      // 총 주행 시간 (초)
float tripDistanceKm = 0.0;         // 총 주행 거리 (km)
unsigned long lastStatsUpdate = 0;  // 통계 업데이트용 타이머

// --- [비차단 부저 시스템 타입 선언] (Arduino 자동 전방선언 호환) ---
struct ToneNote {
    int freq;
    int durationMs;
    int pauseMs;  // 다음 음 전 쉬는 시간 (0이면 즉시 연주)
};

const int MAX_MELODY_NOTES = 5;
ToneNote _melodyQueue[MAX_MELODY_NOTES];
int _melodyLen = 0;
int _melodyIdx = 0;
unsigned long _toneEndAt = 0;
unsigned long _pauseEndAt = 0;
enum TonePhase { TONE_IDLE, TONE_PLAYING, TONE_PAUSE };
TonePhase _tonePhase = TONE_IDLE;

/**
 * @brief 패시브 부저음을 발생시킵니다 (펄스 생성).
 * @param duration 소리가 나는 시간 (ms)
 * @param freq 주파수 (Hz) - 기본값 2000Hz
 */
void beep(int duration = 50, int freq = 2000) {
  tone(BUZZER_PIN, freq);
  delay(duration);
  noTone(BUZZER_PIN);
}

/**
 * @brief 시스템 시작 멜로디를 재생합니다 (띠리리링~).
 */
void playBootMelody() {
    beep(80, 2093);
    delay(30);
    beep(80, 2637);
    delay(30);
    beep(80, 3136);
    delay(30);
    beep(200, 4186);
}

// --- [비차단 부저 함수 정의] (loop 내 사용 — GPS 프레임 유실 방지) ---

/**
 * @brief 비차단 멜로디 재생 시작 (여러 음을 큐에 등록)
 */
void playMelodyAsync(const ToneNote* notes, int count) {
    if (count > MAX_MELODY_NOTES) count = MAX_MELODY_NOTES;
    for (int i = 0; i < count; i++) _melodyQueue[i] = notes[i];
    _melodyLen = count;
    _melodyIdx = 0;
    tone(BUZZER_PIN, _melodyQueue[0].freq);
    _toneEndAt = millis() + _melodyQueue[0].durationMs;
    _tonePhase = TONE_PLAYING;
}

/**
 * @brief 비차단 단일 부저음 (loop 내 버튼 피드백용)
 */
void beepAsync(int duration = 50, int freq = 2000) {
    ToneNote n = {freq, duration, 0};
    playMelodyAsync(&n, 1);
}

/**
 * @brief 부저 상태 업데이트 — loop()에서 매번 호출 필수
 */
void updateTone() {
    if (_tonePhase == TONE_IDLE) return;
    unsigned long now = millis();
    
    if (_tonePhase == TONE_PLAYING && now >= _toneEndAt) {
        noTone(BUZZER_PIN);
        int pauseMs = _melodyQueue[_melodyIdx].pauseMs;
        _melodyIdx++;
        
        if (_melodyIdx >= _melodyLen) {
            _tonePhase = TONE_IDLE;
        } else if (pauseMs > 0) {
            _pauseEndAt = now + pauseMs;
            _tonePhase = TONE_PAUSE;
        } else {
            tone(BUZZER_PIN, _melodyQueue[_melodyIdx].freq);
            _toneEndAt = now + _melodyQueue[_melodyIdx].durationMs;
        }
    }
    
    if (_tonePhase == TONE_PAUSE && now >= _pauseEndAt) {
        if (_melodyIdx < _melodyLen) {
            tone(BUZZER_PIN, _melodyQueue[_melodyIdx].freq);
            _toneEndAt = now + _melodyQueue[_melodyIdx].durationMs;
            _tonePhase = TONE_PLAYING;
        } else {
            _tonePhase = TONE_IDLE;
        }
    }
}

/**
 * @brief GPS Lock 시 재생할 멜로디 — 비차단 (Fix Acquired)
 */
void playLockMelody() {
    static const ToneNote notes[] = {{2093, 100, 50}, {3136, 150, 0}};
    playMelodyAsync(notes, 2);
}

/**
 * @brief GPS Unlock 시 재생할 멜로디 — 비차단 (Fix Lost)
 */
void playUnlockMelody() {
    static const ToneNote notes[] = {{1047, 150, 50}, {1047, 100, 0}};
    playMelodyAsync(notes, 2);
}

/**
 * @brief 하드웨어 버튼의 Short/Long Press를 관리하는 구조체
 */
struct Button {
    int pin;
    bool lastState;
    unsigned long fallTime;
    bool isPressed;
    bool isLongPressFired; // 롱 프레스 즉시 실행을 위한 락(Lock) 플래그 막기
    
    void (*onShortPress)();
    void (*onLongPress)();
    
    Button(int p, void (*sp)(), void (*lp)()) : pin(p), lastState(HIGH), fallTime(0), isPressed(false), isLongPressFired(false), onShortPress(sp), onLongPress(lp) {}

    void init() {
        pinMode(pin, INPUT_PULLUP);
    }

    void update() {
        bool currentState = digitalRead(pin);
        unsigned long now = millis();
        
        if (lastState == HIGH && currentState == LOW) { // 눌림 (Falling Edge)
            fallTime = now;
            isPressed = true;
            isLongPressFired = false;
        } 
        else if (currentState == LOW) { // 계속 누르고 있는 상태 (Hold)
            if (isPressed && !isLongPressFired) {
                if (now - fallTime > 1000) { // 1s가 지난 순간 손을 떼지 않아도 즉시 롱 프레스 발동!
                    isLongPressFired = true; // 한 번 눌린 것으로 처리하여 중복/연사 방지
                    beepAsync(200, 2000); // 롱 프레스 부저음 (비차단)
                    if (onLongPress) onLongPress();
                }
            }
        } 
        else if (lastState == LOW && currentState == HIGH) { // 떼짐 (Rising Edge)
            if (isPressed && !isLongPressFired) { // 롱 프레스가 안터진 경우에만 숏 프레스 발동
                unsigned long duration = now - fallTime;
                if (duration > 50) { // Short Press (Debounce 50ms)
                    beepAsync(50, 3000); // 숏 프레스 부저음 (비차단)
                    if (onShortPress) onShortPress();
                }
            }
            isPressed = false;
        }
        lastState = currentState;
    }
};

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

// 4개의 버튼을 관리하는 배열
Button btns[4] = {
    Button(BTN1_PIN, btn1_short, btn1_long),
    Button(BTN2_PIN, btn2_short, btn2_long),
    Button(BTN3_PIN, btn3_short, btn3_long),
    Button(BTN4_PIN, btn4_short, btn4_long)
};

void setup() {
    Serial.begin(115200);
    
    // 버튼 핀 초기화
    for (int i = 0; i < 4; i++) {
        btns[i].init();
    }
    
    // 부저 초기화
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    
    // 시작 멜로디 재생
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

    // 2. 비차단 부저 및 NVS 지연 저장 업데이트
    updateTone();
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
    for (int i = 0; i < 4; i++) {
        btns[i].update();
    }

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

