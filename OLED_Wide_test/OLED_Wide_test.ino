// worynim@gmail.com
/**
 * @file OLED_Wide_test.ino
 * @brief 와이드 OLED 디스플레이 드라이버 및 렌더링 성능 테스트
 * @details U8g2 및 LovyanGFX를 이용한 디스플레이 초기화, 통신 속도 측정 및 그래픽 출력 테스트 구현
 */
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Ticker.h>
#include <Preferences.h>
#include <DeskStreamCore.h>   // 4-OLED 화면 엔진·버튼·부저 (libraries/DeskStreamCore)

Preferences prefs;

// --- [1] 하드웨어 설정 ---
// 핀·I2C(HW 1.1MHz·고속 SW I2C)는 DeskStreamCore. BTN1(GPIO 1) = 화면 플립.
bool is_flipped = true;   // true = 패널 기본 방향(0xA1/0xC8) + 화면 순서 4→1

static ds::DisplayCore display;   // 4화면 전송 엔진 — 캔버스 전치·바뀐 타일만 병렬 전송
static ds::Buttons buttons;
static ds::Buzzer buzzer;

// --- [1-1] 프레임 제한 설정 ---
#define TARGET_FPS 30  // 목표 FPS
const uint32_t FRAME_TIME_US = 1000000 / TARGET_FPS;
static uint32_t last_frame_micros = 0;

// --- [2] LovyanGFX 가상 캔버스 설정 (512x64 와이드 화면) ---
LGFX_Sprite canvas;

// --- [2-1] FPS 측정용 인터럽트 변수 ---
volatile uint32_t frame_counter = 0;
Ticker fps_ticker;

void onFpsReport() {
    Serial.printf("REAL-TIME FPS: %u\n", frame_counter);
    frame_counter = 0;
}

// --- [2-2] 부저 및 플립 제어 함수 ---
// state=true → 0xA1/0xC8 (U8g2 초기값 = setFlip(false)), false → 0xA0/0xC0 (setFlip(true))
void applyFlipState(bool state) {
    display.setFlip(!state);
}

void toggleFlip() {
    is_flipped = !is_flipped;
    applyFlipState(is_flipped);
    
    prefs.begin("wide_set", false);
    prefs.putBool("flip", is_flipped);
    prefs.end();
    
    buzzer.beep(50, 3000);   // 비차단
    Serial.printf("[SYSTEM] Display Flipped: %s\n", is_flipped ? "180 (ON)" : "Normal (OFF)");
}

// 버튼 폴링 — BTN1 짧게(> 50ms)는 toggleFlip() 콜백으로 처리된다
void checkButtons() {
    buttons.update();
}

// --- [4] 와이드 캔버스 전송 엔진 (Smart Flush) ---
void pushSmartWideCanvas() {
  checkButtons(); // 즉각적인 반응을 위해 프레임 시작 시 체크
  
  // 프레임 레이트 제한 (너무 빠르면 대기)
  uint32_t now_us = micros();
  uint32_t elapsed = now_us - last_frame_micros;
  if (elapsed < FRAME_TIME_US) {
    uint32_t wait_us = FRAME_TIME_US - elapsed;
    if (wait_us > 1000) delay(wait_us / 1000); // 1ms 이상이면 delay() 사용
    while (micros() - last_frame_micros < FRAME_TIME_US) {
        checkButtons(); // 미세 대기 시간 중에도 버튼 스캔 유지
    }
  }
  last_frame_micros = micros();

  frame_counter++;
  // 512x64 캔버스를 4화면으로 전치하고 바뀐 타일만 병렬 전송 (is_flipped면 화면 순서 4→1)
  display.pushCanvas((const uint8_t*)canvas.getBuffer(), is_flipped);
}

// 기존 delay()를 대체하는 스마트 딜레이 (지연 중에도 FPS 유지)
void smartDelay(uint32_t ms) {
    uint32_t start_ms = millis();
    while (millis() - start_ms < ms) {
        checkButtons();
        pushSmartWideCanvas(); // 지연 시간 동안에도 일정한 주기로 데이터 체크/전송
    }
}

// --- [5] Adafruit 스타일 테스트 함수들 (512x64 최적화) ---

void testDrawLine() {
  canvas.clear();
  for (int16_t i=0; i<512; i+=4) {
    canvas.drawLine(0, 0, i, 63, 1);
    pushSmartWideCanvas();
  }
  for (int16_t i=0; i<64; i+=4) {
    canvas.drawLine(0, 0, 511, i, 1);
    pushSmartWideCanvas();
  }
  smartDelay(250);

  canvas.clear();
  for (int16_t i=0; i<512; i+=4) {
    canvas.drawLine(511, 0, i, 63, 1);
    pushSmartWideCanvas();
  }
  for (int16_t i=0; i<64; i+=4) {
    canvas.drawLine(511, 0, 0, i, 1);
    pushSmartWideCanvas();
  }
  smartDelay(250);
}

