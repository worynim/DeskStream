#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "driver/gpio.h"
#include "soc/gpio_struct.h"

#include "LittleFS.h"      
#include <WiFiManager.h>
#include <WebServer.h>         // 표준 ESP32 WebServer (기존 프로젝트에서 검증됨)
#include <ArduinoJson.h>
#include <HijelHID_BLEKeyboard.h>

#include "web_handlers.h" // 프리미엄 UI 및 웹 라우팅 분리

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
U8G2* displays[] = {&u8g2_1, &u8g2_2, &u8g2_3, &u8g2_4};
HijelHID_BLEKeyboard bleKeyboard("DS DECK", "DeskStream", 100);

WebServer server(80); // 표준 웹 서버 객체
uint8_t shadow_buffer[4][1024] = {0};

// === 매크로 설정 전역 배열 (web_handlers.h의 정의를 사용) ===
KeyConfig deckConfigs[4];

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
            else         GPIO.out_w1tc.val = (1 << SW_SCL_PIN);
            break;
        case U8X8_MSG_GPIO_I2C_DATA:
            if (arg_int) GPIO.out_w1ts.val = (1 << SW_SDA_PIN);
            else         GPIO.out_w1tc.val = (1 << SW_SDA_PIN);
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
                    dirty = true; shadow_buffer[idx][i] = buf[i];
                }
            }
            if (dirty) { if (first == -1) first = t; last = t; }
        }
        if (first != -1) displays[idx]->updateDisplayArea(first, p, last - first + 1, 1);
    }
}

void loadConfig() {
    if (!LittleFS.exists("/config.json")) {
        // 기본값 생성
        for (int i = 0; i < 4; i++) {
            sprintf(deckConfigs[i].label, "BTN %d", i+1);
            deckConfigs[i].mode = MODE_STRING;
            sprintf(deckConfigs[i].stringVal, "%c", 'A' + i);
            deckConfigs[i].korVal[0] = '\0';
            memset(deckConfigs[i].modifiers, 0, 3);
            deckConfigs[i].key = 0;
        }
        return;
    }
    File f = LittleFS.open("/config.json", "r");
    if (!f) { Serial.println("[FS] config.json open failed"); return; }
    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, f);
    f.close(); // 에러 여부와 관계없이 항상 파일 핸들 닫기
    if (err) {
        Serial.printf("[FS] JSON parse error: %s\n", err.c_str());
        return; // 기본값 유지
    }
    JsonArray keys = doc["keys"];
    if (keys.isNull()) { Serial.println("[FS] 'keys' array missing"); return; }
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
    displays[idx]->clearBuffer();
    
    char path[24]; sprintf(path, "/icon%d.bin", idx + 1);
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
            char iconChar = 78; // 기본 음표
            uint8_t mk = deckConfigs[idx].key;
            if (mk == 1) iconChar = 79;      else if (mk == 2) iconChar = 80;
            else if (mk == 3) iconChar = 81; else if (mk == 4) iconChar = 78;
            else if (mk == 5) iconChar = 74; else if (mk == 6) iconChar = 73;
            
            char iconStr[2] = { iconChar, '\0' };
            int w = displays[idx]->getStrWidth(iconStr);
            displays[idx]->drawStr((128 - w) / 2, 56, iconStr);
        } else if (deckConfigs[idx].mode == MODE_BROWSER) {
            displays[idx]->setFont(u8g2_font_open_iconic_all_6x_t); // 더 범용적인 아이콘 폰트
            char iconStr[2] = { 175, '\0' }; // 지구본 아이콘
            int w = displays[idx]->getStrWidth(iconStr);
            displays[idx]->drawStr((128 - w) / 2, 56, iconStr);
        } else {
            // 일반 텍스트 라벨 모드
            displays[idx]->setFont(u8g2_font_ncenB10_tf); // 폰트 변경 (호환성 향상)
            int w = displays[idx]->getStrWidth(deckConfigs[idx].label);
            displays[idx]->drawStr((128 - w) / 2, 40, deckConfigs[idx].label);
        }
    }
    
    pushDirty(idx);
}

