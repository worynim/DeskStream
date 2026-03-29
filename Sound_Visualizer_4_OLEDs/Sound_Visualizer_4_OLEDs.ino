#include <Arduino.h>
#include <Ticker.h>
#include "config.h"
#include "DisplayEngine.h"
#include "AudioAnalyzer.h"

// --- [1] 설정 및 객체 생성 ---
// (상수 및 핀 설정은 각 모듈의 헤더 파일에서 관리함)
int DEBUG_MODE = 1; // 0: Off, 1: FPS, 2: FPS+Perf, 3: Full (includes DMA Diag)

AudioAnalyzer analyzer(DEFAULT_SAMPLES, DEFAULT_SAMPLING_FREQ, DEFAULT_NUM_BANDS);
Ticker fps_ticker;

unsigned long btn_press_start_time = 0;
bool btn_is_held = false;

// 캘리브레이션 실행 함수
void runCalibration() {
  LGFX_Sprite& canvas = display.getCanvas();
  canvas.clear();
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.drawCenterString("CALIBRATING NOISE...", CANVAS_WIDTH / 2, TEXT_Y_OFFSET);
  display.pushCanvas();

  const int cal_iterations = CAL_ITERATIONS; // 측정 횟수 증가 (더 정확한 샘플링)
  float max_vals[DEFAULT_NUM_BANDS];
  for (int i = 0; i < DEFAULT_NUM_BANDS; i++) max_vals[i] = 0;

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

void setup() {
  Serial.begin(115200);

  // 모듈 초기화
  display.begin();
  analyzer.begin();

  // 버튼 설정
  pinMode(CAL_BUTTON_PIN, INPUT_PULLUP);

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
  // [Step 0] 캘리브레이션 버튼 체크 (9번 핀, 2초 이상 길게)
  if (digitalRead(CAL_BUTTON_PIN) == LOW) {
    if (!btn_is_held) {
      btn_press_start_time = millis();
      btn_is_held = true;
    } else if (millis() - btn_press_start_time > BTN_LONG_PRESS_MS) {
      // 2초 이상 눌렸을 때: 버튼 떼기를 기다림
      LGFX_Sprite& canvas = display.getCanvas();
      canvas.clear();
      canvas.setFont(&fonts::FreeSansBold12pt7b);
      canvas.drawCenterString("RELEASE BUTTON TO START", CANVAS_WIDTH / 2, TEXT_Y_OFFSET);
      display.pushCanvas();

      while(digitalRead(CAL_BUTTON_PIN) == LOW) delay(10); // 뗄 때까지 대기
      
      // 버튼 클릭 소음이 사라질 때까지 지연
      canvas.clear();
      canvas.drawCenterString("WAIT 1.0 SEC...", CANVAS_WIDTH / 2, TEXT_Y_OFFSET);
      display.pushCanvas();
      delay(CAL_START_DELAY_MS); 

      runCalibration();
      btn_is_held = false; // 연속 실행 방지
    }
  } else {
    btn_is_held = false;
  }

  if (analyzer.available()) {
    uint32_t loop_start = micros();

    // 1. FFT 분석
    analyzer.process();

    // 2. 렌더링 (가상 캔버스에 그리기)
    analyzer.render(display.getCanvas());

    // 3. 디스플레이 전송 (Smart Flush)
    display.pushCanvas();

    // [Step 4] FPS 상한 제어
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
