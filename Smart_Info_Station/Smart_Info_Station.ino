#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <WebServer.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include "driver/gpio.h"  // ESP-IDF 고속 핀 제어 라이브러리 추가

// 데이터 수집 로직 포함
#include "dust.h"
#include "weather.h"
#include "youtube.h"
#include "market.h"
#include "btc.h"
#include "usdkrw.h"
#include "web_handlers.h"
#include "config.h"         // 전역 설정 및 핀 정의
#include "display_utils.h"  // 그리기 관련 헬퍼 함수 분리
#include "widgets_display.h" // 위젯 렌더링 함수 분리
#include "data_manager.h"   // 데이터 수집 및 관리 로직 분리


// OLED 1, 2 (Hardware I2C) -> 표시 갱신 속도가 중요한 시간/마켓 정보 배치에 유리
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_1(U8G2_R2, /* reset=*/U8X8_PIN_NONE);  // 1번 화면 객체
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_2(U8G2_R2, /* reset=*/U8X8_PIN_NONE);  // 2번 화면 객체

// OLED 3, 4 (Software I2C) -> 고정적인 정보 표시에 적합하며 배선 충돌을 방지함
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_3(U8G2_R2, /* clock=*/SW_SCL_PIN, /* data=*/SW_SDA_PIN, /* reset=*/U8X8_PIN_NONE);  // 3번 화면 객체
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_4(U8G2_R2, /* clock=*/SW_SCL_PIN, /* data=*/SW_SDA_PIN, /* reset=*/U8X8_PIN_NONE);  // 4번 화면 객체

// 물리적 GPIO 핀 번호 설정
// 루프 상태 저장용 정적 변수
static bool prev_updating_states[MAX_WIDGET_RECORDS] = {false};
static unsigned long last_animation_tick[MAX_WIDGET_RECORDS] = {0};

/**
 * @brief 패시브 부저음을 비차단(Non-Blocking) 방식으로 발생시킵니다.
 * @note  FreeRTOS 타이머를 사용하여 loop()의 블로킹(OLED 갱신 등)과 무관하게 정확한 시간에 종료합니다.
 */
static TimerHandle_t buzzerTimer = NULL;

void buzzerTimerCallback(TimerHandle_t xTimer) {
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW); // 마그네틱 부저: 잔류 전류 차단
}

void beep(int duration = 50, int freq = 3000) {
    if (buzzerTimer == NULL) return;
    tone(BUZZER_PIN, freq);
    xTimerChangePeriod(buzzerTimer, pdMS_TO_TICKS(duration), 0);
    xTimerStart(buzzerTimer, 0);
}

/**
 * @brief 시작 멜로디를 재생합니다. (띠리리링~)
 * @note  setup() 내에서만 호출. FreeRTOS 태스크 시작 전이므로 delay() 블로킹 허용.
 *        마그네틱 부저를 위해 각 음 이후 명시적 LOW 출력 필수.
 */

void playStartupMelody() {
  // 띠 - 리 - 리 - 링~ : 도미솔도 (경쾌한 상승 화음)
  const int notes[]     = { 2093, 2637, 3136, 4186 };  // Hz
  const int durations[] = {  80,  80,  80,  220 };  // ms (음 지속 시간)
  const int gaps[]      = {  30,  30,  30,    0 };  // ms (음간 묵음)

  for (int i = 0; i < 4; i++) {
    tone(BUZZER_PIN, notes[i]);
    delay(durations[i]);
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);  // 마그네틱 부저: 잔류 전류 차단
    if (gaps[i] > 0) delay(gaps[i]);
  }
}

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
        if (lastState == HIGH && currentState == LOW) { fallTime = now; isPressed = true; isLongPressFired = false; } 
        else if (currentState == LOW) {
            if (isPressed && !isLongPressFired && (now - fallTime > 1000)) {
                isLongPressFired = true;
                beep(200,2000); // 롱 프레스는 조금 더 길게
                if (onLongPress) onLongPress();
            }
        } 
        else if (lastState == LOW && currentState == HIGH) {
            if (isPressed && !isLongPressFired && (now - fallTime > 50)) { 
                beep(50,3000); // 숏 프레스는 짧게
                if (onShortPress) onShortPress(); 
            }
            isPressed = false;
        }
        lastState = currentState;
    }
};

