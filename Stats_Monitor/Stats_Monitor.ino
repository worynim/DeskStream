#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <Wire.h>

/**
 * 🛠 Stats_Monitor: GPS_DASH 스타일의 세련된 UI 버전
 */

// --- I2C 및 OLED 설정 ---
const uint8_t hw_sda_pin = 5;
const uint8_t hw_scl_pin = 6;
const uint8_t sw_sda_pin = 2;
const uint8_t sw_scl_pin = 3;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_1(U8G2_R0, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_2(U8G2_R0, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_3(U8G2_R0, sw_scl_pin, sw_sda_pin, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_4(U8G2_R0, sw_scl_pin, sw_sda_pin, U8X8_PIN_NONE);

// --- BLE 설정 ---
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

BLEServer *pServer = NULL;
bool deviceConnected = false;
bool dataReceived = false;

struct SysStats {
  float cpu_u = 0, cpu_t = 0;
  float gpu_u = 0, gpu_t = 0;
  float ram_u = 0, disk_u = 0;
  float power = 0;
} stats;

// --- 헬퍼 함수: 중앙 정렬 텍스트 ---
void drawCentered(U8G2 &u, const char* text, int y, const uint8_t *font) {
  u.setFont(font);
  u.drawStr((128 - u.getStrWidth(text)) / 2, y, text);
}

// --- 헬퍼 함수: 세련된 헤더 ---
void drawHeader(U8G2 &u, const char* label) {
  u.setFont(u8g2_font_7x14B_tf);
  u.drawStr(0, 12, label);
  u.drawHLine(0, 15, 128); // 헤더 라인
}

// --- 헬퍼 함수: 슬림 프로그레스 바 ---
void drawSlimBar(U8G2 &u, int y, float p) {
  u.drawFrame(0, y, 128, 6);
  int w = (int)(126 * (p / 100.0));
  if (w > 0) u.drawBox(1, y + 1, w, 4);
}

// --- 화면별 그리기 함수 ---

void drawCPU(U8G2 &u) {
  u.clearBuffer();
  drawHeader(u, "PROCESSOR (CPU)");
  
  char b[16];
  snprintf(b, 16, "%d", (int)stats.cpu_u);
  drawCentered(u, b, 50, u8g2_font_maniac_tr); // GPS_DASH 스타일 대형 폰트
  
  u.setFont(u8g2_font_6x10_tf);
  u.drawStr(95, 50, "%");
  
  drawSlimBar(u, 58, stats.cpu_u);
  u.sendBuffer();
}

void drawGPU(U8G2 &u) {
  u.clearBuffer();
  drawHeader(u, "GRAPHICS (GPU/NW)");
  
  char b[16];
  snprintf(b, 16, "%d", (int)stats.gpu_u);
  drawCentered(u, b, 50, u8g2_font_maniac_tr);
  
  u.setFont(u8g2_font_6x10_tf);
  u.drawStr(95, 50, "%");
  
  drawSlimBar(u, 58, stats.gpu_u);
  u.sendBuffer();
}

void drawStorage(U8G2 &u) {
  u.clearBuffer();
  drawHeader(u, "MEMORY / DISK");
  
  u.setFont(u8g2_font_6x10_tf);
  u.drawStr(0, 30, "RAM");
  char b[16]; snprintf(b, 16, "%d%%", (int)stats.ram_u);
  u.drawStr(100, 30, b);
  drawSlimBar(u, 33, stats.ram_u);
  
  u.drawStr(0, 53, "DISK");
  snprintf(b, 16, "%d%%", (int)stats.disk_u);
  u.drawStr(100, 53, b);
  drawSlimBar(u, 56, stats.disk_u);
  
  u.sendBuffer();
}

void drawPower(U8G2 &u) {
  u.clearBuffer();
  drawHeader(u, "POWER CONSUMP.");
  
  char b[16];
  snprintf(b, 16, "%.1f", stats.power);
  drawCentered(u, b, 50, u8g2_font_maniac_tr);
  
  u.setFont(u8g2_font_7x14B_tf);
  u.drawStr(95, 52, "W");
  
  u.setFont(u8g2_font_6x10_tf);
  drawCentered(u, "Real-time Watts", 64, u8g2_font_6x10_tf);
  u.sendBuffer();
}

// --- BLE 콜백 ---
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *p) {
      deviceConnected = true;
      Serial.println("[BLE] Client connected.");
    }
    void onDisconnect(BLEServer *p) {
      deviceConnected = false;
      Serial.println("[BLE] Client disconnected. Re-advertising...");
      pServer->getAdvertising()->start();
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
        if (doc.containsKey("p_w")) stats.power = doc["p_w"];
        dataReceived = true;
        Serial.printf("[DATA] CPU:%.0f%% T:%.0fC GPU:%.0f%% RAM:%.0f%% DSK:%.0f%% PWR:%.1fW\n",
          stats.cpu_u, stats.cpu_t, stats.gpu_u, stats.ram_u, stats.disk_u, stats.power);
      } else {
        Serial.println("[ERR] JSON parse failed.");
      }
    }
};

void setup() {
  Serial.begin(115200);
  
  Wire.begin(hw_sda_pin, hw_scl_pin);
  u8g2_1.setI2CAddress(0x3C*2); u8g2_1.begin(); u8g2_2.setI2CAddress(0x3D*2); u8g2_2.begin();
  u8g2_3.setI2CAddress(0x3C*2); u8g2_3.begin(); u8g2_4.setI2CAddress(0x3D*2); u8g2_4.begin();

  BLEDevice::init("DeskStream_Stats");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pSvc = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pRx = pSvc->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pRx->setCallbacks(new MyCallbacks());
  pSvc->start();
  pServer->getAdvertising()->start();
  Serial.println("[SYSTEM] Stats Monitor Ready. Waiting for BLE connection...");
}

void loop() {
  if (dataReceived) {
    drawCPU(u8g2_1);
    drawGPU(u8g2_2);
    drawStorage(u8g2_3);
    drawPower(u8g2_4);
    dataReceived = false;
  } else {
    static bool lastState = false;
    if (deviceConnected != lastState) {
      lastState = deviceConnected;
      const char* msg = deviceConnected ? "CONNECTED" : "WAITING...";
      u8g2_1.clearBuffer(); drawCentered(u8g2_1, msg, 40, u8g2_font_7x14B_tf); u8g2_1.sendBuffer();
      u8g2_2.clearBuffer(); drawCentered(u8g2_2, msg, 40, u8g2_font_7x14B_tf); u8g2_2.sendBuffer();
      u8g2_3.clearBuffer(); drawCentered(u8g2_3, msg, 40, u8g2_font_7x14B_tf); u8g2_3.sendBuffer();
      u8g2_4.clearBuffer(); drawCentered(u8g2_4, msg, 40, u8g2_font_7x14B_tf); u8g2_4.sendBuffer();
    }
  }
  delay(100);
}
