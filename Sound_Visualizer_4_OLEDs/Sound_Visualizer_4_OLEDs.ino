// worynim@gmail.com
/**
 * @file Sound_Visualizer_4_OLEDs.ino
 * @brief 4개의 OLED를 이용한 고성능 오디오 시각화 메인 펌웨어
 * @details I2S 마이크 입력을 분석하여 실시간 음악 스펙트럼 및 비주얼 이펙트를 4개의 OLED에 렌더링
 */
#include <Arduino.h>
#include <Ticker.h>
#include <Preferences.h>
#include <LovyanGFX.hpp>
#include <DeskStreamCore.h>
#include "config.h"
#include "AudioAnalyzer.h"

Preferences prefs;

// --- [1] 설정 및 객체 생성 ---
// (상수 및 핀 설정은 각 모듈의 헤더 파일에서 관리함)
int DEBUG_MODE = 1; // 0: Off, 1: FPS, 2: FPS+Perf, 3: Full (includes DMA Diag)

AudioAnalyzer analyzer(DEFAULT_SAMPLES, DEFAULT_SAMPLING_FREQ, DEFAULT_NUM_BANDS);
Ticker fps_ticker;

uint8_t vizMode = 0; // 0: 스펙트럼(막대), 1: 파형(오실로스코프)
bool is_flipped = true; // true = 패널 기본 방향(0xA1/0xC8) + 화면 순서 4→1 (기존 기본값)

// --- [도움말 모드] ---
bool isHelpMode = false;         // 버튼 설명 화면 표시 여부

// --- [공통 서비스] DeskStreamCore ---
static ds::Buttons buttons;
static ds::Buzzer buzzer;

/**
 * @brief 512x64 캔버스 화면 — LovyanGFX 1bit 스프라이트에 그리고 ds::DisplayCore로 4화면에 보낸다
 * @details 예전 자체 디스플레이 엔진(I2C·셰도·전치)을 대체한다. 그리기 코드가 쓰는 메서드 이름은 그대로 둔다.
 *          전치·바뀐 타일만 병렬 전송은 ds::DisplayCore::pushCanvas()가 한다.
 */
class CanvasDisplay {
public:
    void begin() {
        _core.begin();
        _canvas.setColorDepth(1);
        _canvas.createSprite(CANVAS_WIDTH, CANVAS_HEIGHT);
        _canvas.setBitmapColor(COLOR_WHITE, COLOR_BLACK);
    }
    LGFX_Sprite& getCanvas() { return _canvas; }
    /** 캔버스를 4화면에 보낸다 — is_flipped면 화면 순서를 뒤집는다 (180° 장착) */
    void pushCanvas() {
        const uint32_t t0 = micros();
        _frames++;
        _core.pushCanvas((const uint8_t*)_canvas.getBuffer(), is_flipped);
        _lastUs = micros() - t0;
    }
    /** ms 동안 전송을 계속하며 기다린다 */
    void smartDelay(uint32_t ms) {
        const uint32_t start = millis();
        while (millis() - start < ms) { pushCanvas(); delay(1); }
    }
    ds::DisplayCore& core() { return _core; }
    uint32_t getLastTransmitTime() const { return _lastUs; }
    uint32_t getFrameCounter() const { return _frames; }
    void resetFrameCounter() { _frames = 0; }
private:
    ds::DisplayCore _core;
    LGFX_Sprite _canvas;
    uint32_t _lastUs = 0;
    volatile uint32_t _frames = 0;
};
CanvasDisplay display;

// 패널 방향: state=true → 0xA1/0xC8 (U8g2 초기값 = setFlip(false)), false → 0xA0/0xC0 (setFlip(true))
void applyFlipState(bool state) {
  display.core().setFlip(!state);
}

void toggleFlip() {
  is_flipped = !is_flipped;
  applyFlipState(is_flipped);
  
  prefs.begin("viz_set", false);
  prefs.putBool("flip", is_flipped);
  prefs.end();
  
  Serial.printf("[SYSTEM] Display Flipped: %s\n", is_flipped ? "180 (ON)" : "Normal (OFF)");
}

