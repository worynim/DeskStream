// worynim@gmail.com
/**
 * @file DS_DECK.ino
 * @brief BLE 매크로 키보드 (Stream Deck 스타일) 메인 펌웨어
 * @details 4개의 OLED 디스플레이를 독립적으로 제어하며, BLE HID를 통한 단축키 및 문자열 전송 기능 구현
 */
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "driver/gpio.h"
#include "soc/gpio_struct.h"

#include "LittleFS.h"
#include <WiFiManager.h>
#include <WebServer.h>  // 표준 ESP32 WebServer (기존 프로젝트에서 검증됨)
#include <ArduinoJson.h>
#include <HijelHID_BLEKeyboard.h>

#include "web_handlers.h"  // 프리미엄 UI 및 웹 라우팅 분리

// === 물리적 핀 정의 ===
#define BTN1_PIN 1
#define BTN2_PIN 4
#define BTN3_PIN 10
#define BTN4_PIN 9
#define HW_SDA_PIN 5
#define HW_SCL_PIN 6
#define SW_SDA_PIN 2
#define SW_SCL_PIN 3
#define BUZZER_PIN 7

// === 전역 객체 ===
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_1(U8G2_R2, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_2(U8G2_R2, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_3(U8G2_R2, SW_SCL_PIN, SW_SDA_PIN, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_4(U8G2_R2, SW_SCL_PIN, SW_SDA_PIN, U8X8_PIN_NONE);
U8G2* displays[] = { &u8g2_1, &u8g2_2, &u8g2_3, &u8g2_4 };
HijelHID_BLEKeyboard bleKeyboard("DS DECK", "DeskStream", 100);

WebServer server(80);  // 표준 웹 서버 객체
uint8_t shadow_buffer[4][1024] = { 0 };

// === 매크로 설정 전역 배열 (web_handlers.h의 정의를 사용) ===
KeyConfig deckConfigs[4];
bool isShowingIP = false;

// === 고속 SW I2C 콜백 (레지스터 직접 제어) ===
extern "C" uint8_t u8x8_gpio_and_delay_esp32_c3_fast(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
  switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
      pinMode(u8x8->pins[U8X8_PIN_I2C_CLOCK], OUTPUT_OPEN_DRAIN);
      pinMode(u8x8->pins[U8X8_PIN_I2C_DATA], OUTPUT_OPEN_DRAIN);
      gpio_pullup_en((gpio_num_t)SW_SCL_PIN);
      gpio_pullup_en((gpio_num_t)SW_SDA_PIN);
      break;
    case U8X8_MSG_GPIO_I2C_CLOCK:
      if (arg_int) GPIO.out_w1ts.val = (1 << SW_SCL_PIN);
      else GPIO.out_w1tc.val = (1 << SW_SCL_PIN);
      break;
    case U8X8_MSG_GPIO_I2C_DATA:
      if (arg_int) GPIO.out_w1ts.val = (1 << SW_SDA_PIN);
      else GPIO.out_w1tc.val = (1 << SW_SDA_PIN);
      break;
    default: break;
  }
  return 1;
}

// === OLED 더티 체킹 전송 ===
void pushDirty(int idx) {
  uint8_t* buf = displays[idx]->getBufferPtr();
  if (!buf) return;
  for (int p = 0; p < 8; p++) {
    int first = -1, last = -1;
    for (int t = 0; t < 16; t++) {
      bool dirty = false;
      for (int tx = 0; tx < 8; tx++) {
        int i = p * 128 + t * 8 + tx;
        if (buf[i] != shadow_buffer[idx][i]) {
          dirty = true;
          shadow_buffer[idx][i] = buf[i];
        }
      }
      if (dirty) {
        if (first == -1) first = t;
        last = t;
      }
    }
    if (first != -1) displays[idx]->updateDisplayArea(first, p, last - first + 1, 1);
  }
}

void loadConfig() {
  if (!LittleFS.exists("/config.json")) {
    // 기본값 생성
    for (int i = 0; i < 4; i++) {
      sprintf(deckConfigs[i].label, "BTN %d", i + 1);
      deckConfigs[i].mode = MODE_STRING;
      sprintf(deckConfigs[i].stringVal, "%c", 'A' + i);
      deckConfigs[i].korVal[0] = '\0';
      memset(deckConfigs[i].modifiers, 0, 3);
      deckConfigs[i].key = 0;
    }
    return;
  }
  File f = LittleFS.open("/config.json", "r");
  if (!f) {
    Serial.println("[FS] config.json open failed");
    return;
  }
  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, f);
  f.close();  // 에러 여부와 관계없이 항상 파일 핸들 닫기
  if (err) {
    Serial.printf("[FS] JSON parse error: %s\n", err.c_str());
    return;  // 기본값 유지
  }
  JsonArray keys = doc["keys"];
  if (keys.isNull()) {
    Serial.println("[FS] 'keys' array missing");
    return;
  }
  for (int i = 0; i < 4; i++) {
    strlcpy(deckConfigs[i].label, keys[i]["label"] | "", sizeof(deckConfigs[i].label));
    deckConfigs[i].mode = keys[i]["mode"] | MODE_STRING;
    strlcpy(deckConfigs[i].stringVal, keys[i]["stringVal"] | "", sizeof(deckConfigs[i].stringVal));
    strlcpy(deckConfigs[i].korVal, keys[i]["korVal"] | "", sizeof(deckConfigs[i].korVal));
    deckConfigs[i].modifiers[0] = keys[i]["mod0"] | 0;
    deckConfigs[i].modifiers[1] = keys[i]["mod1"] | 0;
    deckConfigs[i].modifiers[2] = keys[i]["mod2"] | 0;
    deckConfigs[i].key = keys[i]["key"] | 0;
  }
}

void drawDefaultScreen(int idx) {
  uint8_t* buf = displays[idx]->getBufferPtr();
  if (buf) memset(buf, 0, 1024); // clearBuffer() 대신 직접 메모리 초기화로 속도 향상
  
  displays[idx]->setFontPosBaseline(); // 상태 복구 (중요: IP 화면의 Top 정렬 영향 방지)

  char path[24];
  sprintf(path, "/icon%d.bin", idx + 1);
  bool hasIcon = false;

  if (LittleFS.exists(path)) {
    File f = LittleFS.open(path, "r");
    if (f) {
      f.read(displays[idx]->getBufferPtr(), 1024);
      f.close();
      // ★ 중요: 여기서 shadow_buffer 복사 부분을 제거해야 pushDirty가 실제로 이미지를 I2C로 전송합니다
      hasIcon = true;
    }
  }

  // 이미지가 없거나 읽기 실패한 경우에만 라벨 또는 미디어 아이콘 출력
  if (!hasIcon) {
    displays[idx]->setDrawColor(1);

    if (deckConfigs[idx].mode == MODE_MEDIA) {
      displays[idx]->setFont(u8g2_font_open_iconic_play_6x_t);
      char iconChar = 78;  // 기본 음표
      uint8_t mk = deckConfigs[idx].key;
      if (mk == 1) iconChar = 79;
      else if (mk == 2) iconChar = 80;
      else if (mk == 3) iconChar = 81;
      else if (mk == 4) iconChar = 78;
      else if (mk == 5) iconChar = 74;
      else if (mk == 6) iconChar = 73;

      char iconStr[2] = { iconChar, '\0' };
      int w = displays[idx]->getStrWidth(iconStr);
      displays[idx]->drawStr((128 - w) / 2, 56, iconStr);
    } else if (deckConfigs[idx].mode == MODE_BROWSER) {
      displays[idx]->setFont(u8g2_font_open_iconic_all_6x_t);  // 더 범용적인 아이콘 폰트
      char iconStr[2] = { 175, '\0' };                         // 지구본 아이콘
      int w = displays[idx]->getStrWidth(iconStr);
      displays[idx]->drawStr((128 - w) / 2, 56, iconStr);
    } else {
      // 일반 텍스트 라벨 모드
      displays[idx]->setFont(u8g2_font_ncenB10_tf);  // 폰트 변경 (호환성 향상)
      int w = displays[idx]->getStrWidth(deckConfigs[idx].label);
      displays[idx]->drawStr((128 - w) / 2, 40, deckConfigs[idx].label);
    }
  }

  pushDirty(idx);
}

// === OLED 헬퍼 함수 (Smart_Info_Station 스타일) ===
void u8g2_prepare(U8G2* u8g2) {
  u8g2->setBitmapMode(1);
  u8g2->setDrawColor(1);
  u8g2->setFontMode(1);
  u8g2->setFontPosTop();
}

void drawCenteredText(U8G2* u8g2, String text, int y) {
  const char* str = text.c_str();
  int textWidth = u8g2->getUTF8Width(str);
  int x = (128 - textWidth) / 2;
  if (x < 0) x = 0;
  u8g2->drawStr(x, y, str);
}

void display_ip_page() {
  IPAddress ip = WiFi.localIP();
  for (int i = 0; i < 4; i++) {
    displays[i]->clearBuffer();
    u8g2_prepare(displays[i]);

    // 첫 번째 화면에만 제목 표시
    if (i == 0) {
      displays[i]->setFont(u8g2_font_6x10_tf);
      drawCenteredText(displays[i], "SETTING ADDR", 5);
    }

    // IP 각 마디를 크게 표시
    displays[i]->setFont(u8g2_font_maniac_tr);
    String segment = String(ip[i]);
    drawCenteredText(displays[i], segment, 25);

    // 마지막 화면 제외하고 점(.) 표시용 세로 바 또는 점 추가 (옵션)
    if (i < 3) {
      displays[i]->setFont(u8g2_font_ncenB14_tr);
      displays[i]->drawStr(115, 35, ".");
    }
    pushDirty(i);
  }
  isShowingIP = true;
}

// === ASCII/Dashboard 코드를 Raw HID 코드로 변환하는 통합 헬퍼 함수 ===
uint8_t getHIDFromASCII(uint8_t c) {
  // 2. 대시보드 전용 특수 매핑 (이전 저장 데이터와의 완벽한 호환성을 위한 원본 값 복구)
  // 방향키 (기존 176~179)
  if (c == 0xB2) return KEY_LEFT;      // 178
  if (c == 0xB3) return KEY_RIGHT;     // 179
  if (c == 0xB1) return KEY_UP;        // 177
  if (c == 0xB0) return KEY_DOWN;      // 176

  // 제어 문자 (기존 8,9,10,27)
  if (c == 0x0A || c == 0x0D) return KEY_RETURN;    // 10 / 13
  if (c == 0x1B) return KEY_ESCAPE;    // 27
  if (c == 0x08) return KEY_BACKSPACE; // 8
  if (c == 0x09) return KEY_TAB;       // 9

  // 특수 기능키 (기존 128, 206~214)
  if (c == 0x80) return KEY_CAPS_LOCK;    // 128
  if (c == 0xCE) return KEY_PRINT_SCREEN; // 206
  if (c == 0xD2) return KEY_HOME;         // 210
  if (c == 0xD5) return KEY_END;          // 213
  if (c == 0xD4) return KEY_DELETE;       // 212
  if (c == 0xD3) return KEY_PAGE_UP;      // 211
  if (c == 0xD6) return KEY_PAGE_DOWN;    // 214

  // F1 ~ F12 (기존 215~226 = 0xD7~0xE2)
  if (c >= 0xD7 && c <= 0xE2) return KEY_F1 + (c - 0xD7);

  // F13 ~ F24 (기존 227~238 = 0xE3~0xEE)
  if (c >= 0xE3 && c <= 0xEE) return KEY_F13 + (c - 0xE3);

  // F19, F20 (한영 전환 / 기존 233, 234)
  if (c == 0xE9) return KEY_F19;          // 233
  if (c == 0xEA) return KEY_F20;          // 234


  // 4. 일반 알파벳 및 숫자 (HID 표준 매핑 4-39)
  if (c >= 'a' && c <= 'z') return c - 'a' + 4;
  if (c >= 'A' && c <= 'Z') return c - 'A' + 4;
  if (c >= '1' && c <= '9') return c - '1' + 30;
  if (c == '0') return 39;

  // 5. 표준 기호류
  if (c == ' ') return 44;
  if (c == '-') return 45;
  if (c == '=') return 46;
  if (c == '[') return 47;
  if (c == ']') return 48;
  if (c == '\\') return 49;
  if (c == ';') return 51;
  if (c == '\'') return 52;
  if (c == '`') return 53;
  if (c == ',') return 54;
  if (c == '.') return 55;
  if (c == '/') return 56;

  return c;
}

// === 버튼 피드백 및 논스토핑 로직 ===
struct Button {
  int pin;
  int index;
  bool lastState = HIGH;
  unsigned long pressTime = 0;
  bool active = false;
  unsigned long feedbackEnd = 0;
  bool isPressed = false;
  bool isLongPressFired = false;

  Button(int p, int i)
    : pin(p), index(i) {}

  void init() {
    pinMode(pin, INPUT_PULLUP);
  }

  void update() {
    bool state = digitalRead(pin);
    unsigned long now = millis();

    // 1. 버튼이 눌리는 순간 (Falling Edge)
    if (state == LOW && lastState == HIGH) {
      pressTime = now;
      isPressed = true;
      isLongPressFired = false;
    } 
    // 2. 버튼이 눌려있는 동안 (Holding)
    else if (state == LOW) {
      if (isPressed && !isLongPressFired && (now - pressTime >= 1000)) {
        isLongPressFired = true;
        
        Serial.print("[BTN ");
        Serial.print(index + 1);
        Serial.println("] Long Press");

        // BTN4 롱 프레스: IP 주소 표시
        if (index == 3 && !isShowingIP) {
          display_ip_page();
          tone(BUZZER_PIN, 1500, 200);
        }
      }
    }
    // 3. 버튼을 떼는 순간 (Rising Edge)
    else if (state == HIGH && lastState == LOW) {
      isPressed = false;
      
      // 롱 프레스가 실행되지 않았을 때만 숏 프레스 체크 (디바운싱 50ms)
      if (!isLongPressFired && (now - pressTime > 50)) {
        if (isShowingIP) {
          // IP 표시 중 아무 버튼이나 누르면 원래 화면으로 복구
          isShowingIP = false;
          for (int i = 0; i < 4; i++) drawDefaultScreen(i);
          tone(BUZZER_PIN, 2000, 50);
          lastState = HIGH; // 중복 입력 방지
          return;
        }

        displays[index]->clearBuffer();
        displays[index]->setFont(u8g2_font_6x10_tf);
        displays[index]->drawStr(10, 35, "PRESSED!");
        pushDirty(index);

        Serial.printf("[BTN %d] Action Complete\n", index + 1);

        if (bleKeyboard.isConnected()) {
          if (deckConfigs[index].mode == MODE_STRING) {
            // 초고속 타이핑 로직 & 자동 한/영 전환 (F19, F20 방식 적용)
            String s = String(deckConfigs[index].stringVal);
            int i = 0;
            
            while (i < s.length()) {
              if (s.substring(i).startsWith("[#ENG#]")) {
                bleKeyboard.press(KEY_RIGHT);
                delay(20);
                bleKeyboard.releaseAll();
                delay(100);
                bleKeyboard.press(KEY_F19);
                delay(20);
                bleKeyboard.releaseAll();
                delay(40);
                i += 7;
              } else if (s.substring(i).startsWith("[#KOR#]")) {
                bleKeyboard.press(KEY_F20);
                delay(20);
                bleKeyboard.releaseAll();
                delay(40);
                i += 7;
              } else if (s.substring(i).startsWith("[#CAPS#]")) {
                // 기존 토글 기반 호환성 대응
                bleKeyboard.tap(KEY_SPACE, KEY_MOD_LCTRL);
                delay(60);
                i += 8;
              } else {
                // 다음 제어 토큰을 찾고, 그 사이 문자들을 출력
                int nextEng = s.indexOf("[#ENG#]", i);
                int nextKor = s.indexOf("[#KOR#]", i);
                int nextCaps = s.indexOf("[#CAPS#]", i);
                
                int nextToken = s.length();
                if (nextEng != -1 && nextEng < nextToken) nextToken = nextEng;
                if (nextKor != -1 && nextKor < nextToken) nextToken = nextKor;
                if (nextCaps != -1 && nextCaps < nextToken) nextToken = nextCaps;
                
                bleKeyboard.print(s.substring(i, nextToken));
                i = nextToken;
              }
            }
          } else if (deckConfigs[index].mode == MODE_MEDIA) {
            uint8_t mk = deckConfigs[index].key;
            if (mk == 1) bleKeyboard.press(MEDIA_VOLUME_UP);
            else if (mk == 2) bleKeyboard.press(MEDIA_VOLUME_DOWN);
            else if (mk == 3) bleKeyboard.press(MEDIA_MUTE);
            else if (mk == 4) bleKeyboard.press(MEDIA_PLAY_PAUSE);
            else if (mk == 5) bleKeyboard.press(MEDIA_NEXT_TRACK);
            else if (mk == 6) bleKeyboard.press(MEDIA_PREV_TRACK);
            delay(20);  // 미디어 키 인식 시간 단축 (40 -> 20)
            bleKeyboard.releaseAll();
          } else if (deckConfigs[index].mode == MODE_BROWSER) {
            // 유니버설 브라우저 런처 (가장 깔끔한 Spotlight 기본 브라우저 실행 방식)
            // 1. Spotlight 호출
            bleKeyboard.press(KEY_LGUI);  // KEY_LGUI == cmd 
            bleKeyboard.press(KEY_SPACE);
            delay(50);
            bleKeyboard.releaseAll();
            delay(500);  // Spotlight 활성화 대기

            // 기존 타이핑 내용 삭제 (텍스트 전체 선택 상태에서 백스페이스)
            bleKeyboard.press(KEY_BACKSPACE);
            delay(50);
            bleKeyboard.releaseAll();
            delay(50);
            
            // Spotlight에서 영문 URL 압력을 위해 확실한 '영어 모드(F19)'로 전환
            bleKeyboard.press(KEY_F19);
            delay(20);
            bleKeyboard.releaseAll();
            delay(40);

            // 2. 패시브 타이핑: 기기에 저장된 오염된 한영 전환 신호 태그 완전 무시
            bleKeyboard.setTapDelay(20);  // 오타 방지를 위한 안정적 타이핑 속도

            String s = String(deckConfigs[index].stringVal);
            s.replace("[#ENG#]", "");
            s.replace("[#KOR#]", "");
            s.replace("[#CAPS#]", "");  // 구형 토큰 무시

            bleKeyboard.print(s);

            delay(2500);  // Mac Spotlight가 주소를 완전히 파싱하고 '웹사이트' 검색 결과를 준비할 시간 부여

            // 3. 브라우저 실행(Return)
            bleKeyboard.press(KEY_RETURN);    // 확실한 물리적 Enter 키코드 상수(0xB0) 사용
            delay(50);
            bleKeyboard.releaseAll();

            bleKeyboard.setTapDelay(1);  // 다시 초고속 모드로 복구
          } else if (deckConfigs[index].mode == MODE_COMBO) {
            // 콤보(단축키) 모드
            
            // [한글 조합 깨짐 방지] 라이브러리에 정의된 KEY_F19, KEY_F20 사용 시 
            // 현재 입력 중인 한글이 마쳐지지 않고 덮어씌워지는 것을 방지하기 위해 
            // 오른쪽 방향키를 한 번 보내 조합을 강제로 완료(Commit)시킵니다.
            uint8_t configKey = deckConfigs[index].key;
            uint8_t hidCode = getHIDFromASCII(configKey);

            if (hidCode == KEY_F19 || hidCode == KEY_F20) {
              bleKeyboard.press(KEY_RIGHT);
              delay(30);
              bleKeyboard.releaseAll();
              delay(30);
            }

            if (deckConfigs[index].modifiers[0]) bleKeyboard.press((uint8_t)deckConfigs[index].modifiers[0]);
            if (deckConfigs[index].modifiers[1]) bleKeyboard.press((uint8_t)deckConfigs[index].modifiers[1]);
            if (deckConfigs[index].modifiers[2]) bleKeyboard.press((uint8_t)deckConfigs[index].modifiers[2]);

            if (hidCode != 0) {
              bleKeyboard.press(hidCode);
            }
            delay(10);  // 단축키 인식 안정성 한계치
            bleKeyboard.releaseAll();
          }
        }
        active = true;
        feedbackEnd = now + 150;
        tone(BUZZER_PIN, 3000, 50);
      }
    }
    lastState = state;

    // 2. 비동기 화면 복구
    if (active && now > feedbackEnd) {
      active = false;
      drawDefaultScreen(index);
    }
  }
};

Button btns[4] = { Button(BTN1_PIN, 0), Button(BTN2_PIN, 1), Button(BTN3_PIN, 2), Button(BTN4_PIN, 3) };

void setup() {
  Serial.begin(115200);
  Serial.println("\n[SYSTEM] DS DECK Booting...");

  // 1. 하드웨어 및 OLED 초기화 (사용자 피드백용)
  Wire.begin(HW_SDA_PIN, HW_SCL_PIN);
  Wire.setClock(400000);
  
  // 1번 OLED 우선 기동하여 로그 표시
  u8g2_1.setI2CAddress(0x3C * 2);
  u8g2_1.begin();
  u8g2_1.clearBuffer();
  u8g2_1.setFont(u8g2_font_6x10_tf);
  u8g2_1.drawStr(0, 10, "DS DECK SYSTEM");
  u8g2_1.drawStr(0, 25, "Booting...");
  pushDirty(0); // sendBuffer() 대신 pushDirty() 사용하여 shadow_buffer 동기화

  // 2. BLE 최우선 시작 (WiFi 이전에 실행하여 검색성 확보)
  u8g2_1.drawStr(0, 40, "> BLE INIT...");
  pushDirty(0);
  bleKeyboard.begin();
  bleKeyboard.setTapDelay(1);
  Serial.println("[BLE] Bluetooth Started");

  // 3. LittleFS 및 설정 로드
  u8g2_1.drawStr(0, 55, "> FS MOUNT...");
  pushDirty(0);
  if (!LittleFS.begin(true)) {
    Serial.println("[FS] LittleFS Mount Failed");
  } else {
    loadConfig();
  }

  // 나머지 하드웨어 및 OLED 초기화
  for (int i = 0; i < 4; i++) btns[i].init();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  u8g2_2.setI2CAddress(0x3D * 2);
  u8g2_2.begin();
  u8g2_3.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;
  u8g2_3.begin();
  u8g2_4.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;
  u8g2_4.setI2CAddress(0x3D * 2);
  u8g2_4.begin();

  // 4. WiFi 연결 (WiFiManager) - 여기서 블로킹되어도 BLE는 이미 작동 중
  u8g2_1.clearBuffer();
  u8g2_1.drawStr(0, 10, "DS DECK SYSTEM");
  u8g2_1.drawStr(0, 30, "> WiFi Connecting...");
  u8g2_1.drawStr(0, 45, "(Wait 60s or Setup)");
  pushDirty(0);

  WiFiManager wm;
  wm.setConfigPortalTimeout(60);
  if (wm.autoConnect("DS_DECK_SETUP")) {
    Serial.println("[NET] Connected to WiFi");
    initWebHandlers();
  } else {
    Serial.println("[NET] WiFi Timeout - Standalone Mode");
  }

  // 5. 최종 화면 출력 (여기서 pushDirty가 로그를 지워줌)
  for (int i = 0; i < 4; i++) drawDefaultScreen(i);
  Serial.println("[SYSTEM] Ready.");
}

void loop() {
  for (int i = 0; i < 4; i++) btns[i].update();

  // WebServer 클라이언트 처리
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }
}