int currentFlipMode = 2; // 0:Normal, 1:Mirror H, 2:180(HV), 3:Mirror V
int currentBrightnessLevel = 0; // (디폴트 1단계 설정)
bool isLoopingMode = false;     // 자동 페이지 전환 모드
unsigned long lastPageCycleTime = 0;
const unsigned long CYCLE_INTERVAL = 5000; 
int current_page = 1;

/**
 * @brief OLED의 밝기를 조절 (Contrast 설정)
 */
void updateBrightness() {
  uint8_t contrast;
  switch (currentBrightnessLevel) {
    case 0:  contrast = 1; break;  // 1단계
    case 1:  contrast = 50; break;  // 2단계
    case 2:  contrast = 100; break; // 3단계
    default: contrast = 240; break; // 4단계
  }
  u8g2_1.setContrast(contrast);
  u8g2_2.setContrast(contrast);
  u8g2_3.setContrast(contrast);
  u8g2_4.setContrast(contrast);
}

/**
 * @brief 화면 반전 설정을 하드웨어 레지스터에 즉시 적용
 */
void updateDisplayFlip() {
  uint8_t seg, com;
  switch (currentFlipMode) {
    case 1:  seg = 0xA1; com = 0xC0; break; 
    case 2:  seg = 0xA1; com = 0xC8; break; 
    case 3:  seg = 0xA0; com = 0xC8; break; 
    default: seg = 0xA0; com = 0xC0; break; 
  }
  u8g2_1.sendF("c", seg); u8g2_1.sendF("c", com);
  u8g2_2.sendF("c", seg); u8g2_2.sendF("c", com);
  u8g2_3.sendF("c", seg); u8g2_3.sendF("c", com);
  u8g2_4.sendF("c", seg); u8g2_4.sendF("c", com);
}

void saveGeneralSettings() {
  preferences.begin("settings", false);
  preferences.putInt("flip", currentFlipMode);
  preferences.putInt("bright", currentBrightnessLevel);
  preferences.putBool("loop", isLoopingMode);
  preferences.end();
}


/**
 * 📺 디스플레이 스크린 맵핑 (1~12번 슬롯)
 * 0~3번 인덱스: 1페이지의 화면 1~4
 * 4~7번 인덱스: 2페이지의 화면 1~4
 * 8~11번 인덱스: 3페이지의 화면 1~4
 */
WidgetType SCREEN_MAP[MAX_DATA_PAGE * 4] = {
  // 1페이지 설정 (기본 정보)
  W_TIME,     // 스크린 1
  W_WEATHER,  // 스크린 2
  W_DUST,     // 스크린 3
  W_YOUTUBE,  // 스크린 4

  // 2페이지 설정 (금융 지수)
  W_KOSPI,    // 스크린 5
  W_KOSDAQ,   // 스크린 6
  W_KPI200,   // 스크린 7
  W_FUTURES,   // 스크린 8

  // 3페이지 설정 (심화 정보 - 기본값)
  W_SNP500,   // 스크린 9
  W_NASDAQ,  // 스크린 10
  W_BTC,      // 스크린 11
  W_USDKRW    // 스크린 12
};

/**
 * 슬롯 번호에 대응하는 물리 OLED 객체를 반환합니다.
 * @param num 슬롯 번호 (1~4 또는 5~8)
 * @return U8G2 디스플레이 객체 참조
 */
U8G2 &getScreen(int num) {
  if (num <= 0) return u8g2_1;
  int p = (num - 1) % 4; // 페이지와 무관하게 1~4번 중 하나로 매핑

  // Flip Mode 0, 3일 때는 화면 순서를 뒤집음 (4, 3, 2, 1)
  if (currentFlipMode == 0 || currentFlipMode == 3) {
    if (p == 0) return u8g2_4;
    if (p == 1) return u8g2_3;
    if (p == 2) return u8g2_2;
    return u8g2_1;
  } else {
    // Flip Mode 1, 2일 때는 기본 순서 (1, 2, 3, 4)
    if (p == 0) return u8g2_1;
    if (p == 1) return u8g2_2;
    if (p == 2) return u8g2_3;
    return u8g2_4;
  }
}


// 화면 전환 및 렌더링 함수 선언
void redraw_current_page();

int timezone_offset = 9;            // 시간대 설정 (기본값: 대한민국 UTC+9)
const char* NTP_SERVER1 = "kr.pool.ntp.org"; // 주 NTP 서버
const char* NTP_SERVER2 = "time.nist.gov";   // 보조 NTP 서버

