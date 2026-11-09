// worynim@gmail.com
/**
 * @file Smart_Info_Station.ino
 * @brief 멀티 소스 정보를 OLED에 교차 표시하는 정보 스테이션 메인 펌웨어
 * @details 날씨, 증시, 코인 등 외부 API 데이터를 주기적으로 수집하여 4개의 OLED에 시네마틱하게 표시
 */
#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <Preferences.h>
#include <WebServer.h>
#include <DeskStreamCore.h>  // 화면 엔진·버튼·부저·WiFi·NTP 감독 (libraries/DeskStreamCore)

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


// === 공통 서비스 (DeskStreamCore) ===
// OLED 1·2(HW I2C)·3·4(SW I2C) — U8G2_R2, 바뀐 타일만 병렬 전송. 반전은 패널 명령(updateDisplayFlip)
ds::DisplayCore display;
static ds::Buttons buttons;
static ds::Buzzer buzzer;
static ds::WifiService wifi;
ds::TimeSync timeSync;               // web_handlers.cpp도 쓴다 (시간대 변경 시 재적용)

// 물리적 GPIO 핀 번호 설정
// 루프 상태 저장용 정적 변수
static bool prev_updating_states[MAX_WIDGET_RECORDS] = {false};
static unsigned long last_animation_tick[MAX_WIDGET_RECORDS] = {0};

// 시작 멜로디 (띠-리-리-링~ 도미솔도) — ds::Buzzer가 타이머로 비차단 재생한다
static const ds::Note STARTUP_MELODY[] = {{2093, 80, 30}, {2637, 80, 30}, {3136, 80, 30}, {4186, 220, 0}};

void playStartupMelody() {
  buzzer.playMelody(STARTUP_MELODY, sizeof(STARTUP_MELODY) / sizeof(STARTUP_MELODY[0]));
}

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
  display.setBrightness(contrast);
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
  // 좌우·상하 반전 조합 4종이라 ds::DisplayCore::setFlip(2종) 대신 패널 명령을 직접 보낸다
  for (int i = 0; i < ds::kNumScreens; i++) {
    display.screen(i).sendF("c", seg);
    display.screen(i).sendF("c", com);
  }
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
  if (num <= 0) return display.screen(0);
  int p = (num - 1) % 4; // 페이지와 무관하게 1~4번 중 하나로 매핑

  // Flip Mode 0, 3일 때는 화면 순서를 뒤집음 (4, 3, 2, 1), 1·2일 때는 기본 순서 (1, 2, 3, 4)
  if (currentFlipMode == 0 || currentFlipMode == 3) return display.screen(3 - p);
  return display.screen(p);
}


// 화면 전환 및 렌더링 함수 선언
void redraw_current_page();

int timezone_offset = 9;            // 시간대 설정 (기본값: 대한민국 UTC+9)
// NTP 서버 이름은 config.h의 NTP_SERVER1/NTP_SERVER2 매크로가 단일 정의처다
// (setup·web_handlers·time_sync가 같은 값을 쓰도록 — 여기서 중복 정의하지 않는다).

Preferences preferences;            // 설정을 플래시 메모리에 영구 저장하기 위한 객체
WebServer server(80);               // 설정용 웹 페이지를 제공하는 서버 객체 (80포트)


/**
 * 와이파이 설정 모드(Access Point)로 진입했을 때 OLED에 정보를 표시하는 콜백
 */
static void onWifiPortal(const char* apName) {
  U8G2 &main_screen = getScreen(1);
  main_screen.clearBuffer();
  main_screen.setFont(u8g2_font_6x10_tf);
  main_screen.drawStr(0, 10, "WiFi Config Mode");
  main_screen.drawStr(0, 25, "Connect to SSID:");
  main_screen.drawStr(0, 35, apName);
  main_screen.drawStr(0, 50, "IP: 192.168.4.1");
  display.push();
}

/**
 * NTP 감독이 적용할 시간대 — 이 판은 웹에서 **시간 단위 오프셋**(tz_offset, 예: 9)을 저장한다.
 * @details 예전 configTime(offset*3600, 0, ...)과 같은 동작을 POSIX TZ로 표현한다: "UTC-9" = UTC+9
 *          (POSIX는 부호가 반대). DST 규칙은 없다 — 저장 형식·웹 UI는 그대로다.
 */