// 캘리브레이션 실행 함수
void runCalibration() {
  LGFX_Sprite& canvas = display.getCanvas();
  canvas.clear();
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.drawCenterString("CALIBRATING NOISE...", CANVAS_WIDTH / 2, TEXT_Y_OFFSET);
  display.pushCanvas();
  delay(300);

  const int cal_iterations = CAL_ITERATIONS; // 측정 횟수 증가 (더 정확한 샘플링)
  float max_vals[DEFAULT_NUM_BANDS];
  for (int i = 0; i < DEFAULT_NUM_BANDS; i++) max_vals[i] = 0;

  // 🚨 핵심 조치: 딜레이 시간 동안 가득 차서 오버플로우(끊어짐 파형 생성)된
  // ADC DMA 링 버퍼 데이터를 강제로 퍼내어 쓰레기통에 버립니다 (Flushing).
  while (analyzer.available()) { 
      // 텅 빌 때까지 허공에 읽기 수행
  }

  int count = 0;
  while (count < cal_iterations) {
    if (analyzer.available()) {
      analyzer.process();
      for (int i = 0; i < DEFAULT_NUM_BANDS; i++) {
        // 현재 매핑 모드가 적용된 실제 밴드 진폭을 측정
        float amp = analyzer.getBandAmplitude(i);
        if (amp > max_vals[i]) max_vals[i] = amp;
      }
      count++;
    }
    delay(1);
  }

  Serial.println("--- Calibration Results (Noise Floor) ---");
  for (int i = 0; i < DEFAULT_NUM_BANDS; i++) {
    // 측정된 최댓값에 마진을 더해 노이즈 플로어로 확정
    float noiseVal = (max_vals[i] * NOISE_FLOOR_FACTOR) + NOISE_FLOOR_OFFSET;
    analyzer.setNoiseFloor(i, noiseVal); 
    Serial.printf("Band %3d: %.2f\n", i, noiseVal);
  }
  Serial.println("---------------------------------------");

  canvas.clear();
  canvas.drawCenterString("CALIBRATION DONE!", CANVAS_WIDTH / 2, TEXT_Y_OFFSET);
  display.pushCanvas();
  delay(CAL_DONE_DELAY_MS);
}

// FPS 보고 콜백
void onFpsReport() {
  if (DEBUG_MODE >= 1) {
    Serial.printf("FPS: %u\n", display.getFrameCounter());
  }
  display.resetFrameCounter();
}



// 부저는 ds::Buzzer가 FreeRTOS 타이머로 비차단 재생한다.

/**
 * @brief 버튼 도움말 화면을 512x64 가상 캔버스에 렌더링
 * 
 * 4개의 128x64 영역에 각 버튼의 기능을 설명합니다.
 * 정적 콘텐츠이므로 진입 시 1회만 그리고 탈출 시까지 유지합니다.
 */
void drawHelpScreen() {
  LGFX_Sprite& canvas = display.getCanvas();
  canvas.clear();
  canvas.setFont(&fonts::Font0); // 기본 6x8 고정폭 폰트

  // 각 OLED 영역(128px)에 버튼 설명 배치
  // OLED 1 영역 (x: 0~127) — BTN 1 (GPIO 1)
  canvas.drawCenterString("- BTN 1 -", 64, 2);
  canvas.drawLine(0, 12, 127, 12, COLOR_WHITE);
  canvas.drawString(" S: FLIP SCREEN", 4, 22);
  canvas.drawString(" (180 ROTATION)", 4, 38);

  // OLED 2 영역 (x: 128~255) — BTN 2 (GPIO 4)
  canvas.drawCenterString("- BTN 2 -", 128 + 64, 2);
  canvas.drawLine(128, 12, 255, 12, COLOR_WHITE);
  canvas.drawString(" L: SHOW/HIDE", 132, 22);
  canvas.drawString("    THIS INFO", 132, 38);

  // OLED 3 영역 (x: 256~383) — BTN 3 (GPIO 10, 미사용)
  canvas.drawCenterString("- BTN 3 -", 256 + 64, 2);
  canvas.drawLine(256, 12, 383, 12, COLOR_WHITE);
  canvas.drawString(" (NOT ASSIGNED)", 260, 30);

  // OLED 4 영역 (x: 384~511) — BTN 4 (GPIO 9)
  canvas.drawCenterString("- BTN 4 -", 384 + 64, 2);
  canvas.drawLine(384, 12, 511, 12, COLOR_WHITE);
  canvas.drawString(" S: VIZ MODE", 388, 22);
  canvas.drawString(" L: CALIBRATE", 388, 38);
  canvas.drawString("    NOISE", 388, 50);

  display.pushCanvas();
  Serial.printf("[HELP] Mode: %s\n", isHelpMode ? "ON" : "OFF");
}

// --- [버튼 콜백] ---
/** BTN1 짧게: 화면 180° 플립 */
static void onFlipShort() {
  toggleFlip();
  buzzer.beep(50, 3000);
}

/** BTN2 짧게: (미할당 — 비프음만) */
static void onHelpShort() {
  buzzer.beep(50, 3000);
}

/** BTN2 길게: 도움말 토글 */
static void onHelpLong() {
  isHelpMode = !isHelpMode;
  buzzer.beep(200, 2000);
  if (isHelpMode) drawHelpScreen();   // 진입 시 1회 렌더링
}

/** BTN4 짧게: 스펙트럼 ↔ 파형 */
static void onModeShort() {
  vizMode = (vizMode + 1) % 2;
  buzzer.beep(50, 3000);
  Serial.printf("[VIZ] Mode: %s\n", vizMode == 0 ? "Spectrum" : "Waveform");
}