Preferences preferences;            // 설정을 플래시 메모리에 영구 저장하기 위한 객체
WebServer server(80);               // 설정용 웹 페이지를 제공하는 서버 객체 (80포트)


/**
 * 와이파이 설정 모드(Access Point)로 진입했을 때 OLED에 정보를 표시하는 콜백
 */
void configModeCallback(WiFiManager *myWiFiManager) {
  U8G2 &main_screen = getScreen(1);
  main_screen.clearBuffer();
  main_screen.setFont(u8g2_font_6x10_tf);
  main_screen.drawStr(0, 10, "WiFi Config Mode");
  main_screen.drawStr(0, 25, "Connect to SSID:");
  main_screen.drawStr(0, 35, myWiFiManager->getConfigPortalSSID().c_str());
  main_screen.drawStr(0, 50, "IP: 192.168.4.1");
  main_screen.sendBuffer();
}




// BTN 콜백 정의
void btn4_short() { 
  current_page = (current_page % MAX_PAGE) + 1;
  for (int w = 0; w < WIDGET_COUNT; w++) prev_updating_states[w] = false;
  redraw_current_page();
  Serial.printf("[BUTTON] BTN4 Short -> Page %d\n", current_page);
}
void btn4_long() {
  Serial.println("[BUTTON] BTN4 Long -> Refreshing data...");
  portENTER_CRITICAL(&updateMux);  // force_update 쓰기 보호
  force_update = true;
  portEXIT_CRITICAL(&updateMux);
  if (is_waiting && dataTaskHandle != NULL) xTaskAbortDelay(dataTaskHandle);
}

void btn1_short() {
  currentFlipMode = (currentFlipMode + 1) % 4;
  updateDisplayFlip();
  saveGeneralSettings();
  Serial.printf("[BUTTON] BTN1 Short -> Flip Mode %d\n", currentFlipMode);
  
  // 잔상이 남지 않도록 모든 화면의 버퍼를 초기화 후 전송
  u8g2_1.clearBuffer(); u8g2_1.sendBuffer();
  u8g2_2.clearBuffer(); u8g2_2.sendBuffer();
  u8g2_3.clearBuffer(); u8g2_3.sendBuffer();
  u8g2_4.clearBuffer(); u8g2_4.sendBuffer();
  
  // 바뀐 화면 순서 기반으로 현재 페이지 전체 재렌더링
  redraw_current_page();
}
void btn1_long() { /* Reserved */ }

void btn2_short() {
  currentBrightnessLevel = (currentBrightnessLevel + 1) % 4;
  updateBrightness();
  saveGeneralSettings();
  Serial.printf("[BUTTON] BTN2 Short -> Brightness %d%%\n", (currentBrightnessLevel + 1) * 25);
  if (current_page == 5) redraw_current_page();
}
void btn2_long() { /* Reserved */ }

void btn3_short() { /* Reserved */ }
void btn3_long() {
  isLoopingMode = !isLoopingMode;
  if (isLoopingMode) lastPageCycleTime = millis();
  saveGeneralSettings();
  Serial.printf("[BUTTON] BTN3 Long -> Looping Mode: %s\n", isLoopingMode ? "ON" : "OFF");
  
  // 상태 변경 시 즉시 피드백 (도움말 페이지 보고 있을 경우)
  if (current_page == 5) redraw_current_page();
}

// 버튼 객체 정의
Button btns[4] = {
  Button(BTN1_PIN, btn1_short, btn1_long),
  Button(BTN2_PIN, btn2_short, btn2_long),
  Button(BTN3_PIN, btn3_short, btn3_long),
  Button(BTN4_PIN, btn4_short, btn4_long)
};