static const char* currentTimezone() {
  static char tz[16];
  snprintf(tz, sizeof(tz), "UTC%d", -timezone_offset);
  return tz;
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
  
  // 잔상이 남지 않도록 모든 화면을 지우고 강제 전송
  display.clearAll();
  
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

// 버튼 — ds::Buttons (짧게 > 50ms: 50ms·3kHz 비프 후 동작 / 길게 > 1000ms: 200ms·2kHz 비프 후 동작)
static void s0() { buzzer.beep(50, 3000); btn1_short(); }
static void s1() { buzzer.beep(50, 3000); btn2_short(); }
static void s2() { buzzer.beep(50, 3000); btn3_short(); }
static void s3() { buzzer.beep(50, 3000); btn4_short(); }
static void l0() { buzzer.beep(200, 2000); btn1_long(); }
static void l1() { buzzer.beep(200, 2000); btn2_long(); }
static void l2() { buzzer.beep(200, 2000); btn3_long(); }
static void l3() { buzzer.beep(200, 2000); btn4_long(); }

// 4번째 페이지: 버튼 기능 안내 (Short/Long Press 설명)
void display_button_help_page() {
  for (int i = 1; i <= 4; i++) {
    // Flip Mode(역방향 매핑) 속성을 무시하고, 버튼 설정 가이드는 항상 기존 물리 순서(1,2,3,4)대로 표시
    U8G2 &u8g2 = display.screen(i - 1);
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
    
    display.push();
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
    
    display.push();
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
      display.push();
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

  // ESP32-C3 USB CDC(HWCDC): 호스트가 포트를 읽지 않아 TX 링(256B)이 차면
  // HWCDC::write()가 호출당 최대 2초(100ms × 20회)를 블로킹해 loop()가 멈춘다.
  // 버튼·부저 등 실시간 처리가 지연되지 않도록 TX 타임아웃을 0(비차단)으로.
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif

  // 1. 버튼 및 부저 초기화
  buttons.begin();
  buttons.setCallbacks(0, s0, l0);
  buttons.setCallbacks(1, s1, l1);
  buttons.setCallbacks(2, s2, l2);
  buttons.setCallbacks(3, s3, l3);
  buzzer.begin();

  // 2. 부팅 시 BTN1 눌림 감지 (WiFi 초기화 기능)
  bool should_reset_wifi = (digitalRead(ds::kButtonPins[0]) == LOW);

  // 3. 화면 엔진 초기화 (HW I2C 1.1MHz·고속 SW I2C·병렬 전송, 4화면 지움)
  display.begin();

  // 4. WiFi 초기화 실행 및 피드백
  if (should_reset_wifi) {
    buzzer.beep(500, 1000); // 길고 낮은 소리로 경고
    U8G2 &main_screen = getScreen(1);
    main_screen.setFont(u8g2_font_6x10_tf);
    main_screen.drawStr(0, 10, "!!! WiFi RESET !!!");
    main_screen.drawStr(0, 25, "Clearing SSID/PW...");
    main_screen.drawStr(0, 45, "Starting Config...");
    display.push();

    ds::WifiService::resetSettings(); // 저장된 세팅 삭제

    delay(500);
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
  display.push();

  // 저장된 AP 10초 비차단 시도 → 설정 포털(시간 제한 없음 — 예전 autoConnect와 같다)
  ds::WifiConfig wifiCfg;
  wifiCfg.apName = "Smart_Info_Station";
  wifiCfg.portalTimeoutS = 0;
  wifiCfg.restartOnFail = false;
  wifiCfg.onPortal = onWifiPortal;
  wifi.begin(wifiCfg);

  // 기본 설정 및 OLED 배치 정보 읽기
  preferences.begin("settings", true);
  youtube_channel = preferences.getString("channel", "@worynim");
  dust_location = preferences.getString("dust_loc", "가산동");
  weather_location_code = preferences.getString("weather_code", "1154551000");
  timezone_offset = preferences.getInt("tz_offset", 9);
  if (timezone_offset < -12 || timezone_offset > 14) timezone_offset = 9;   // 범위 밖 저장값 → 기본값 (UTC+9)
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

  // [2026-11-08] 시각 동기화 감독 시작 + 최초 동기화.
  //   시간대(오프셋) 적용·SNTP 시작은 timeSync.applyTimezone() 한 경로로만 일어난다
  //   (웹에서 시간대를 바꿀 때도 같은 경로 — web_handlers.cpp).
  ds::TimeSyncConfig timeCfg;
  timeCfg.servers[0] = NTP_SERVER1;
  timeCfg.servers[1] = NTP_SERVER2;
  timeCfg.servers[2] = nullptr;
  timeCfg.tzProvider = currentTimezone;
  timeSync.begin(timeCfg);
  timeSync.applyTimezone();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/set", HTTP_POST, handleSet);
  server.begin();

  // 데이터 수집 태스크 생성 (금융 1분, 기타 30분 주기 관리)
  xTaskCreate(dataTask, "DataTask", 8192, NULL, 1, &dataTaskHandle);
}

void loop() {
  server.handleClient();
  wifi.loop();       // WiFi 끊김 시 60초마다 재접속 시도
  timeSync.loop();   // NTP 재동기화 감독: 주기·WiFi 복구·실패 재시도
  unsigned long now_ms = millis();

  // [입력] 버튼 처리
  buttons.update();

  // [부저] 비차단 부저 처리는 ds::Buzzer의 FreeRTOS 타이머가 백그라운드에서 수행한다.

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