/** BTN4 길게: 버튼을 뗀 뒤 1초 기다렸다가 노이즈 캘리브레이션 (버튼 소음 방지) */
static void onCalibrateLong() {
  buzzer.beep(200, 2000);
  LGFX_Sprite& canvas = display.getCanvas();
  canvas.clear();
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.drawCenterString("RELEASE BUTTON TO START", CANVAS_WIDTH / 2, TEXT_Y_OFFSET);
  display.pushCanvas();

  while (digitalRead(ds::kButtonPins[BTN_IDX_CAL]) == LOW) vTaskDelay(pdMS_TO_TICKS(10));

  canvas.clear();
  canvas.drawCenterString("WAIT 1.0 SEC...", CANVAS_WIDTH / 2, TEXT_Y_OFFSET);
  display.pushCanvas();
  delay(CAL_START_DELAY_MS);

  runCalibration();
}

void setup() {
  Serial.begin(115200);

  // ESP32-C3 USB CDC(HWCDC): 호스트가 포트를 읽지 않아 TX 링(256B)이 차면
  // HWCDC::write()가 호출당 최대 2초(100ms × 20회)를 블로킹해 loop()가 멈춘다.
  // 버튼·부저 등 실시간 처리가 지연되지 않도록 TX 타임아웃을 0(비차단)으로.
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif

  // 모듈 초기화
  display.begin();
  analyzer.begin();

  // 설정 로드 및 플립 하드웨어 초기화
  prefs.begin("viz_set", true);
  is_flipped = prefs.getBool("flip", true);
  prefs.end();
  applyFlipState(is_flipped);

  // 버튼 설정 (짧게 > 50ms, 길게 > BTN_LONG_PRESS_MS)
  ds::ButtonTiming timing;
  timing.longPressMs = BTN_LONG_PRESS_MS;
  buttons.begin(timing);
  buttons.setCallbacks(BTN_IDX_FLIP, onFlipShort, nullptr);
  buttons.setCallbacks(BTN_IDX_HELP, onHelpShort, onHelpLong);
  buttons.setCallbacks(BTN_IDX_CAL, onModeShort, onCalibrateLong);

  // 부저 초기화
  buzzer.begin();

  // FPS 타이머 시작
  fps_ticker.attach(1.0, onFpsReport);

  // 시작 메시지
  LGFX_Sprite& canvas = display.getCanvas();
  canvas.clear();
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.drawCenterString("     Sound  Visualizer", CANVAS_WIDTH / 2, TEXT_Y_OFFSET);
  display.pushCanvas();
  display.smartDelay(BOOT_MESSAGE_DELAY_MS);
}

void loop() {
  // [Step 1] 버튼 처리 (콜백: onFlipShort / onHelpShort·onHelpLong / onModeShort·onCalibrateLong)
  buttons.update();

  // [Step 4] 도움말 모드일 때는 비주얼라이저 일시정지 (정적 화면 유지)
  if (isHelpMode) {
    // DMA 버퍼가 넘치지 않도록 백그라운드에서 데이터 소비만 수행
    while (analyzer.available()) { /* 허공에 읽기 — 오버플로우 방지 */ }
    vTaskDelay(pdMS_TO_TICKS(50)); // CPU 양보 (50ms 주기)
    return;
  }

  // [Step 5] 사운드 비주얼라이저 렌더 루프
  if (analyzer.available()) {
    uint32_t loop_start = micros();

    // 1. FFT 분석
    analyzer.process();

    // 2. 렌더링 (모드에 따라 스펙트럼 또는 파형 선택)
    if (vizMode == 0) {
      analyzer.render(display.getCanvas());      // 스펙트럼 막대 모드
    } else {
      analyzer.renderWaveform(display.getCanvas()); // 오실로스코프 파형 모드
    }

    // 3. 디스플레이 전송 (Smart Flush)
    display.pushCanvas();

    // [Step 6] FPS 상한 제어
    // 화면 변화가 적을 때 너무 빨라지는 것을 막아 애니메이션 일관성 유지
    uint32_t frame_time_us = micros() - loop_start;
    const uint32_t target_us = TARGET_FRAME_US; 
    if (frame_time_us < target_us) {
      uint32_t delay_us = target_us - frame_time_us;
      if (delay_us >= 2000) {
        // 2ms 이상 대기해야 할 때는 백그라운드 태스크를 위해 CPU 코어 점유 양보
        vTaskDelay(pdMS_TO_TICKS(delay_us / 1000));
        delayMicroseconds(delay_us % 1000);   // 잔여 마이크로초 대기
      } else {
        // 2ms 미만일 경우 가벼운 Microseconds 단위 대기
        delayMicroseconds(delay_us);
      }
    }

    // 성능 지표 디버깅
    if (DEBUG_MODE == 2) {
      uint32_t fft_time = analyzer.getLastFftTime();
      uint32_t render_time = analyzer.getLastRenderTime();
      uint32_t transmit_time = display.getLastTransmitTime();
      Serial.printf("FFT: %lu us | R: %lu us | T: %lu us | Loop: %lu us\n", 
                    fft_time, render_time, transmit_time, frame_time_us);
    }
  }
}