// 4번째 페이지: 버튼 기능 안내 (Short/Long Press 설명)
void display_button_help_page() {
  for (int i = 1; i <= 4; i++) {
    // Flip Mode(역방향 매핑) 속성을 무시하고, 버튼 설정 가이드는 항상 기존 물리 순서(1,2,3,4)대로 표시
    U8G2 *p_u8g2;
    if (i == 1) p_u8g2 = &u8g2_1;
    else if (i == 2) p_u8g2 = &u8g2_2;
    else if (i == 3) p_u8g2 = &u8g2_3;
    else p_u8g2 = &u8g2_4;
    
    U8G2 &u8g2 = *p_u8g2;
    u8g2.clearBuffer();
    u8g2_prepare(u8g2);
    
    // 상단 제목
    u8g2.setFont(u8g2_font_6x10_tf);
    char title[16];
    snprintf(title, sizeof(title), "BTN %d SETTING", i);
    drawCenteredText(u8g2, title, 5);
    u8g2.drawLine(0, 18, 128, 18);

    // 상세 설명
    u8g2.setFont(u8g2_font_5x7_tr);
    if (i == 4) { // BTN 4 (Pin 9)
      u8g2.drawStr(5, 30, "SHORT: NEXT PAGE");
      u8g2.drawStr(5, 45, "LONG: REFRESH DATA");
    } else if (i == 1) { // BTN 1 (Pin 1)
      u8g2.drawStr(5, 30, "SHORT: SCREEN FLIP");
      const char* modeStr;
      switch (currentFlipMode) {
        case 1:  modeStr = "MIRROR H"; break;
        case 2:  modeStr = "180 (FLIP)"; break;
        case 3:  modeStr = "MIRROR V"; break;
        default: modeStr = "NORMAL"; break;
      }
      char modeInfo[20];
      snprintf(modeInfo, sizeof(modeInfo), "MODE: %s", modeStr);
      u8g2.drawStr(5, 45, modeInfo);
    } else if (i == 2) { // BTN 2 (Pin 4)
      u8g2.drawStr(5, 30, "SHORT: BRIGHTNESS");
      
      // 밝기 상태 비주얼 바 (사각형 표시)
      int boxSize = 8;
      int gap = 4;
      int totalWidth = (boxSize * 4) + (gap * 3);
      int startX = 5; // 왼쪽 정렬
      int startY = 45;
      for (int j = 0; j < 4; j++) {
        if (j <= currentBrightnessLevel) {
          u8g2.drawBox(startX + j * (boxSize + gap), startY, boxSize, boxSize); // 활성화된 칸
        } else {
          u8g2.drawFrame(startX + j * (boxSize + gap), startY, boxSize, boxSize); // 비활성화된 칸
        }
      }
    } else if (i == 3) { // BTN 3 (Pin 10)
      u8g2.drawStr(5, 30, "SHORT: - RESERVED -");
      u8g2.drawStr(5, 45, isLoopingMode ? "LONG: AUTO LOOP [ON]" : "LONG: AUTO LOOP [OFF]");
    } else {
      u8g2.drawStr(5, 30, "SHORT: - RESERVED -");
      u8g2.drawStr(5, 45, "LONG:  - RESERVED -");
    }
    
    u8g2.sendBuffer();
  }
}

// 5번째 페이지: 설정 웹서버 IP 주소를 4개의 OLED에 크게 분산 표시
void display_ip_page() {
  IPAddress ip = WiFi.localIP();
  for (int i = 1; i <= 4; i++) {
    U8G2 &u8g2 = getScreen(i);
    u8g2.clearBuffer();
    u8g2_prepare(u8g2);
    
    // 상단 제목 (첫 번째 화면에만 표시)
    if (i == 1) {
      u8g2.setFont(u8g2_font_6x10_tf);
      drawCenteredText(u8g2, "SETTING ADDR", 5);
    }
    
    // IP Octet (숫자 3개 분량) 크게 표시
    u8g2.setFont(u8g2_font_maniac_tr);
    String segment = String(ip[i-1]);
    drawCenteredText(u8g2, segment, 28);
    
    if (i < 4) {
      u8g2.setFont(MAIN_NUM_FONT);
      u8g2.drawStr(115, 28, ".");
    }
    
    u8g2.sendBuffer();
  }
}

void redraw_current_page() {
  // 페이지 전환 시 태스크만 깨워 가시성 변화 확인 (강제 수집 제외)
  if (dataTaskHandle != NULL && is_waiting) xTaskAbortDelay(dataTaskHandle);

  // 페이지 4: 시스템 정보 (IP 주소 표시)
  if (current_page == 4) {
    display_ip_page();
    return;
  }
  // 페이지 5: 버튼 기능 도움말 가이드
  if (current_page == 5) {
    display_button_help_page();
    return;
  }

  int start_idx = (current_page - 1) * 4;
  for (int i = start_idx; i < start_idx + 4; i++) {
    int slot_num = i + 1;
    WidgetType type = SCREEN_MAP[i];

    if (type == W_NONE) {
      U8G2 &u8g2 = getScreen(slot_num);
      u8g2.clearBuffer();
      u8g2.sendBuffer();
      continue;
    }

    // 시계 및 달력은 별도 처리 (widgets_info 테이블에 없음)
    if (type == W_TIME) {
      display_clock_oled(getScreen(slot_num));
      continue;
    }
    if (type == W_CALENDAR) {
      display_calendar_oled(getScreen(slot_num));
      continue;
    }

    // 테이블에서 매칭되는 위젯을 찾아 상태에 따라 표시
    for (int w = 0; w < WIDGET_COUNT; w++) {
      if (widgets_info[w].type == type) {
        if (*widgets_info[w].is_updating) {
          showUpdateMessage(getScreen(slot_num), String(widgets_info[w].update_msg) + "...");
        } else {
          widgets_info[w].render(getScreen(slot_num));
        }
        break;
      }
    }
  }
}


