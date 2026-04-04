#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Preferences.h>      // 비휘발성 설정 저장용
#include "driver/gpio.h"      // ESP32 하드웨어 레지스터 직접 제어를 위한 헤더
#include "soc/gpio_struct.h"  // GPIO 구조체 정의 (고속 GPIO 조작용)

/**
 * 🛠 Stats_Monitor: ESP32-C3 기반 4개 OLED 시스템 상태 모니터
 * - 2개의 하드웨어 I2C 및 2개의 소프트웨어 I2C 사용
 * - BLE를 통한 실시간 PC 상태 데이터 수신
 * - 고속 렌더링 최적화 (Dirty Checking 및 Fast GPIO) 적용
 * - 버튼(BTN1)을 통한 180도 화면 회전 및 부저 피드백 추가
 */

// --- [0] 하드웨어 버튼 및 부저 핀 설정 ---
const int BTN1_PIN = 1;   // OLED 1 / Flip 제어용
const int BTN2_PIN = 4;   // 기능 확장용
const int BTN3_PIN = 10;  // 기능 확장용
const int BTN4_PIN = 9;   // 기능 확장용
const int BUZZER_PIN = 7; // 부저 피드백용

// --- [1] HW/SW I2C 핀 설정 ---
const uint8_t hw_sda_pin = 5;
const uint8_t hw_scl_pin = 6;
const uint8_t sw_sda_pin = 2;
const uint8_t sw_scl_pin = 3;

// --- [2] OLED 디스플레이 객체 초기화 ---
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_1(U8G2_R0, U8X8_PIN_NONE);                          // HW I2C (0x3C)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_2(U8G2_R0, U8X8_PIN_NONE);                          // HW I2C (0x3D)
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_3(U8G2_R0, sw_scl_pin, sw_sda_pin, U8X8_PIN_NONE);  // SW I2C (0x3C)
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_4(U8G2_R0, sw_scl_pin, sw_sda_pin, U8X8_PIN_NONE);  // SW I2C (0x3D)

U8G2 *screens[4] = { &u8g2_1, &u8g2_2, &u8g2_3, &u8g2_4 };
uint8_t shadow_buffer[4][1024];  // 변경 감지용 버퍼 (전체 화면 전송 최소화)

// --- [3] 시스템 설정 및 상태 변수 ---
Preferences prefs;
int currentFlipMode = 0;         // 0: Normal, 2: 180도 회전
const int BRIGHTNESS_LEVELS[] = {1, 50, 100, 240}; // 밝기 단계 설정 (사용자 요청: 1, 50, 100, 240)
const int NUM_BRIGHTNESS_LEVELS = sizeof(BRIGHTNESS_LEVELS) / sizeof(BRIGHTNESS_LEVELS[0]);
int currentBrightness = BRIGHTNESS_LEVELS[1];      // 기본 밝기 (50)
bool isHelpMode = false;         // 버튼 도움말 표시 여부
bool _settingsDirty = false;
unsigned long _lastSettingsChange = 0;

// --- [4] 비차단 부저 시스템 (Tone System) ---
struct ToneNote {
  int freq;
  int durationMs;
  int pauseMs;
};

const int MAX_MELODY_NOTES = 5;
ToneNote _melodyQueue[MAX_MELODY_NOTES];
int _melodyLen = 0;
int _melodyIdx = 0;
unsigned long _toneEndAt = 0;
unsigned long _pauseEndAt = 0;
enum TonePhase { TONE_IDLE, TONE_PLAYING, TONE_PAUSE };
TonePhase _tonePhase = TONE_IDLE;

void playMelodyAsync(const ToneNote* notes, int count) {
  if (count > MAX_MELODY_NOTES) count = MAX_MELODY_NOTES;
  for (int i = 0; i < count; i++) _melodyQueue[i] = notes[i];
  _melodyLen = count;
  _melodyIdx = 0;
  tone(BUZZER_PIN, _melodyQueue[0].freq);
  _toneEndAt = millis() + _melodyQueue[0].durationMs;
  _tonePhase = TONE_PLAYING;
}

void beepAsync(int duration = 50, int freq = 3000) {
  ToneNote n = {freq, duration, 0};
  playMelodyAsync(&n, 1);
}

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

void playBootMelody() {
  ToneNote notes[] = {{2093, 80, 30}, {2637, 80, 30}, {3136, 80, 30}, {4186, 200, 0}};
  playMelodyAsync(notes, 4);
}

