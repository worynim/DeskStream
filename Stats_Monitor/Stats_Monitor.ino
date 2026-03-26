#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "driver/gpio.h"      // ESP32 하드웨어 레지스터 직접 제어를 위한 헤더
#include "soc/gpio_struct.h"  // GPIO 구조체 정의 (고속 GPIO 조작용)

/**
 * 🛠 Stats_Monitor: ESP32-C3 기반 4개 OLED 시스템 상태 모니터
 * - 2개의 하드웨어 I2C 및 2개의 소프트웨어 I2C 사용
 * - BLE를 통한 실시간 PC 상태 데이터 수신
 * - 고속 렌더링 최적화 (Dirty Checking 및 Fast GPIO) 적용
 */

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
bool deviceConnected = false;
bool dataReceived = false;

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
void drawCPU(U8G2 &u) {
  u.clearBuffer();
  drawHeader(u, "PROCESSOR (CPU)");
  char b[16];
  snprintf(b, 16, "%d", (int)stats.cpu_u);
  drawCentered(u, b, 22, u8g2_font_7_Seg_33x19_mn);
  u.setFont(u8g2_font_profont15_tr);
  u.drawStr(100, 50, "%");
  drawSlimBar(u, 60, stats.cpu_u);
  smartSendBuffer(0);
}

// Screen 2: GPU 점유율 (또는 보조 지표)
void drawGPU(U8G2 &u) {
  u.clearBuffer();
  drawHeader(u, "GRAPHICS (GPU)");
  char b[16];
  snprintf(b, 16, "%d", (int)stats.gpu_u);
  drawCentered(u, b, 22, u8g2_font_7_Seg_33x19_mn);
  u.setFont(u8g2_font_profont15_tr);
  u.drawStr(100, 50, "%");
  drawSlimBar(u, 60, stats.gpu_u);
  smartSendBuffer(1);
}

// Screen 3: 메모리 및 디스크 사용량
void drawStorage(U8G2 &u) {
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
  smartSendBuffer(2);
}

// 네트워크 속도 단위 변환 (KB/s -> MB/s)
void formatNetSpeed(float speed_kb, char *out_val, char *out_unit) {
  if (speed_kb >= 1024.0) {
    snprintf(out_val, 16, "%.1f", speed_kb / 1024.0);
    strcpy(out_unit, "MB/s");
  } else {
    snprintf(out_val, 16, "%.0f", speed_kb);
    strcpy(out_unit, "KB/s");
  }
}

// Screen 4: 네트워크 트래픽 (UP/DOWN 분리)
void drawNetwork(U8G2 &u) {
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

  smartSendBuffer(3);
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
  if (dataReceived) {
    drawCPU(u8g2_1);
    drawGPU(u8g2_2);
    drawStorage(u8g2_3);
    drawNetwork(u8g2_4);
    dataReceived = false;
  } else {
    // 연결 상태 모니터링 및 메시지 표시
    static bool lastState = false;
    if (deviceConnected != lastState) {
      lastState = deviceConnected;
      const char *msg = deviceConnected ? "CONNECTED" : "WAITING...";
      for (int i = 0; i < 4; i++) {
        screens[i]->clearBuffer();
        drawCentered(*screens[i], msg, 40, u8g2_font_profont15_tr);
        smartSendBuffer(i);
      }
    }
  }
  delay(100);
}
