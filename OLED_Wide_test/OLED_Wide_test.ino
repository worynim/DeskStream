#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <LovyanGFX.hpp>
#include <Ticker.h>
#include "driver/gpio.h"

// --- [1] 하드웨어 설정 ---
const uint8_t hw_sda_pin = 5;
const uint8_t hw_scl_pin = 6;
const uint8_t sw_sda_pin = 2;
const uint8_t sw_scl_pin = 3;

// --- [1-1] 프레임 제한 설정 ---
#define TARGET_FPS 30  // 목표 FPS
const uint32_t FRAME_TIME_US = 1000000 / TARGET_FPS;
static uint32_t last_frame_micros = 0;

// 드라이버 객체 (전송용) - 4개의 OLED를 각각 제어
// HW I2C 버스 (2개) 및 SW I2C 버스 (2개) 사용
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_1(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_3(U8G2_R0, sw_scl_pin, sw_sda_pin, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_4(U8G2_R0, sw_scl_pin, sw_sda_pin, U8X8_PIN_NONE);

U8G2* screens[4] = { &u8g2_1, &u8g2_2, &u8g2_3, &u8g2_4 };

// --- [2] LovyanGFX 가상 캔버스 설정 (512x64 와이드 화면) ---
LGFX_Sprite canvas;
uint8_t shadow_buffer[4][1024]; // 변경 감지용 독립 버퍼 (Dirty Checking)

// --- [2-1] FPS 측정용 인터럽트 변수 ---
volatile uint32_t frame_counter = 0;
Ticker fps_ticker;

void onFpsReport() {
    Serial.printf("REAL-TIME FPS: %u\n", frame_counter);
    frame_counter = 0;
}

// --- [3] 고속 SW I2C 콜백 (ESP32-C3 최적화) ---
extern "C" uint8_t u8x8_gpio_and_delay_esp32_c3_fast(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  uint8_t pin;
  switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
      pin = u8x8->pins[U8X8_PIN_I2C_CLOCK];
      if (pin != U8X8_PIN_NONE) { pinMode(pin, OUTPUT_OPEN_DRAIN); gpio_set_level((gpio_num_t)pin, 1); }
      pin = u8x8->pins[U8X8_PIN_I2C_DATA];
      if (pin != U8X8_PIN_NONE) { pinMode(pin, OUTPUT_OPEN_DRAIN); gpio_set_level((gpio_num_t)pin, 1); }
      break;
    case U8X8_MSG_DELAY_MILLI: delay(arg_int); break;
    case U8X8_MSG_DELAY_10MICRO: break; 
    case U8X8_MSG_DELAY_100NANO: break; 
    case U8X8_MSG_GPIO_I2C_CLOCK:
      pin = u8x8->pins[U8X8_PIN_I2C_CLOCK];
      if (pin != U8X8_PIN_NONE) {
        if (arg_int) GPIO.out_w1ts.val = (1 << pin); // Set High
        else         GPIO.out_w1tc.val = (1 << pin); // Set Low
      }
      break;
    case U8X8_MSG_GPIO_I2C_DATA:
      pin = u8x8->pins[U8X8_PIN_I2C_DATA];
      if (pin != U8X8_PIN_NONE) {
        if (arg_int) GPIO.out_w1ts.val = (1 << pin); // Set High
        else         GPIO.out_w1tc.val = (1 << pin); // Set Low
      }
      break;
    default: u8x8_SetGPIOResult(u8x8, 1); break;
  }
  return 1;
}

// --- [4] 와이드 캔버스 전송 엔진 (Smart Flush) ---
void pushSmartWideCanvas() {
  // 프레임 레이트 제한 (너무 빠르면 대기)
  uint32_t now_us = micros();
  uint32_t elapsed = now_us - last_frame_micros;
  if (elapsed < FRAME_TIME_US) {
    uint32_t wait_us = FRAME_TIME_US - elapsed;
    if (wait_us > 1000) delay(wait_us / 1000); // 1ms 이상이면 delay() 사용
    while (micros() - last_frame_micros < FRAME_TIME_US); // 남은 시간 미세 대기
  }
  last_frame_micros = micros();

  frame_counter++; 
  uint8_t* canvas_ptr = (uint8_t*)canvas.getBuffer();
  
  for (int s = 0; s < 4; s++) {
    uint8_t* u8g2_buf = screens[s]->getBufferPtr();
    
    for (int p = 0; p < 8; p++) {
      uint8_t page_cache[128];
      bool page_dirty = false;
      
      for (int x = 0; x < 128; x++) {
        int gx = (s * 128) + x;
        int byte_idx = gx >> 3;
        int bit_mask = 0x80 >> (gx & 7);
        
        uint8_t out_byte = 0;
        for (int b = 0; b < 8; b++) {
          if (canvas_ptr[((p << 3) + b) * 64 + byte_idx] & bit_mask) {
            out_byte |= (1 << b);
          }
        }
        
        page_cache[x] = out_byte;
        if (page_cache[x] != shadow_buffer[s][p * 128 + x]) {
          page_dirty = true;
        }
      }

      if (page_dirty) {
        memcpy(&shadow_buffer[s][p * 128], page_cache, 128);
        memcpy(&u8g2_buf[p * 128], page_cache, 128);
        screens[s]->updateDisplayArea(0, p, 16, 1);
      }
    }
  }
}

// 기존 delay()를 대체하는 스마트 딜레이 (지연 중에도 FPS 유지)
void smartDelay(uint32_t ms) {
    uint32_t start_ms = millis();
    while (millis() - start_ms < ms) {
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
  
  Wire.begin(hw_sda_pin, hw_scl_pin);
  Wire.setClock(800000);

  u8g2_3.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;
  u8g2_4.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;

  // HW I2C 클럭 설정 (U8g2 내부 설정 방식)
  u8g2_1.setBusClock(800000);
  u8g2_2.setBusClock(800000);

  u8g2_1.setI2CAddress(0x3C * 2); u8g2_1.begin();
  u8g2_2.setI2CAddress(0x3D * 2); u8g2_2.begin();
  u8g2_3.setI2CAddress(0x3C * 2); u8g2_3.begin();
  u8g2_4.setI2CAddress(0x3D * 2); u8g2_4.begin();

  // FPS 타이머 인터럽트 시작 (1초마다 onFpsReport 실행)
  fps_ticker.attach(1.0, onFpsReport);

  // 캔버스 초기화 (512x64, 1비트 컬러)
  canvas.setColorDepth(1);
  canvas.createSprite(512, 64);
  canvas.setBitmapColor(1, 0); 
  memset(shadow_buffer, 0, sizeof(shadow_buffer));

  // 시작 메시지
  canvas.clear();
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.drawCenterString("WIDE OLED DEMO", 256, 20);
  pushSmartWideCanvas();
  smartDelay(2000);
}

void loop() {
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