// --- [5] 화면 회전 (Flip) 제어 ---
void updateDisplayFlip() {
  uint8_t seg, com;
  if (currentFlipMode == 2) { // 180도 회전 (H+V Flip)
    seg = 0xA1; com = 0xC8;
  } else { // 원래 화면
    seg = 0xA0; com = 0xC0;
  }
  for (int i = 0; i < 4; i++) {
    screens[i]->sendF("c", seg);
    screens[i]->sendF("c", com);
  }
}

// --- [6] 밝기 제어 ---
void updateBrightness() {
  for (int i = 0; i < 4; i++) {
    screens[i]->setContrast(currentBrightness);
  }
}

// --- [6] 도움말 화면 (Help Screen) ---
void drawHelp(U8G2 &u, int logicalIdx, int physicalIdx) {
  u.clearBuffer();
  u.setFont(u8g2_font_7x14B_tf);
  char title[16];
  snprintf(title, sizeof(title), "BTN %d INFO", logicalIdx + 1);
  drawCentered(u, title, 14, u8g2_font_7x14B_tf);
  u.drawHLine(0, 17, 128);

  u.setFont(u8g2_font_profont15_tr);
  const char *info = "";
  char stateInfo[16] = "";  
  bool drawStatusSquares = false;
  int statusLevel = 0;

  switch (logicalIdx) {
    case 0: 
      info = "SHORT: FLIP 180"; 
      snprintf(stateInfo, sizeof(stateInfo), "[ %s ]", currentFlipMode == 2 ? "180 DEG" : "0 DEG");
      break;
    case 1: 
      info = "SHORT: BRIGHTNESS"; 
      drawStatusSquares = true;
      statusLevel = 0;
      for (int k = 0; k < NUM_BRIGHTNESS_LEVELS; k++) {
        if (currentBrightness >= BRIGHTNESS_LEVELS[k]) {
          statusLevel = k + 1;
        }
      }
      break;
    case 2: 
      info = "SHORT: UNUSED"; 
      break;
    case 3: 
      info = "SHORT: TOGGLE HELP"; 
      break;
  }
  
  drawCentered(u, info, 34, u8g2_font_profont15_tr);
  
  if (drawStatusSquares) {
    // 밝기 상태 비주얼 바 (사각형 표시)
    int boxSize = 10;
    int gap = 4;
    int totalWidth = (boxSize * 4) + (gap * 3);
    int startX = (128 - totalWidth) / 2;
    int startY = 40;
    for (int j = 0; j < 4; j++) {
      if (j < statusLevel) {
        u.drawBox(startX + j * (boxSize + gap), startY, boxSize, boxSize); // 활성화된 칸 (빛남)
      } else {
        u.drawFrame(startX + j * (boxSize + gap), startY, boxSize, boxSize); // 비활성화된 칸
      }
    }
  } else if (stateInfo[0] != '\0') {
    drawCentered(u, stateInfo, 49, u8g2_font_7x14B_tf);
  }
  
  u.setFont(u8g2_font_6x10_tf);
  drawCentered(u, "DeskStream Stats Mon", 62, u8g2_font_6x10_tf);
  smartSendBuffer(physicalIdx); 
}

// --- [6] 비휘발성 설정 저장 (NVS) ---
void saveSettings() {
  prefs.begin("stats_mon", false);
  prefs.putInt("flip", currentFlipMode);
  prefs.putInt("bright", currentBrightness);
  prefs.end();
}

void loadSettings() {
  prefs.begin("stats_mon", true);
  currentFlipMode = prefs.getInt("flip", 0);
  currentBrightness = prefs.getInt("bright", BRIGHTNESS_LEVELS[1]); // 기본값 50 (배열의 두 번째 값)
  prefs.end();
  
  updateDisplayFlip(); // 저장된 회전 상태 적용
  updateBrightness();  // 저장된 밝기 적용
}

void markSettingsDirty() {
  _settingsDirty = true;
  _lastSettingsChange = millis();
}

void checkDeferredSave() {
  if (_settingsDirty && millis() - _lastSettingsChange > 3000) {
    saveSettings();
    _settingsDirty = false;
    Serial.println("[NVS] Settings saved to flash.");
  }
}