// === ASCII를 Raw HID 코드로 변환하는 헬퍼 함수 ===
uint8_t getHIDFromASCII(uint8_t c) {
    if (c >= 'a' && c <= 'z') return c - 'a' + 4;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 4;
    if (c >= '1' && c <= '9') return c - '1' + 30;
    if (c == '0') return 39;
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
    if ((uint8_t)c == 178) return 80; // Left
    if ((uint8_t)c == 179) return 79; // Right
    if ((uint8_t)c == 177) return 82; // Up
    if ((uint8_t)c == 176) return 81; // Down
    if ((uint8_t)c == 10)  return 40; // Enter
    if ((uint8_t)c == 27)  return 41; // Escape
    if ((uint8_t)c == 210) return 74; // Home
    if ((uint8_t)c == 213) return 77; // End
    if ((uint8_t)c == 212) return 76; // Delete
    if ((uint8_t)c == 211) return 75; // PgUp
    if ((uint8_t)c == 214) return 78; // PgDn
    if ((uint8_t)c == 8)   return 42; // Backspace
    if ((uint8_t)c == 9)   return 43; // Tab
    if ((uint8_t)c == 128) return 57; // CapsLock
    if ((uint8_t)c == 232) return 0;  // Fn (HID Native 지원 어려움, 예약됨)
    if (c >= 215 && c <= 226) return c - 215 + 58; // F1 to F12 (58~69)
    if ((uint8_t)c == 206) return 70; // PrtSc
    if ((uint8_t)c == 207) return 71; // ScrollLock
    if ((uint8_t)c == 208) return 72; // Pause
    if ((uint8_t)c == 209) return 73; // Insert
    if (c == 101) return 101; // Application / Menu
    return 0;
}

// === 버튼 피드백 및 논스토핑 로직 ===
struct Button {
    int pin; int index;
    bool lastState = HIGH;
    unsigned long pressTime = 0;
    bool active = false;
    unsigned long feedbackEnd = 0;

    Button(int p, int i) : pin(p), index(i) {}

    void init() { pinMode(pin, INPUT_PULLUP); }