void testDrawRect() {
  canvas.clear();
  for (int16_t i=0; i<64; i+=2) {
    canvas.drawRect(i*4, i/2, 512-i*8, 64-i, 1);
    pushSmartWideCanvas();
  }
  smartDelay(500);
}

void testFillRect() {
  canvas.clear();
  for (int16_t i=0; i<64; i+=2) {
    canvas.fillRect(i*4, i/2, 512-i*8, 64-i, 1);
    pushSmartWideCanvas();
  }
  smartDelay(500);
}

void testDrawCircle() {
  canvas.clear();
  for (int16_t i=0; i<64; i+=2) {
    canvas.drawCircle(256, 32, i, 1);
    pushSmartWideCanvas();
  }
  smartDelay(500);
}

void testFillCircle() {
  canvas.clear();
  for (int16_t i=64; i>0; i-=4) {
    canvas.fillCircle(256, 32, i, 1);
    pushSmartWideCanvas();
  }
  smartDelay(500);
}

void testDrawRoundRect() {
  canvas.clear();
  for (int16_t i=0; i<32; i+=2) {
    canvas.drawRoundRect(i*8, i, 512-i*16, 64-i*2, i/2, 1);
    pushSmartWideCanvas();
  }
  smartDelay(500);
}

void testDrawTriangle() {
  canvas.clear();
  for (int16_t i=0; i<32; i+=2) {
    canvas.drawTriangle(256, 32-i, 256-i*4, 32+i, 256+i*4, 32+i, 1);
    pushSmartWideCanvas();
  }
  smartDelay(500);
}

void testDrawText() {
  canvas.clear();
  canvas.setFont(&fonts::Font0); // 기본 폰트
  canvas.setTextSize(1);
  canvas.setCursor(0, 0);
  canvas.println("Hello, Wide World!");
  canvas.setTextSize(2);
  canvas.println("512x64 OLED Matrix");
  canvas.setTextSize(1);
  canvas.println("Adafruit SSD1306 Style Demo");
  canvas.println("Using LovyanGFX + U8g2 Optimization");
  pushSmartWideCanvas();
  smartDelay(2000);
}

void testScrollText() {
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  const char* msg = "4 OLEDs DEMO ON 512x64 WIDE SCREEN";
  int16_t text_width = canvas.textWidth(msg);
  
  for (int16_t x = 512; x > -text_width; x -= 4) {
    canvas.clear();
    canvas.drawString(msg, x, 20);
    pushSmartWideCanvas();
  }
}

// --- [6] 메인 루틴 ---

void setup() {
  Serial.begin(115200);

  // ESP32-C3 USB CDC(HWCDC): 호스트가 포트를 읽지 않아 TX 링(256B)이 차면
  // HWCDC::write()가 호출당 최대 2초(100ms × 20회)를 블로킹해 loop()가 멈춘다.
  // 버튼·부저 등 실시간 처리가 지연되지 않도록 TX 타임아웃을 0(비차단)으로.
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  
  // 화면 엔진 초기화 (I2C·HW 전송 태스크·U8G2 4개)
  display.begin();

  // 버튼 및 부저 초기화
  buttons.begin();
  buttons.setCallbacks(0, toggleFlip, nullptr);
  buzzer.begin();

  // 저장된 설정 로드
  prefs.begin("wide_set", true);
  is_flipped = prefs.getBool("flip", true);
  prefs.end();
  applyFlipState(is_flipped);

  // FPS 타이머 인터럽트 시작 (1초마다 onFpsReport 실행)
  fps_ticker.attach(1.0, onFpsReport);

  // 캔버스 초기화 (512x64, 1비트 컬러)
  canvas.setColorDepth(1);
  canvas.createSprite(512, 64);
  canvas.setBitmapColor(1, 0);

  // 시작 메시지
  canvas.clear();
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.drawCenterString("WIDE OLED DEMO", 256, 20);
  pushSmartWideCanvas();
  smartDelay(2000);
}

void loop() {
  checkButtons();
  // Adafruit SSD1306 라이브러리 예제 스타일의 순환 데모
  testDrawLine();
  testDrawRect();
  testFillRect();
  testDrawCircle();
  testFillCircle();
  testDrawRoundRect();
  testDrawTriangle();
  testDrawText();
  testScrollText();
  
  // 모든 테스트 후 잠깐 대기
  canvas.clear();
  canvas.drawCenterString("RESTARTING DEMO...", 256, 20);
  pushSmartWideCanvas();
  smartDelay(1000);
}