// --- [7] 하드웨어 버튼 처리 (Button System) ---
struct Button {
  int pin;
  bool lastState;
  unsigned long fallTime;
  bool isPressed;
  bool isLongPressFired;
  void (*onShortPress)();
  void (*onLongPress)();

  Button(int p, void (*sp)(), void (*lp)()) : pin(p), lastState(HIGH), fallTime(0), isPressed(false), isLongPressFired(false), onShortPress(sp), onLongPress(lp) {}

  void init() { pinMode(pin, INPUT_PULLUP); }

  void update() {
    bool currentState = digitalRead(pin);
    unsigned long now = millis();
    if (lastState == HIGH && currentState == LOW) {
      fallTime = now;
      isPressed = true;
      isLongPressFired = false;
    } else if (currentState == LOW) {
      if (isPressed && !isLongPressFired && (now - fallTime > 1000)) {
        isLongPressFired = true;
        beepAsync(200, 2000);
        if (onLongPress) onLongPress();
      }
    } else if (lastState == LOW && currentState == HIGH) {
      if (isPressed && !isLongPressFired && (now - fallTime > 50)) {
        beepAsync(50, 3000);
        if (onShortPress) onShortPress();
      }
      isPressed = false;
    }
    lastState = currentState;
  }
};

void btn1_short() {
  currentFlipMode = (currentFlipMode == 0) ? 2 : 0;
  updateDisplayFlip();
  markSettingsDirty();
  Serial.printf("[FLIP] Screen rotated to %d degrees.\n", currentFlipMode == 2 ? 180 : 0);
}

void btn2_short() {
  int currentIdx = -1;
  for (int i = 0; i < NUM_BRIGHTNESS_LEVELS; i++) {
    if (currentBrightness <= BRIGHTNESS_LEVELS[i]) {
      currentIdx = i;
      break;
    }
  }
  
  int nextIdx = (currentIdx + 1) % NUM_BRIGHTNESS_LEVELS;
  currentBrightness = BRIGHTNESS_LEVELS[nextIdx];
  
  updateBrightness();
  markSettingsDirty();
  Serial.printf("[BRIGHT] Level changed: %d (Step %d/%d)\n", currentBrightness, nextIdx + 1, NUM_BRIGHTNESS_LEVELS);
}

void btn3_short() {
}

void btn4_short() {
  isHelpMode = !isHelpMode;
  Serial.printf("[HELP] Help Mode: %s\n", isHelpMode ? "ON" : "OFF");
}

Button btns[4] = {
  Button(BTN1_PIN, btn1_short, NULL),
  Button(BTN2_PIN, btn2_short, NULL),
  Button(BTN3_PIN, btn3_short, NULL),
  Button(BTN4_PIN, btn4_short, NULL)
};

// --- [3] 고속 최적화: ESP32-C3 전용 SW I2C 콜백 ---
// 표준 digitalWrite보다 훨씬 빠른 레지스터 직접 쓰기 방식 사용
extern "C" uint8_t u8x8_gpio_and_delay_esp32_c3_fast(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  uint8_t pin;
  switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
      pin = u8x8->pins[U8X8_PIN_I2C_CLOCK];
      if (pin != U8X8_PIN_NONE) {
        pinMode(pin, OUTPUT_OPEN_DRAIN);
        gpio_set_level((gpio_num_t)pin, 1);
      }
      pin = u8x8->pins[U8X8_PIN_I2C_DATA];
      if (pin != U8X8_PIN_NONE) {
        pinMode(pin, OUTPUT_OPEN_DRAIN);
        gpio_set_level((gpio_num_t)pin, 1);
      }
      break;
    case U8X8_MSG_DELAY_MILLI: delay(arg_int); break;
    case U8X8_MSG_GPIO_I2C_CLOCK:
      pin = u8x8->pins[U8X8_PIN_I2C_CLOCK];
      if (pin != U8X8_PIN_NONE) {
        if (arg_int) GPIO.out_w1ts.val = (1 << pin);
        else GPIO.out_w1tc.val = (1 << pin);
      }
      break;
    case U8X8_MSG_GPIO_I2C_DATA:
      pin = u8x8->pins[U8X8_PIN_I2C_DATA];
      if (pin != U8X8_PIN_NONE) {
        if (arg_int) GPIO.out_w1ts.val = (1 << pin);
        else GPIO.out_w1tc.val = (1 << pin);
      }
      break;
    default: u8x8_SetGPIOResult(u8x8, 1); break;
  }
  return 1;
}