void setup() {
  Serial.begin(115200);

  // 1. 버튼 및 부저 핀 초기화
  for (int i = 0; i < 4; i++) btns[i].init();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  // 부저 종료용 소프트웨어 타이머 생성 (루프 블로킹 방지)
  buzzerTimer = xTimerCreate("BuzzerTimer", pdMS_TO_TICKS(50), pdFALSE, (void*)0, buzzerTimerCallback);

  // 2. 부팅 시 BTN1 눌림 감지 (WiFi 초기화 기능)
  bool should_reset_wifi = (digitalRead(BTN1_PIN) == LOW);

  // 3. 하드웨어 I2C 초기화 (핀 5, 6)
  Wire.begin(HW_SDA_PIN, HW_SCL_PIN);
  Wire.setClock(400000);

  // OLED 초기화 전 SW I2C 콜백 가로채기(Injection)
  u8g2_3.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;
  u8g2_4.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;

  // OLED 통신 시작
  u8g2_1.setI2CAddress(0x3C * 2); u8g2_1.begin();
  u8g2_2.setI2CAddress(0x3D * 2); u8g2_2.begin();
  u8g2_3.setI2CAddress(0x3C * 2); u8g2_3.begin();
  u8g2_4.setI2CAddress(0x3D * 2); u8g2_4.begin();

  // 모든 화면 초기 지우기
  u8g2_1.clearBuffer(); u8g2_1.sendBuffer();
  u8g2_2.clearBuffer(); u8g2_2.sendBuffer();
  u8g2_3.clearBuffer(); u8g2_3.sendBuffer();
  u8g2_4.clearBuffer(); u8g2_4.sendBuffer();

  // 4. WiFi 초기화 실행 및 피드백
  if (should_reset_wifi) {
    tone(BUZZER_PIN, 1000,500); // 길고 낮은 소리로 경고
    U8G2 &main_screen = getScreen(1);
    main_screen.setFont(u8g2_font_6x10_tf);
    main_screen.drawStr(0, 10, "!!! WiFi RESET !!!");
    main_screen.drawStr(0, 25, "Clearing SSID/PW...");
    main_screen.drawStr(0, 45, "Starting Config...");
    main_screen.sendBuffer();
    
    WiFiManager wifiManager;
    wifiManager.resetSettings(); // 저장된 세팅 삭제
    
    delay(500);
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    // 일반 부팅 시 시작 멜로디 재생 (띠리리링~)
    playStartupMelody();
  }

  // WiFi 정보 표시 (1번 화면)
  U8G2 &main_screen = getScreen(1);
  main_screen.clearBuffer();
  main_screen.setFont(u8g2_font_6x10_tf);
  main_screen.drawStr(0, 10, "WiFi Connecting...");
  main_screen.drawStr(0, 25, "SSID: Smart_Info_Station");
  main_screen.drawStr(0, 35, "IP: 192.168.4.1");
  main_screen.sendBuffer();

  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.autoConnect("Smart_Info_Station");

  // 기본 설정 및 OLED 배치 정보 읽기
  preferences.begin("settings", true);
  youtube_channel = preferences.getString("channel", "@worynim");
  dust_location = preferences.getString("dust_loc", "가산동");
  weather_location_code = preferences.getString("weather_code", "1154551000");
  timezone_offset = preferences.getInt("tz_offset", 9);
  currentFlipMode = preferences.getInt("flip", 2);
  currentBrightnessLevel = preferences.getInt("bright", 0);
  isLoopingMode = preferences.getBool("loop", false);
  preferences.end();

  // 저장된 반전 모드 및 밝기 즉시 적용
  updateDisplayFlip();
  updateBrightness();

  preferences.begin("screen_map", true);
  for (int i = 0; i < MAX_DATA_PAGE * 4; i++) {
    String key = "s" + String(i);
    SCREEN_MAP[i] = (WidgetType)preferences.getInt(key.c_str(), SCREEN_MAP[i]);
  }
  preferences.end();

  configTime(timezone_offset * 3600, 0, "kr.pool.ntp.org", "time.nist.gov");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/set", HTTP_POST, handleSet);
  server.begin();

  // 데이터 수집 태스크 생성 (금융 1분, 기타 30분 주기 관리)
  xTaskCreate(dataTask, "DataTask", 8192, NULL, 1, &dataTaskHandle);
}