    void update() {
        bool state = digitalRead(pin);
        unsigned long now = millis();

        if (state == LOW && lastState == HIGH) {
            pressTime = now;
        } 
        else if (state == HIGH && lastState == LOW) {
            if (now - pressTime > 50 && now - pressTime < 1000) {
                displays[index]->clearBuffer();
                displays[index]->setFont(u8g2_font_6x10_tf);
                displays[index]->drawStr(10, 35, "PRESSED!");
                pushDirty(index);
                
                Serial.printf("[BTN %d] Action Complete\n", index + 1);
                
                if (bleKeyboard.isConnected()) {
                    if (deckConfigs[index].mode == MODE_STRING) {
                        // 초고속 타이핑 로직 & 자동 한/영 전환
                        String s = String(deckConfigs[index].stringVal);
                        int lastPos = 0;
                        int capsPos = s.indexOf("[#CAPS#]");
                        
                        while(capsPos != -1) {
                            if (capsPos > lastPos) {
                                bleKeyboard.print(s.substring(lastPos, capsPos));
                            }
                            // 한/영 전환: macOS 표준 단축키 Ctrl+Space
                            bleKeyboard.tap(KEY_SPACE, KEY_MOD_LCTRL);
                            delay(60); // macOS 전환 대기시간 더 단축 (80 -> 60)
                            
                            lastPos = capsPos + 8;
                            capsPos = s.indexOf("[#CAPS#]", lastPos);
                        }
                        
                        if (lastPos < s.length()) {
                            bleKeyboard.print(s.substring(lastPos));
                        }
                    } else if (deckConfigs[index].mode == MODE_MEDIA) {
                        uint8_t mk = deckConfigs[index].key;
                        if (mk == 1) bleKeyboard.press(MEDIA_VOLUME_UP);
                        else if (mk == 2) bleKeyboard.press(MEDIA_VOLUME_DOWN);
                        else if (mk == 3) bleKeyboard.press(MEDIA_MUTE);
                        else if (mk == 4) bleKeyboard.press(MEDIA_PLAY_PAUSE);
                        else if (mk == 5) bleKeyboard.press(MEDIA_NEXT_TRACK);
                        else if (mk == 6) bleKeyboard.press(MEDIA_PREV_TRACK);
                        delay(20); // 미디어 키 인식 시간 단축 (40 -> 20)
                        bleKeyboard.releaseAll();
                    } else if (deckConfigs[index].mode == MODE_BROWSER) {
                        // 유니버설 브라우저 런처 (가장 깔끔한 Spotlight 기본 브라우저 실행 방식)
                        // 1. Spotlight 호출
                        bleKeyboard.press((uint8_t)227); bleKeyboard.press((uint8_t)44); delay(100); bleKeyboard.releaseAll();
                        delay(1000); // Spotlight 활성화 대기
                        
                        // 기존 타이핑 내용 삭제 (텍스트 전체 선택 상태에서 백스페이스)
                        bleKeyboard.press(KEY_BACKSPACE);
                        delay(50);
                        bleKeyboard.releaseAll();
                        delay(100);
                        
                        // 2. 패시브 타이핑: 기기에 저장된 오염된 한영 전환 신호([#CAPS#]) 완전 무시하고 순수 URL 문자열만 출력
                        bleKeyboard.setTapDelay(20); // 오타 방지를 위한 안정적 타이핑 속도
                        
                        String s = String(deckConfigs[index].stringVal);
                        s.replace("[#CAPS#]", ""); // 💡 핵심: 플래시 내 한영 전환 찌꺼기를 완전히 지움
                        
                        bleKeyboard.print(s);
                        
                        delay(1200); // Mac Spotlight가 주소를 완전히 파싱하고 '웹사이트' 검색 결과를 준비할 시간 부여
                        
                        // 3. 확실한 브라우저 실행 강제 (Cmd + Return)
                        // ❌ 기존 실패 원인: Enter 키코드 대신 '('의 아스키코드인 40을 쏘고 있었음. 
                        // ✅ 수정: 정확한 Enter 코드 사용 (ASCII 10)
                        bleKeyboard.press((uint8_t)227); // Cmd 누름
                        delay(150);                      // Cmd가 먼저 눌렸음을 확실히 함
                        bleKeyboard.press(KEY_RETURN);   // 확실한 물리적 Enter 키코드 상수(0xB0) 사용
                        delay(100);
                        bleKeyboard.releaseAll();
                        
                        bleKeyboard.setTapDelay(1); // 다시 초고속 모드로 복구
                    } else if (deckConfigs[index].mode == MODE_COMBO) {
                        // 콤보(단축키) 모드
                        if (deckConfigs[index].modifiers[0]) bleKeyboard.press((uint8_t)deckConfigs[index].modifiers[0]);
                        if (deckConfigs[index].modifiers[1]) bleKeyboard.press((uint8_t)deckConfigs[index].modifiers[1]);
                        if (deckConfigs[index].modifiers[2]) bleKeyboard.press((uint8_t)deckConfigs[index].modifiers[2]);
                        
                        if (deckConfigs[index].key) {
                            uint8_t hidCode = getHIDFromASCII(deckConfigs[index].key); 
                            if (hidCode != 0) bleKeyboard.press(hidCode);
                        }
                        delay(10); // 단축키 인식 안정성 한계치 (50 -> 10)
                        bleKeyboard.releaseAll();
                    }
                }
                active = true;
                feedbackEnd = now + 150;
                tone(BUZZER_PIN, 3000, 50);
            }
            else if (now - pressTime >= 1000) {
                Serial.print("[BTN "); Serial.print(index + 1); Serial.println("] Long Press");
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

    // 1. LittleFS 초기화
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] LittleFS Mount Failed");
    } else {
        loadConfig();
    }

    // 2. 하드웨어 초기화 (버튼, I2C)
    for (int i = 0; i < 4; i++) btns[i].init();
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    Wire.begin(HW_SDA_PIN, HW_SCL_PIN);
    Wire.setClock(400000);

    // 3. OLED 초기화
    u8g2_1.setI2CAddress(0x3C * 2); u8g2_1.begin();
    u8g2_2.setI2CAddress(0x3D * 2); u8g2_2.begin();
    u8g2_3.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;
    u8g2_3.begin(); 
    u8g2_4.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;
    u8g2_4.setI2CAddress(0x3D * 2); u8g2_4.begin();

    for(int i = 0; i < 4; i++) drawDefaultScreen(i);

    // 4. WiFi 연결 (WiFiManager)
    WiFiManager wm;
    wm.setConfigPortalTimeout(60); 
    if (wm.autoConnect("DS_DECK_SETUP")) {
        Serial.println("[NET] Connected to WiFi");
        Serial.println(WiFi.localIP());
        // WebServer 라우팅 설정
        initWebHandlers();
    }

    // 5. BLE 시작
    bleKeyboard.begin();
    bleKeyboard.setTapDelay(1); // 초고속 타이핑으로 상향 조정 (5ms -> 1ms)
    Serial.println("[BLE] Bluetooth Deck Ready.");
}

void loop() {
    for (int i = 0; i < 4; i++) btns[i].update();
    
    // WebServer 클라이언트 처리
    if (WiFi.status() == WL_CONNECTED) {
        server.handleClient();
    }
}