// --- [4] 고속 최적화: 스마트 데이터 전송 (Dirty Checking) ---
// 이전 프레임과 비교하여 데이터가 변경된 페이지만 선택적으로 OLED에 전송
void smartSendBuffer(int s_idx) {
  U8G2 *u = screens[s_idx];
  uint8_t *u8g2_buf = u->getBufferPtr();

  for (int p = 0; p < 8; p++) {
    if (memcmp(&u8g2_buf[p * 128], &shadow_buffer[s_idx][p * 128], 128) != 0) {
      memcpy(&shadow_buffer[s_idx][p * 128], &u8g2_buf[p * 128], 128);
      u->updateDisplayArea(0, p, 16, 1);  // 변경된 1페이지(128px)만 업데이트
    }
  }
}

// --- [5] BLE 서비스 설정 ---
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

BLEServer *pServer = NULL;
volatile bool deviceConnected = false; 
volatile bool dataReceived = false; 

// PC로부터 수신할 시스템 상태 구조체
struct SysStats {
  float cpu_u = 0, cpu_t = 0;
  float gpu_u = 0, gpu_t = 0;
  float ram_u = 0, disk_u = 0;
  float net_i = 0, net_o = 0;  // Download, Upload (KB/s)
} stats;

// --- [6] UI 그리기 헬퍼 함수 ---

// 텍스트 중앙 정렬 출력
void drawCentered(U8G2 &u, const char *text, int y, const uint8_t *font) {
  u.setFont(font);
  u.drawStr((128 - u.getStrWidth(text)) / 2, y, text);
}

// 화면 상단 헤더 및 구분선 출력
void drawHeader(U8G2 &u, const char *label) {
  u.setFont(u8g2_font_7x14B_tf);
  u.drawStr(0, 12, label);
  u.drawHLine(0, 15, 128);
}

// 슬림한 프로그레스 바 출력
void drawSlimBar(U8G2 &u, int y, float p) {
  u.drawFrame(0, y, 128, 4);
  int w = (int)(126 * (p / 100.0));
  if (w > 0) u.drawBox(1, y + 1, w, 2);
}

// --- [7] 개별 화면 렌더링 함수 ---

// Screen 1: CPU 점유율 및 상태
void drawCPU(U8G2 &u, int physIdx) {
  u.clearBuffer();
  drawHeader(u, "PROCESSOR (CPU)");
  char b[16];
  snprintf(b, 16, "%d", (int)stats.cpu_u);
  drawCentered(u, b, 22, u8g2_font_7_Seg_33x19_mn);
  u.setFont(u8g2_font_profont15_tr);
  u.drawStr(100, 50, "%");
  drawSlimBar(u, 60, stats.cpu_u);
  smartSendBuffer(physIdx);
}

// Screen 2: GPU 점유율 (또는 보조 지표)
void drawGPU(U8G2 &u, int physIdx) {
  u.clearBuffer();
  drawHeader(u, "GRAPHICS (GPU)");
  char b[16];
  snprintf(b, 16, "%d", (int)stats.gpu_u);
  drawCentered(u, b, 22, u8g2_font_7_Seg_33x19_mn);
  u.setFont(u8g2_font_profont15_tr);
  u.drawStr(100, 50, "%");
  drawSlimBar(u, 60, stats.gpu_u);
  smartSendBuffer(physIdx);
}

// Screen 3: 메모리 및 디스크 사용량
void drawStorage(U8G2 &u, int physIdx) {
  u.clearBuffer();
  drawHeader(u, "MEMORY / DISK");
  u.setFont(u8g2_font_profont15_tr);
  u.drawStr(0, 35, "RAM");
  char b[16];
  snprintf(b, 16, "%d %%", (int)stats.ram_u);
  u.drawStr(100, 35, b);
  drawSlimBar(u, 38, stats.ram_u);
  u.drawStr(0, 57, "DISK");
  snprintf(b, 16, "%d %%", (int)stats.disk_u);
  u.drawStr(100, 57, b);
  drawSlimBar(u, 60, stats.disk_u);
  smartSendBuffer(physIdx);
}