void loop() {
  server.handleClient();
  unsigned long now_ms = millis();

  // [입력] 버튼 처리
  for (int i = 0; i < 4; i++) btns[i].update();

  // [부저] 비차단 부저 처리는 이제 FreeRTOS 타이머가 백그라운드에서 수행합니다.

  // [루핑 모드] 페이지 자동 전환 처리 (1~3페이지 순환)
  if (isLoopingMode) {
    bool is_current_page_loading = false;
    if (current_page < 4) {
      int start_idx = (current_page - 1) * 4;
      for (int i = start_idx; i < start_idx + 4; i++) {
        WidgetType type = SCREEN_MAP[i];
        if (type == W_NONE || type == W_TIME || type == W_CALENDAR) continue;
        for (int w = 0; w < WIDGET_COUNT; w++) {
          if (widgets_info[w].type == type && *widgets_info[w].is_updating) {
            is_current_page_loading = true;
            break;
          }
        }
        if (is_current_page_loading) break;
      }
    }

    if (is_current_page_loading) {
      // 현재 페이지의 데이터가 로딩 중이면 전환 타이머 초기화 (지연)
      lastPageCycleTime = now_ms;
    } else if (now_ms - lastPageCycleTime >= CYCLE_INTERVAL) {
      if (current_page < 4) { // 현재 데이터 페이지를 보고 있을 때만 자동 전환
        current_page = (current_page % 3) + 1;
        redraw_current_page();
      }
      lastPageCycleTime = now_ms;
    }
  }

  // [표시 1] 시계 전용 업데이트 (1초마다 무조건 갱신, 3페이지 제외)
  static unsigned long last_clock_ms = 0;
  if (now_ms - last_clock_ms >= 1000) {
    if (current_page <= 3) {
      int start_idx = (current_page - 1) * 4;
      for (int i = start_idx; i < start_idx + 4; i++) {
         if (SCREEN_MAP[i] == W_TIME) display_clock_oled(getScreen(i + 1));
         if (SCREEN_MAP[i] == W_CALENDAR) display_calendar_oled(getScreen(i + 1));
      }
    }
    last_clock_ms = now_ms;
  }

  // [표시 2] 데이터 위젯 통합 상태 엔진 (1, 2, 3페지만 해당)
  if (current_page <= 3) {
    int start_idx = (current_page - 1) * 4;
    for (int w = 0; w < WIDGET_COUNT; w++) {
    const WidgetUpdateInfo &info = widgets_info[w];

    if (*info.is_updating) {
      // 데이터 수집 중일 때: 로딩 메시지 또는 애니메이션 표시
      if (info.animate) {
        if (now_ms - last_animation_tick[w] > 500) {
          int dots = (now_ms / 500) % 4;
          String dMsg = info.update_msg;
          for (int d = 0; d < dots; d++) dMsg += ".";
          for (int i = start_idx; i < start_idx + 4; i++) {
            if (SCREEN_MAP[i] == info.type) showUpdateMessage(getScreen(i + 1), dMsg);
          }
          last_animation_tick[w] = now_ms;
        }
      } else if (!prev_updating_states[w]) {
        for (int i = start_idx; i < start_idx + 4; i++) {
          if (SCREEN_MAP[i] == info.type) showUpdateMessage(getScreen(i + 1), String(info.update_msg) + "...");
        }
      }
      prev_updating_states[w] = true;
    } else if (update_flag & info.flag_mask) {
      // 데이터 수집 완료됨: 즉시 해당 화면들 렌더링
      for (int i = start_idx; i < start_idx + 4; i++) {
        if (SCREEN_MAP[i] == info.type) info.render(getScreen(i + 1));
      }
      portENTER_CRITICAL(&updateMux);
      update_flag &= ~info.flag_mask;
      portEXIT_CRITICAL(&updateMux);
      prev_updating_states[w] = false;
    }
    }
  }

  vTaskDelay(1); // 시스템 안정성을 위해 최소 딜레이 유지
}
