#include <Arduino.h>
#include <Ticker.h>
#include <Preferences.h>
#include "config.h"
#include "DisplayEngine.h"
#include "AudioAnalyzer.h"

Preferences prefs;

// --- [비차단 부저 시스템 타입/상태 선언] (Arduino 자동 전방선언 호환) ---
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

// --- [1] 설정 및 객체 생성 ---
// (상수 및 핀 설정은 각 모듈의 헤더 파일에서 관리함)
int DEBUG_MODE = 1; // 0: Off, 1: FPS, 2: FPS+Perf, 3: Full (includes DMA Diag)

AudioAnalyzer analyzer(DEFAULT_SAMPLES, DEFAULT_SAMPLING_FREQ, DEFAULT_NUM_BANDS);
Ticker fps_ticker;

unsigned long btn_press_start_time = 0;
bool btn_is_held = false;
bool cal_long_press_fired = false; // 캘리브레이션 롱프레스 중복 방지
uint8_t vizMode = 0; // 0: 스펙트럼(막대), 1: 파형(오실로스코프)

bool flip_btn_last_state = HIGH;
unsigned long flip_btn_fall_time = 0;
bool is_flipped = true; // 현재 U8G2_R2 설정이므로 초기값은 true

// --- [도움말 모드] ---
bool isHelpMode = false;         // 버튼 설명 화면 표시 여부
bool help_btn_last_state = HIGH; // 도움말 버튼 이전 상태
unsigned long help_btn_fall_time = 0;
bool help_long_press_fired = false; // 롱프레스 중복 발동 방지

void applyFlipState(bool state) {
  uint8_t seg = state ? 0xA1 : 0xA0;
  uint8_t com = state ? 0xC8 : 0xC0;
  
  for (int i = 0; i < NUM_SCREENS; i++) {
    DisplayEngine::screens[i]->sendF("c", seg);
    DisplayEngine::screens[i]->sendF("c", com);
  }
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
 * @param duration 소리가 나는 시간 (ms)
 * @param freq 주파수 (Hz) - 기본값 2000Hz
 */
void beepAsync(int duration = 50, int freq = 2000) {
    ToneNote n = {freq, duration, 0};
    playMelodyAsync(&n, 1);
}

/**
 * @brief 부저 상태 업데이트 — loop()에서 매 사이클 호출 필수
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
 * @brief 블로킹 부저음 (캘리브레이션 등 의도적 대기 구간 전용).
 * @param duration 소리가 나는 시간 (ms)
 * @param freq 주파수 (Hz) - 기본값 2000Hz
 */
void beep(int duration = 50, int freq = 2000) {
  tone(BUZZER_PIN, freq);
  delay(duration);
  noTone(BUZZER_PIN);
}

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

void setup() {
  Serial.begin(115200);

  // 모듈 초기화
  display.begin();
  analyzer.begin();

  // 설정 로드 및 플립 하드웨어 초기화
  prefs.begin("viz_set", true);
  is_flipped = prefs.getBool("flip", true);
  prefs.end();
  applyFlipState(is_flipped);

  // 버튼 설정
  pinMode(CAL_BUTTON_PIN, INPUT_PULLUP);
  pinMode(FLIP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(HELP_BUTTON_PIN, INPUT_PULLUP);
  
  // 부저 초기화
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

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
  // [Step 0] 비차단 부저 상태 업데이트 (매 사이클 필수)
  updateTone();

  // [Step 1] 캘리브레이션/모드 버튼 체크 (9번 핀, Short/Long 분리)
  if (digitalRead(CAL_BUTTON_PIN) == LOW) {
    if (!btn_is_held) {
      btn_press_start_time = millis();
      btn_is_held = true;
      cal_long_press_fired = false;
    } else if (!cal_long_press_fired && millis() - btn_press_start_time > BTN_LONG_PRESS_MS) {
      // 롱 프레스: 캘리브레이션 진입
      cal_long_press_fired = true;
      beep(200, 2000); // 캘리브레이션 진입이므로 블로킹 OK
      LGFX_Sprite& canvas = display.getCanvas();
      canvas.clear();
      canvas.setFont(&fonts::FreeSansBold12pt7b);
      canvas.drawCenterString("RELEASE BUTTON TO START", CANVAS_WIDTH / 2, TEXT_Y_OFFSET);
      display.pushCanvas();

      while(digitalRead(CAL_BUTTON_PIN) == LOW) vTaskDelay(pdMS_TO_TICKS(10));
      
      canvas.clear();
      canvas.drawCenterString("WAIT 1.0 SEC...", CANVAS_WIDTH / 2, TEXT_Y_OFFSET);
      display.pushCanvas();
      delay(CAL_START_DELAY_MS); 

      runCalibration();
      btn_is_held = false;
    }
  } else {
    // 버튼을 떼 때: 롱프레스가 안 터졌으면 숏프레스
    if (btn_is_held && !cal_long_press_fired) {
      if (millis() - btn_press_start_time > 50) { // 디바운스 50ms
        vizMode = (vizMode + 1) % 2;
        beepAsync(50, 3000);
        Serial.printf("[VIZ] Mode: %s\n", vizMode == 0 ? "Spectrum" : "Waveform");
      }
    }
    btn_is_held = false;
  }

  // [Step 0.5] 플립 버튼 체크 (1번 핀, 엣지 감지 방식)
  bool flip_state = digitalRead(FLIP_BUTTON_PIN);
  if (flip_btn_last_state == HIGH && flip_state == LOW) { // 눌림
    flip_btn_fall_time = millis();
  } else if (flip_btn_last_state == LOW && flip_state == HIGH) { // 떼짐
    if (millis() - flip_btn_fall_time > 50) { // 디바운싱 50ms
      toggleFlip();
      beepAsync(50, 3000); // 비차단 피드백음
    }
  }
  flip_btn_last_state = flip_state;

  // [Step 3] 도움말 버튼 체크 (4번 핀, Short/Long 분리 — GPS_DASH Button 패턴)
  bool help_state = digitalRead(HELP_BUTTON_PIN);
  if (help_btn_last_state == HIGH && help_state == LOW) { // 눌림 (Falling Edge)
    help_btn_fall_time = millis();
    help_long_press_fired = false;
  } else if (help_state == LOW) { // 누르고 있는 상태 (Hold)
    if (!help_long_press_fired && millis() - help_btn_fall_time > BTN_LONG_PRESS_MS) {
      // 롱 프레스: 도움말 토글
      help_long_press_fired = true;
      isHelpMode = !isHelpMode;
      beepAsync(200, 2000);
      if (isHelpMode) {
        drawHelpScreen(); // 진입 시 1회 렌더링
      }
    }
  } else if (help_btn_last_state == LOW && help_state == HIGH) { // 떼짐 (Rising Edge)
    if (!help_long_press_fired && millis() - help_btn_fall_time > 50) {
      // 숏 프레스: (현재 미할당 — 비프음만 발생)
      beepAsync(50, 3000);
    }
  }
  help_btn_last_state = help_state;

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