// 네트워크 속도 단위 변환 (KB/s -> Mbps/kbps)
void formatNetSpeed(float speed_kb, char *out_val, char *out_unit) {
  float speed_kbps = speed_kb * 8.0; // Byte -> Bit 환산 (kbps)
  
  if (speed_kbps >= 1000.0) {
    snprintf(out_val, 16, "%.1f", speed_kbps / 1000.0);
    strcpy(out_unit, "Mbps");
  } else {
    snprintf(out_val, 16, "%.0f", speed_kbps);
    strcpy(out_unit, "kbps");
  }
}

// Screen 4: 네트워크 트래픽 (UP/DOWN 분리)
void drawNetwork(U8G2 &u, int physIdx) {
  u.clearBuffer();
  drawHeader(u, "NETWORK TRAFFIC");
  char val[16], unit[8], buf[32];

  // Down speed
  u.setFont(u8g2_font_profont15_tr);
  u.drawStr(0, 35, "DOWN");
  formatNetSpeed(stats.net_i, val, unit);
  snprintf(buf, 32, "%s %s", val, unit);
  u.drawStr(128 - u.getStrWidth(buf), 35, buf);
  float down_p = (stats.net_i / 12500.0) * 100.0;  // 100Mbps 기준
  drawSlimBar(u, 38, min(down_p, 100.0f));

  // Up speed
  u.drawStr(0, 57, "UP");
  formatNetSpeed(stats.net_o, val, unit);
  snprintf(buf, 32, "%s %s", val, unit);
  u.drawStr(128 - u.getStrWidth(buf), 57, buf);
  float up_p = (stats.net_o / 1250.0) * 100.0;  // 10Mbps 기준
  drawSlimBar(u, 60, min(up_p, 100.0f));
  smartSendBuffer(physIdx);
}

// --- [8] BLE 콜백 정의 ---

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *p) {
    deviceConnected = true;
  }
  void onDisconnect(BLEServer *p) {
    deviceConnected = false;
    pServer->getAdvertising()->start();  // 연결 끊기면 다시 광고 시작
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) {
    String val = pChar->getValue();
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, val.c_str()) == DeserializationError::Ok) {
      if (doc.containsKey("c_u")) stats.cpu_u = doc["c_u"];
      if (doc.containsKey("c_t")) stats.cpu_t = doc["c_t"];
      if (doc.containsKey("g_u")) stats.gpu_u = doc["g_u"];
      if (doc.containsKey("r_u")) stats.ram_u = doc["r_u"];
      if (doc.containsKey("d_u")) stats.disk_u = doc["d_u"];
      if (doc.containsKey("n_i")) stats.net_i = doc["n_i"];
      if (doc.containsKey("n_o")) stats.net_o = doc["n_o"];
      dataReceived = true;
    }
  }
};

// --- [9] 메인 설정 및 루프 ---

void setup() {
  Serial.begin(115200);

  // I2C 초기화 및 속도 최적화
  Wire.begin(hw_sda_pin, hw_scl_pin);
  Wire.setClock(800000);  // 하드웨어 I2C 800kHz

  // 소프트웨어 I2C 고속 드라이버 할당
  u8g2_3.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;
  u8g2_4.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;
  u8g2_1.setBusClock(800000);
  u8g2_2.setBusClock(800000);

  // 디스플레이 시작
  u8g2_1.setI2CAddress(0x3C * 2);
  u8g2_1.begin();
  u8g2_2.setI2CAddress(0x3D * 2);
  u8g2_2.begin();
  u8g2_3.setI2CAddress(0x3C * 2);
  u8g2_3.begin();
  u8g2_4.setI2CAddress(0x3D * 2);
  u8g2_4.begin();

  memset(shadow_buffer, 0, sizeof(shadow_buffer));  // 섀도 버퍼 초기화

  // 버튼 및 부저 초기화
  for (int i = 0; i < 4; i++) btns[i].init();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // 부팅 멜로디 재생
  playBootMelody();

  // 설정 로드 (화면 회전 등)
  loadSettings();

  // BLE 서버 및 서비스 초기화
  BLEDevice::init("DeskStream_Stats");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pSvc = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pRx = pSvc->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pRx->setCallbacks(new MyCallbacks());
  pSvc->start();
  pServer->getAdvertising()->start();
  Serial.println("[SYSTEM] Stats Monitor Ready.");
}

void loop() {
  // 1. 하드웨어 업데이트 (버튼, 부저, 설정 저장)
  for (int i = 0; i < 4; i++) btns[i].update();
  updateTone();
  checkDeferredSave();

  // 2. 화면 매핑 인덱스 계산 (180도 회전 시 4,3,2,1 순서로 반전)
  int m[4];
  if (currentFlipMode == 2) {
    m[0] = 3; m[1] = 2; m[2] = 1; m[3] = 0; // 180도 회전 시 순서 반전
  } else {
    m[0] = 0; m[1] = 1; m[2] = 2; m[3] = 3; // 기본 순서
  }

  // 3. BLE 데이터 수신 및 화면 렌더링
  static int lastState = -1;
  static int lastFlipMode = -1;
  static bool lastHelpMode = false;
  static int lastBrightness = -1;   // 밝기 상태도 추적(도움말 모드 업데이트용)
  static bool hasFirstData = false; // 첫 데이터를 받았는지 여부

  int currentState = deviceConnected ? 1 : 0;
  if (currentState == 0) hasFirstData = false; // 연결 끊기면 데이터 수신 상태 초기화

  // 상태 변경 추적 (밝기 변경 감지 추가)
  bool stateChanged = (currentState != lastState || currentFlipMode != lastFlipMode || isHelpMode != lastHelpMode || currentBrightness != lastBrightness);

  if (isHelpMode) {
    // 도움말 모드: 회전과 관계없이 물리적 화면 위치(1~4)에 맞춰 버튼 정보를 고정 표시
    if (stateChanged) {
      for (int i = 0; i < 4; i++) {
        drawHelp(*screens[i], i, i); 
      }
      lastState = currentState;
      lastFlipMode = currentFlipMode;
      lastHelpMode = isHelpMode;
      lastBrightness = currentBrightness;
    }
  } else if (dataReceived || (stateChanged && hasFirstData)) {
    // [버그 수정]: U8G2 객체가 메모리를 아끼고자 하나의 전역 1024바이트 버퍼를 돌려막기함.
    // 다 그린 뒤 한꺼번에 반영하려 하면 결국 마지막에 그려진 "네트워크 화면"만 덮어쓰기되어 4개 화면 모두 네트웍만 뜸!
    // 따라서 그리는 즉시! 바로 매핑된 물리 패널로 날려버려야(smartSendBuffer) 합니다.
    drawCPU(*screens[m[0]], m[0]);
    drawGPU(*screens[m[1]], m[1]);
    drawStorage(*screens[m[2]], m[2]);
    drawNetwork(*screens[m[3]], m[3]);
    
    if (dataReceived) {
      dataReceived = false;
      hasFirstData = true;
    }
    
    lastState = currentState;
    lastFlipMode = currentFlipMode;
    lastHelpMode = isHelpMode;
    lastBrightness = currentBrightness;
  } else {
    // 데이터 수신 전 대기 상태: 연결 상태나 회전이 바뀌었을 때만 안내 화면 그림
    if (stateChanged && !hasFirstData) {
      if (currentState == 1) {
        // BLE 연결됨, 클라이언트에서 데이터 데이터가 오기를 기다리는 중
        for (int i = 0; i < 4; i++) {
          U8G2 *u = screens[m[i]];
          u->clearBuffer();
          drawCentered(*u, "CONNECTED", 30, u8g2_font_7x14B_tf);
          drawCentered(*u, "Waiting for Data..", 48, u8g2_font_profont15_tr);
          smartSendBuffer(m[i]);
        }
      } else {
        // 연결 안됨 (부팅 직후 또는 연결 끊김) -> 클라이언트 실행 메시지 표시
        const char *titles[4] = { "DeskStream", "SYSTEM STATS", "PLEASE RUN", "BLE READY" };
        const char *subs[4] = { "Stats Monitor", "WAITING...", "PC CLIENT APP", "Searching..." };
        for (int i = 0; i < 4; i++) {
          U8G2 *u = screens[m[i]];
          u->clearBuffer();
          drawCentered(*u, titles[i], 30, u8g2_font_7x14B_tf);
          drawCentered(*u, subs[i], 48, u8g2_font_profont15_tr);
          smartSendBuffer(m[i]);
        }
      }
      lastState = currentState;
      lastFlipMode = currentFlipMode;
      lastHelpMode = isHelpMode;
      lastBrightness = currentBrightness;
    }
  }
  delay(10); // 루프 주기를 조금 짧게 조정 (버튼 반응성 향상)
}
