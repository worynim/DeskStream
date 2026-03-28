#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <WebServer.h>
#include <Wire.h>
#include "driver/gpio.h"  // ESP-IDF 고속 핀 제어 라이브러리 추가

// 데이터 수집 로직 포함
#include "dust.h"
#include "weather.h"
#include "youtube.h"
#include "market.h"
#include "btc.h"
#include "usdkrw.h"
#include "web_index.h"

// --- 설정 상수 ---
// --- 설정 상수 ---
#define MAX_WIDGET_TYPE 13        // 등록 가능한 위젯 종류의 최대값 (W_CALENDAR 포함)
#define MAX_PAGE 4                // OLED 디바이스에서 전환 가능한 총 페이지 수 (1, 2, 3, 4페이지)

/**
 * OLED 핀 설정 (하이브리드 I2C 버스 구성)
 * Hardware I2C: ESP32-C3 전용 하드웨어 컨트롤러 사용 (속도 중시)
 * Software I2C: 비트 뱅잉 방식으로 독립적인 통신 채널 확보 (안정성 중시)
 */
const uint8_t hw_sda_pin = 5;     // 하드웨어 I2C용 SDA 핀
const uint8_t hw_scl_pin = 6;     // 하드웨어 I2C용 SCL 핀
const uint8_t sw_sda_pin = 2;     // 소프트웨어 I2C용 SDA 핀
const uint8_t sw_scl_pin = 3;     // 소프트웨어 I2C용 SCL 핀

// OLED 1, 2 (Hardware I2C) -> 표시 갱신 속도가 중요한 시간/마켓 정보 배치에 유리
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_1(U8G2_R0, /* reset=*/U8X8_PIN_NONE);  // 1번 화면 객체
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);  // 2번 화면 객체

// OLED 3, 4 (Software I2C) -> 고정적인 정보 표시에 적합하며 배선 충돌을 방지함
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_3(U8G2_R0, /* clock=*/sw_scl_pin, /* data=*/sw_sda_pin, /* reset=*/U8X8_PIN_NONE);  // 3번 화면 객체
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_4(U8G2_R0, /* clock=*/sw_scl_pin, /* data=*/sw_sda_pin, /* reset=*/U8X8_PIN_NONE);  // 4번 화면 객체

// 물리적 GPIO 핀 번호 설정
const int BUTTON_PIN = 9;         // 페이지 전환 및 리셋용 푸시 버튼 핀

// 페이지 및 버튼 인터랙션 제어 변수
int current_page = 1;                         // 현재 표시 중인 페이지 번호 (1~MAX_PAGE)
bool button_state = HIGH;                     // 현재 버튼의 논리적 상태
bool last_button_reading = HIGH;              // 직전 루프에서 읽은 버튼의 물리적 값
unsigned long last_debounce_time = 0;         // 버튼 디바운싱을 위한 마지막 시간 기록
unsigned long button_press_start_time = 0;    // 버튼이 눌리기 시작한 시각 (millis)
bool is_long_press_triggered = false;         // 길게 누름(Long Press)이 이미 실행되었는지 여부
const unsigned long LONG_PRESS_TIME = 1000;   // 길게 누름을 판정하는 Threshold (1초)

/**
 * 📺 디스플레이 표시 위젯 타입 열거형
 * 각 슬롯에 배치될 위젯의 종류를 정의합니다.
 */
enum WidgetType {
  W_NONE = 0,    // 표시 안 함
  W_TIME,        // 현재 시각/날짜
  W_WEATHER,     // 기상청 단기 예보
  W_DUST,        // 네이버 실시간 미세먼지
  W_YOUTUBE,     // 유튜브 채널 구독자 수
  W_KOSPI,       // 코스피 지수
  W_KOSDAQ,      // 코스닥 지수
  W_KPI200,      // 코스피 200 지수
  W_FUTURES,     // 코스피 200 선물지수
  W_SNP500,      // S&P 500 지수
  W_NASDAQ,      // 나스닥 지수
  W_BTC,         // 비트코인 시세 (Binance)
  W_USDKRW,      // 원/달러 환율 (Yahoo)
  W_CALENDAR     // 달력 (격자형 캘린더)
};

/**
 * 📺 디스플레이 스크린 맵핑 (1~12번 슬롯)
 * 0~3번 인덱스: 1페이지의 화면 1~4
 * 4~7번 인덱스: 2페이지의 화면 1~4
 * 8~11번 인덱스: 3페이지의 화면 1~4
 */
WidgetType SCREEN_MAP[12] = {
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
  if (p == 0) return u8g2_1;
  if (p == 1) return u8g2_2;
  if (p == 2) return u8g2_3;
  return u8g2_4;
}

/**
 * 특정 위젯이 스크린 맵(1, 2페이지 전체) 중 하나라도 등록되어 있는지 확인합니다.
 * 백그라운드 데이터 수집 여부를 결정할 때 사용됩니다.
 * @param type 확인할 위젯 종류
 * @return 등록 여부 (true/false)
 */
bool isWidgetActive(WidgetType type) {
  for (int i = 0; i < 12; i++) {
    if (SCREEN_MAP[i] == type) return true;
  }
  return false;
}

/**
 * 특정 위젯이 사용자에게 실제로 보여지고 있는 현재 페이지에 있는지 확인합니다.
 * @param type 확인할 위젯 종류
 * @return 가시성 여부 (true/false)
 */
bool isWidgetVisible(WidgetType type) {
  if (current_page == 4) return false; // 4페이지는 IP 표시용 시스템 고정 페이지
  int start_idx = (current_page - 1) * 4;
  for (int i = start_idx; i < start_idx + 4; i++) {
    if (SCREEN_MAP[i] == type) return true;
  }
  return false;
}

// 화면 전환 및 렌더링 함수 선언
void redraw_current_page();

int timezone_offset = 9;            // 시간대 설정 (기본값: 대한민국 UTC+9)
const char* NTP_SERVER1 = "kr.pool.ntp.org"; // 주 NTP 서버
const char* NTP_SERVER2 = "time.nist.gov";   // 보조 NTP 서버

Preferences preferences;            // 설정을 플래시 메모리에 영구 저장하기 위한 객체
WebServer server(80);               // 설정용 웹 페이지를 제공하는 서버 객체 (80포트)

// 업데이트 관리 플래그 (비트 마스킹으로 관리)
volatile uint16_t update_flag = 0x00;
portMUX_TYPE updateMux = portMUX_INITIALIZER_UNLOCKED;  // update_flag 동기화용 뮤텍스
volatile bool is_updating_dust = false;
volatile bool is_updating_weather = false;
volatile bool is_updating_youtube = false;
volatile bool is_updating_kospi = false;
volatile bool is_updating_kosdaq = false;
volatile bool is_updating_SnP500 = false;
volatile bool is_updating_btc = false;
volatile bool is_updating_usdkrw = false;
volatile bool is_updating_nasdaq = false;
volatile bool is_updating_futures = false;
volatile bool is_updating_kpi200 = false;
volatile bool force_update = false;  // 웹 설정 즉시 갱신 트리거 플래그
volatile bool is_waiting = false;    // 데이터 수집 태스크 대기 상태 여부
TaskHandle_t dataTaskHandle = NULL;  // 데이터 수집 태스크 핸들

// --- 위젯 상태 관리용 구조체 및 설정 (글로벌 정의) ---
typedef void (*RenderFn)(U8G2 &);
typedef void (*FetchFn)();

// 후방 정의된 함수들을 위한 프로토타입 선언
void display_kospi_oled(U8G2 &u8g2);
void display_kosdaq_oled(U8G2 &u8g2);
void display_SnP500_oled(U8G2 &u8g2);
void display_btc_oled(U8G2 &u8g2);
void display_usdkrw_oled(U8G2 &u8g2);
void display_nasdaq_oled(U8G2 &u8g2);
void display_futures_oled(U8G2 &u8g2);
void display_kpi200_oled(U8G2 &u8g2);
void display_weather_oled(U8G2 &u8g2);
void display_dust_oled(U8G2 &u8g2);
void display_youtube_oled(U8G2 &u8g2);

struct WidgetUpdateInfo {
  WidgetType type;
  volatile bool *is_updating;
  uint16_t flag_mask;
  const char *update_msg;
  RenderFn render;
  FetchFn fetch;
  bool is_finance;
  bool animate;
};

// 위젯별 매핑 테이블 (데이터 그룹에 따른 플래그 비트와 표시/수집 함수 연결)
static const WidgetUpdateInfo widgets_info[] = {
  { W_KOSPI,   &is_updating_kospi,   0x08, "Updating KOSPI",   display_kospi_oled,   get_kospi,     true,  false },
  { W_KOSDAQ,  &is_updating_kosdaq,  0x10, "Updating KOSDAQ",  display_kosdaq_oled,  get_kosdaq,    true,  false },
  { W_KPI200,  &is_updating_kpi200,  0x400, "Updating KOSPI200", display_kpi200_oled, get_kpi200,    true,  false },
  { W_FUTURES, &is_updating_futures, 0x200, "Updating Futures", display_futures_oled, get_futures,   true,  false },
  { W_SNP500,  &is_updating_SnP500,  0x20, "Updating S&P 500", display_SnP500_oled,  get_SnP500,    true,  false },
  { W_NASDAQ,  &is_updating_nasdaq,  0x100, "Updating NASDAQ",  display_nasdaq_oled,  get_nasdaq,    true,  false },
  { W_BTC,     &is_updating_btc,     0x40, "Updating BTC",     display_btc_oled,     get_btc,       true,  false },
  { W_USDKRW,  &is_updating_usdkrw,  0x80, "Updating USD/KRW", display_usdkrw_oled,  get_usdkrw,    true,  false },
  { W_WEATHER, &is_updating_weather, 0x02, "Updating Weather", display_weather_oled, get_weather,   false, false },
  { W_DUST,    &is_updating_dust,    0x01, "Updating Dust",    display_dust_oled,    get_dust,      false, true  },
  { W_YOUTUBE, &is_updating_youtube, 0x04, "Updating YouTube", display_youtube_oled, get_subscribe, false, true  }
};
static const int WIDGET_COUNT = sizeof(widgets_info) / sizeof(widgets_info[0]);

// --- 업데이트 주기 설정 (밀리초 단위) ---
const unsigned long INTERVAL_FINANCE = 60 * 1000;    // 금융 데이터: 1분
const unsigned long INTERVAL_SLOW = 30 * 60 * 1000;  // 날씨/미세먼지/유튜브: 30분

// 마지막 업데이트 시각 기록용 변수 (millis 기반)
unsigned long last_finance_update = 0;  // 금융 지수 마지막 갱신 시간
unsigned long last_slow_update = 0;     // 날씨/먼지/유튜브 마지막 갱신 시간

/**
 * 국내 증시(코스피/코스닥) 업데이트가 필요한 유효 시간인지 확인합니다.
 * @return 업데이트 가능 여부 (평일 08:00 ~ 18:00 이면 true)
 */
bool isDomesticMarketOpen() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return true; // 시간 동기화 전이면 일단 허용
  
  int wday = timeinfo.tm_wday;   // 0=일, 1=월, ..., 6=토
  int hour = timeinfo.tm_hour;   // 0~23 (KST 기준)
  
  // 평일(월~금)이고 오전 8시부터 오후 6시(18시 미만) 사이인지 확인
  if (wday >= 1 && wday <= 5 && hour >= 8 && hour < 18) {
    return true;
  }
  return false;
}

/**
 * 미국 증시(S&P 500, NASDAQ) 업데이트가 필요한 유효 시간인지 확인합니다.
 * 시차 고려: 한국 시간 기준 월요일 17시 ~ 토요일 오전 9시까지 활성화
 * @return 업데이트 가능 여부 (true/false)
 */
bool isUsMarketOpen() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return true; // 시간 동기화 전이면 일단 허용
  
  int wday = timeinfo.tm_wday;   // 0=일, 1=월, ..., 6=토
  int hour = timeinfo.tm_hour;   // 0~23 (KST 기준)
  
  // 1. 주말 체크 (한국 시간 토요일 09시 이후 ~ 월요일 17시 이전은 휴장)
  if (wday == 6) { // 토요일
    if (hour >= 9) return false; 
  }
  if (wday == 0) return false;   // 일요일
  if (wday == 1) { // 월요일
    if (hour < 17) return false;
  }

  // 2. 시간 범위 체크 (평일 중 17:00 ~ 익일 09:00 사이인지 확인)
  if (hour >= 17 || hour < 9) {
    return true;
  }
  return false;
}

/**
 * 문자열에 한글(UTF-8) 문자가 포함되어 있는지 확인합니다.
 * @param s 확인할 문자열
 * @return 한글 포함 여부 (true/false)
 */
bool containsKorean(String s) {
  for (int i = 0; i < s.length(); i++) {
    byte c = s.charAt(i);
    if ((c & 0xF0) == 0xE0) return true; // UTF-8 한글 패턴 감지
  }
  return false;
}

/**
 * OLED 화면의 중앙에 텍스트를 출력합니다.
 * @param u8g2 대상 디스플레이 객체
 * @param text 출력할 문자열
 * @param y 출력할 Y 좌표
 */
void drawCenteredText(U8G2 &u8g2, String text, int y) {
  const char *str = text.c_str();
  int textWidth = u8g2.getUTF8Width(str);
  int displayWidth = u8g2.getDisplayWidth();
  int x = (displayWidth - textWidth) / 2;
  if (x < 0) x = 0;

  if (containsKorean(text)) u8g2.drawUTF8(x, y + 4, str); // 한글 전용 처리
  else u8g2.drawStr(x, y, str); // 일반 영문/숫자 처리
}

/**
 * U8g2 그래픽 그리기를 위한 공통 설정을 초기화합니다.
 * @param u8g2 대상 디스플레이 객체
 */
void u8g2_prepare(U8G2 &u8g2) {
  u8g2.setFont(u8g2_font_6x10_tf);      // 기본 폰트 설정
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);                 // 흰색 드로잉
  u8g2.setFontPosTop();                 // 좌표 기준을 상단으로 설정
  u8g2.setFontDirection(0);             // 가로 방향
}

/**
 * 업데이트 중일 때 OLED 화면에 안내 메시지를 표시합니다.
 * @param u8g2 대상 디스플레이 객체
 * @param msg 표시할 메시지
 */
void showUpdateMessage(U8G2 &u8g2, String msg) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, msg.c_str());
  u8g2.sendBuffer();
}

/**
 * 🚀 극한의 고속 SW I2C 커스텀 콜백 (Direct Register Access)
 * U8g2의 기본 아두이노 오버헤드를 우회하여 ESP32-C3의 GPIO 리지스터를 직접 제어합니다.
 * 통신 속도를 극대화하여 SW I2C 환경에서도 부드러운 화면 갱신을 가능하게 합니다.
 */
extern "C" uint8_t u8x8_gpio_and_delay_esp32_c3_fast(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  uint8_t pin;
  switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT: // 핀 초기화
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
    case U8X8_MSG_DELAY_MILLI: // ms 딜레이
      delay(arg_int);
      break;
    case U8X8_MSG_DELAY_10MICRO: // 10us 딜레이 (1us로 단축하여 가속)
      delayMicroseconds(1);
      break;
    case U8X8_MSG_DELAY_100NANO: // 100ns 딜레이 (코드 주석 수준으로 가속)
      __asm__ __volatile__("nop;nop;nop;nop;");
      break;
    case U8X8_MSG_GPIO_I2C_CLOCK: // CLOCK 핀 제어
      pin = u8x8->pins[U8X8_PIN_I2C_CLOCK];
      if (pin != U8X8_PIN_NONE) gpio_set_level((gpio_num_t)pin, arg_int ? 1 : 0);
      break;
    case U8X8_MSG_GPIO_I2C_DATA: // DATA 핀 제어
      pin = u8x8->pins[U8X8_PIN_I2C_DATA];
      if (pin != U8X8_PIN_NONE) gpio_set_level((gpio_num_t)pin, arg_int ? 1 : 0);
      break;
    default:
      u8x8_SetGPIOResult(u8x8, 1);
      break;
  }
  return 1;
}

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

/**
 * --- 🚀 백그라운드 데이터 수집 태스크 (FreeRTOS Task) ---
 * 1. 부팅 시 모든 금융 데이터를 미리 수집하여 '0.00' 공백을 방지함.
 * 2. 현재 화면에 보이는 위젯을 최우선적으로 순차 갱신함.
 * 3. 보이지 않는 배경 위젯들을 백그라운드에서 순차적으로 갱신함.
 */
void dataTask(void *pvParameters) {
  for (;;) {
    unsigned long now = millis();

    if (WiFi.status() == WL_CONNECTED) {
      // 갱신 주기 판단
      bool is_initial = (last_finance_update == 0);
      bool is_finance_interval = (now - last_finance_update >= INTERVAL_FINANCE) || force_update || is_initial;
      bool is_slow_interval = (now - last_slow_update >= INTERVAL_SLOW) || force_update || (last_slow_update == 0);

      uint16_t processed_mask = 0;

      // [1] 최초 전원 인가 시: 모든 활성 금융 지수를 백그라운드에서 미리 수집 (Quiet Fetch)
      if (is_initial) {
        Serial.println("[TASK] Initial Quiet Fetch for all active Finance widgets...");
        for (int w = 0; w < WIDGET_COUNT; w++) {
          if (widgets_info[w].is_finance && isWidgetActive(widgets_info[w].type)) {
            widgets_info[w].fetch();
            portENTER_CRITICAL(&updateMux);
            update_flag |= widgets_info[w].flag_mask;
            portEXIT_CRITICAL(&updateMux);
          }
        }
      }

      // [2] 순차 업데이트 가시성 스캔 (OLED 1 -> 4 순서)
      int start_idx = (current_page - 1) * 4;
      if (current_page != 4) {
        for (int i = start_idx; i < start_idx + 4; i++) {
          WidgetType type = SCREEN_MAP[i];
          if (type == W_NONE || type == W_TIME || type == W_CALENDAR) continue;

          for (int w = 0; w < WIDGET_COUNT; w++) {
            if (widgets_info[w].type == type) {
              if (processed_mask & widgets_info[w].flag_mask) break;

              bool needs_update = false;
              if (widgets_info[w].is_finance) {
                if (is_finance_interval) needs_update = true;
              } else {
                if (is_slow_interval) needs_update = true;
              }

              if (needs_update) {
                // 국내 증시(코스피, 코스닥, 코스피200)의 경우 평일 운영시간(08~18) 외에는 실시간 갱신 스킵
                if ((type == W_KOSPI || type == W_KOSDAQ || type == W_KPI200) && !isDomesticMarketOpen()) {
                  needs_update = false; 
                }
                // 미국 증시(S&P 500, NASDAQ)의 경우 17시~익일 09시 외에는 실시간 갱신 스킵
                if ((type == W_SNP500 || type == W_NASDAQ) && !isUsMarketOpen()) {
                  needs_update = false;
                }
              }

              if (needs_update) {
                *widgets_info[w].is_updating = true;
                widgets_info[w].fetch();
                *widgets_info[w].is_updating = false;
                portENTER_CRITICAL(&updateMux);
                update_flag |= widgets_info[w].flag_mask;
                portEXIT_CRITICAL(&updateMux);
                processed_mask |= widgets_info[w].flag_mask;
              }
              break;
            }
          }
        }
      }

      if (is_finance_interval) last_finance_update = now;
      if (is_slow_interval) last_slow_update = now;
    }

    force_update = false; 
    is_waiting = true;
    vTaskDelay(pdMS_TO_TICKS(1000));
    is_waiting = false;
  }
}

// --- 개별 OLED 표시 함수 ---

void display_clock_oled(U8G2 &u8g2) {
  u8g2_prepare(u8g2);
  time_t now = time(nullptr);
  String t_str = String(ctime(&now));
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_bytesize_tr);
  String dateLine = t_str.substring(20, 24) + " " + t_str.substring(4, 10) + " " + t_str.substring(0, 3);
  drawCenteredText(u8g2, dateLine, 5);
  u8g2.setFont(u8g2_font_maniac_tr);
  drawCenteredText(u8g2, t_str.substring(11, 19), 25);

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(10, 56, "Set: ");
  u8g2.drawStr(35, 56, WiFi.localIP().toString().c_str());
  u8g2.sendBuffer();
}

/**
 * 달력을 표시하는 격자형 캘린더 위젯
 * GPS_DASH 프로젝트의 drawCalendar 로직과 100% 동일한 좌표를 유지하여 구현함.
 * @param u8g2 대상 디스플레이 객체
 */
/**
 * 달력을 표시하는 격자형 캘린더 위젯
 * 글자 겹침을 방지하고 6행(마지막 주)까지 OLED(64px)에 표시되도록 최적화된 좌표 적용
 * @param u8g2 대상 디스플레이 객체
 */
void display_calendar_oled(U8G2 &u8g2) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  int year = timeinfo.tm_year + 1900;
  int month = timeinfo.tm_mon + 1;
  int day = timeinfo.tm_mday;

  u8g2_prepare(u8g2);
  // GPS_DASH는 기본 Baseline 기준을 사용하므로, 이 화면에만 GPS_DASH와 동일한 좌표 기준을 적용합니다.
  u8g2.setFontPosBaseline(); 
  u8g2.clearBuffer();
  
  // 최상단 Header 날짜 (예: 2026.03)
  char buf[16];
  snprintf(buf, sizeof(buf), "%d.%02d", year, month);

  // GPS_DASH의 drawCenteredText(4인자)와 완전히 동일한 동작을 로컬에서 수행
  u8g2.setFont(u8g2_font_bpixeldouble_tr);
  int textWidth = u8g2.getUTF8Width(buf);
  int headerX = (128 - textWidth) / 2;
  if (headerX < 0) headerX = 0;
  u8g2.drawUTF8(headerX, 14, buf);
  
  // 해당 월의 1일 요일 계산 (Sakamoto Algorithm)
  static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
  int y_calc = year;
  if (month < 3) y_calc -= 1;
  int firstDayOfWeek = ( y_calc + y_calc/4 - y_calc/100 + y_calc/400 + t[month-1] + 1 ) % 7; // 0=Sun, 6=Sat
  
  // 해당 월의 총 일수 자동 계산
  int daysInMonth = 31;
  if (month == 4 || month == 6 || month == 9 || month == 11) daysInMonth = 30;
  else if (month == 2) daysInMonth = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
  
  // 요일 헤더 그리기 (S M T W T F S)
  u8g2.setFont(u8g2_font_5x7_tr);
  const char* wdays[] = {"S", "M", "T", "W", "T", "F", "S"};
  int colW = 18; // 한 열(Day)이 차지하는 가로 너비
  int xOffset = 1;
  
  for (int i = 0; i < 7; i++) {
      int x = xOffset + i * colW + (colW - u8g2.getUTF8Width(wdays[i])) / 2;
      u8g2.drawStr(x, 22, wdays[i]);
  }
  
  u8g2.drawLine(0, 23, 128, 23); // 요일과 날짜 경계선
  
  // 일자 데이터 그리기 (그리드 레이아웃)
  int currentDay = 1;
  for (int row = 0; row < 6; row++) {
      for (int col = 0; col < 7; col++) {
          if (row == 0 && col < firstDayOfWeek) continue;
          if (currentDay > daysInMonth) break;
          
          snprintf(buf, sizeof(buf), "%d", currentDay);
          int dw = u8g2.getUTF8Width(buf);
          int cx = xOffset + col * colW + colW / 2; // 칸의 정중앙 x좌표
          int dx = cx - dw / 2; // 텍스트 렌더링 시작 좌표
          int dy = 31 + row * 8; // 날짜 행(Row)간 세로 간격(8px)
          
          if (currentDay == day) { // 오늘 날짜면 반전 하이라이트 박스 생성
              u8g2.setDrawColor(1); // 박스는 하얗게
              u8g2.drawBox(cx - 8, dy - 7, 16, 8); 
              u8g2.setDrawColor(0); // 글자는 까맣게(투명하게 파내기)
              u8g2.drawStr(dx, dy, buf);
              u8g2.setDrawColor(1); // 원래대로 복구
          } else {
              u8g2.drawStr(dx, dy, buf);
          }
          currentDay++;
      }
      if (currentDay > daysInMonth) break;
  }
  u8g2.sendBuffer();
}


void display_weather_oled(U8G2 &u8g2) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 0, ("Ann.Time: " + announcement_time.substring(0, 5) + " " + announcement_time.substring(announcement_time.length() - 5)).c_str());
  for (int i = 0; i < 3; i++) {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(43 * i, 13, (String(forecast[i].time) + ":00").c_str());
    if (forecast[i].type == 1) u8g2.drawStr(43 * i, 23, "Clear");
    else if (forecast[i].type == 2) u8g2.drawStr(43 * i, 23, "Partly");
    else if (forecast[i].type == 3) u8g2.drawStr(43 * i, 23, "Mostly");
    else if (forecast[i].type == 4) u8g2.drawStr(43 * i, 23, "Cloudy");
    else if (forecast[i].type == 5) u8g2.drawStr(43 * i, 23, "Rain");
    else if (forecast[i].type <= 9) u8g2.drawStr(43 * i, 23, "Shower");
    else u8g2.drawStr(43 * i, 23, "Unknown");
    u8g2.setFont(u8g2_font_open_iconic_weather_2x_t);
    int icon = 68;
    if (forecast[i].type == 1) icon = 69;
    else if (forecast[i].type <= 3) icon = 65;
    else if (forecast[i].type == 4) icon = 64;
    else if (forecast[i].type >= 5) icon = 67;
    u8g2.drawGlyph(43 * i + 2, 33, icon);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(43 * i, 53);
    u8g2.print(forecast[i].temp);
    u8g2.write(0xB0);
    u8g2.print("C");
  }
  u8g2.sendBuffer();
}

void display_dust_oled(U8G2 &u8g2) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 0, "Air pollution ");
  u8g2.drawStr(0, 18, "PM10: ");
  u8g2.drawStr(0, 43, "PM2.5: ");
  if (dust_10_num <= 30) u8g2.drawStr(0, 28, "Good");
  else if (dust_10_num <= 80) u8g2.drawStr(0, 28, "Moderate");
  else if (dust_10_num <= 150) u8g2.drawStr(0, 28, "Bad");
  else u8g2.drawStr(0, 28, "Dangerous");
  if (dust_2_5_num <= 15) u8g2.drawStr(0, 54, "Good");
  else if (dust_2_5_num <= 35) u8g2.drawStr(0, 54, "Moderate");
  else if (dust_2_5_num <= 75) u8g2.drawStr(0, 54, "Bad");
  else u8g2.drawStr(0, 54, "Dangerous");
  u8g2.setFont(u8g2_font_maniac_tr);
  u8g2.drawStr(60, 14, String(dust_10_num).c_str());
  u8g2.drawStr(60, 40, String(dust_2_5_num).c_str());
  u8g2.sendBuffer();
}

void display_youtube_oled(U8G2 &u8g2) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  if (containsKorean(youtube_channel)) u8g2.setFont(u8g2_font_unifont_t_korean2);
  else u8g2.setFont(u8g2_font_bytesize_tr);
  drawCenteredText(u8g2, youtube_channel, 0);
  u8g2.setFont(u8g2_font_6x10_tf);
  drawCenteredText(u8g2, "YouTube Subscribers", 20);
  u8g2.setFont(u8g2_font_maniac_tr);
  drawCenteredText(u8g2, subscribe_num, 33);
  u8g2.sendBuffer();
}

// 공통 시세 위젯 표시 함수 (증시, 코인, 환율 등에 공통 사용)
void display_market_oled(U8G2 &u8g2, const String &title, const String &price, const String &change, const String &percent) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_bytesize_tr);
  drawCenteredText(u8g2, title, 5);
  u8g2.setFont(u8g2_font_maniac_tr);
  drawCenteredText(u8g2, price, 25);
  u8g2.setFont(u8g2_font_6x10_tf);
  String changeStr = change + " (" + percent + ")";
  drawCenteredText(u8g2, changeStr, 56);
  u8g2.sendBuffer();
}

// 개별 위젯 래퍼 함수 (기존 호출부 호환 유지)
void display_kospi_oled(U8G2 &u8g2) {
  display_market_oled(u8g2, "KOSPI", kospi_data.price, kospi_data.change, kospi_data.percent);
}
void display_kosdaq_oled(U8G2 &u8g2) {
  display_market_oled(u8g2, "KOSDAQ", kosdaq_data.price, kosdaq_data.change, kosdaq_data.percent);
}
void display_SnP500_oled(U8G2 &u8g2) {
  display_market_oled(u8g2, "S&P 500", snp500_data.price, snp500_data.change, snp500_data.percent);
}
void display_btc_oled(U8G2 &u8g2) {
  display_market_oled(u8g2, "Bitcoin(BTC $)", btc_data.price, btc_data.change, btc_data.percent);
}
void display_usdkrw_oled(U8G2 &u8g2) {
  display_market_oled(u8g2, "USD/KRW", usdkrw_data.price, usdkrw_data.change, usdkrw_data.percent);
}
void display_nasdaq_oled(U8G2 &u8g2) {
  display_market_oled(u8g2, "NASDAQ", nasdaq_data.price, nasdaq_data.change, nasdaq_data.percent);
}
void display_futures_oled(U8G2 &u8g2) {
  display_market_oled(u8g2, "FUTURES", futures_data.price, futures_data.change, futures_data.percent);
}
void display_kpi200_oled(U8G2 &u8g2) {
  display_market_oled(u8g2, "KOSPI200", kpi200_data.price, kpi200_data.change, kpi200_data.percent);
}


// 3번째 페이지: 설정 웹서버 IP 주소를 4개의 OLED에 크게 분산 표시
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
      u8g2.setFont(u8g2_font_maniac_tr);
      u8g2.drawStr(115, 28, ".");
    }
    
    u8g2.sendBuffer();
  }
}

void redraw_current_page() {
  u8g2_1.clearBuffer(); u8g2_1.sendBuffer();
  u8g2_2.clearBuffer(); u8g2_2.sendBuffer();
  u8g2_3.clearBuffer(); u8g2_3.sendBuffer();
  u8g2_4.clearBuffer(); u8g2_4.sendBuffer();

  if (current_page == 4) {
    display_ip_page();
    return;
  }

  int start_idx = (current_page - 1) * 4;
  for (int i = start_idx; i < start_idx + 4; i++) {
    int slot_num = i + 1;
    WidgetType type = SCREEN_MAP[i];

    if (type == W_NONE) continue;

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

void handleRoot() {
  String html;
  html.reserve(10000);

  // 헤더 및 스타일 (static block)
  html = INDEX_HTML_START;

  html += "<div class=\"section\">";
  html += "<h3>📺 스마트 화면 배치 (Slot 1~8)</h3>";
  html += "<p>물리적인 4개의 화면에 표시될 위젯을 페이지별로 설정하세요.</p>";

  const char *w_names[] = { "사용 안 함", "시계", "날씨", "미세먼지", "유튜브", "코스피", "코스닥", "코스피200", "선물지수", "S&P 500", "나스닥", "비트코인", "환율", "달력" };

  // 1페이지 설정
  html += "<div class=\"page-title\">[PAGE 1] 기본 화면</div>";
  html += "<div class=\"flex-container\">";
  for (int i = 0; i < 4; i++) {
    html += "<div class=\"flex-item\">";
    html += "<span class=\"label\">Screen " + String(i + 1) + "</span>";
    html += "<select name=\"sm" + String(i) + "\">";
    for (int j = 0; j <= 13; j++) {
      html += "<option value=\"" + String(j) + "\"";
      if ((int)SCREEN_MAP[i] == j) html += " selected";
      html += ">" + String(w_names[j]) + "</option>";
    }
    html += "</select></div>";
  }
  html += "</div><br>";

  // 2페이지 설정
  html += "<div class=\"page-title\">[PAGE 2] 버튼 클릭 시 전환 화면</div>";
  html += "<div class=\"flex-container\">";
  for (int i = 4; i < 8; i++) {
    html += "<div class=\"flex-item\">";
    html += "<span class=\"label\">Screen " + String(i - 3) + "</span>";  // 다시 1~4번으로 표시해 물리 화면 매칭
    html += "<select name=\"sm" + String(i) + "\">";
    for (int j = 0; j <= 13; j++) {
      html += "<option value=\"" + String(j) + "\"";
      if ((int)SCREEN_MAP[i] == j) html += " selected";
      html += ">" + String(w_names[j]) + "</option>";
    }
    html += "</select></div>";
  }
  html += "</div><br>";

  // 3페이지 설정
  html += "<div class=\"page-title\">[PAGE 3] 추가/심화 정보</div>";
  html += "<div class=\"flex-container\">";
  for (int i = 8; i < 12; i++) {
    html += "<div class=\"flex-item\">";
    html += "<span class=\"label\">Screen " + String(i - 7) + "</span>";
    html += "<select name=\"sm" + String(i) + "\">";
    for (int j = 0; j <= 13; j++) {
      html += "<option value=\"" + String(j) + "\"";
      if ((int)SCREEN_MAP[i] == j) html += " selected";
      html += ">" + String(w_names[j]) + "</option>";
    }
    html += "</select></div>";
  }
  html += "</div><br>";

  // 4페이지 안내 (고정형)
  html += "<div class=\"page-title\">[PAGE 4] 시스템 정보 (IP 주소)</div>";
  html += "<p style=\"color: #666; font-size: 0.9em; margin-bottom: 20px;\">* 4페이지는 설정 웹서버 접속 주소(IP)를 4개의 화면에 크게 나누어 표시하는 시스템 고정 페이지입니다.</p>";
  html += "</div>";

  // 나머지 입력 필드 템플릿 적용
  String settingsHtml = INDEX_HTML_SETTINGS;
  settingsHtml.replace("%CHANNEL%", youtube_channel);
  settingsHtml.replace("%LOCATION%", dust_location);
  settingsHtml.replace("%WEATHER%", weather_location_code);
  settingsHtml.replace("%TZ%", String(timezone_offset));

  html += settingsHtml;
  server.send(200, "text/html", html);
}

void handleSet() {
  bool changed = false;
  bool tz_changed = false;

  if (server.hasArg("channel") && server.arg("channel").length() > 0) {
    String new_channel = server.arg("channel");
    if (new_channel != youtube_channel) {
      youtube_channel = new_channel;
      subscribe_num = "";
      changed = true;
    }
  }
  if (server.hasArg("location") && server.arg("location").length() > 0) {
    dust_location = server.arg("location");
    dust_10_num = -1;
    changed = true;
  }
  if (server.hasArg("weather_code") && server.arg("weather_code").length() > 0) {
    weather_location_code = server.arg("weather_code");
    announcement_time = "";
    changed = true;
  }
  if (server.hasArg("tz_offset") && server.arg("tz_offset").length() > 0) {
    timezone_offset = server.arg("tz_offset").toInt();
    changed = true;
    tz_changed = true;
  }

  // 1~12번 슬롯 스크린맵 파라미터 처리
  for (int i = 0; i < 12; i++) {
    String argName = "sm" + String(i);
    if (server.hasArg(argName)) {
      int newVal = server.arg(argName).toInt();
      if (newVal < 0 || newVal > MAX_WIDGET_TYPE) newVal = 0;  // 범위 초과 방지
      if ((int)SCREEN_MAP[i] != newVal) {
        SCREEN_MAP[i] = (WidgetType)newVal;
        changed = true;
      }
    }
  }

  // 변경사항이 있을 경우에만 Preferences(Flash 메모리)에 저장하여 소자 수명 보호
  if (changed) {
    preferences.begin("settings", false);
    preferences.putString("channel", youtube_channel);
    preferences.putString("dust_loc", dust_location);
    preferences.putString("weather_code", weather_location_code);
    preferences.putInt("tz_offset", timezone_offset);
    preferences.end();

    // 스크린맵 NVS 메모리에 영구 저장 (별도 네임스페이스)
    preferences.begin("screen_map", false);
    for (int i = 0; i < 12; i++) {
      char key[5];
      sprintf(key, "s%d", i);
      preferences.putInt(key, (int)SCREEN_MAP[i]);
    }
    preferences.end();

    Serial.println("[SUCCESS] Settings saved to Preferences.");

    // 설정 변경 후 즉시 화면을 갱신하기 위해 현재 실행 중인 대기(delay)를 즉시 종료
    if (is_waiting && dataTaskHandle != NULL) {
      xTaskAbortDelay(dataTaskHandle);
    }
  }

  // 시간대 변경 시 즉시 시스템 시각 설정 재호출
  if (tz_changed) {
    configTime(timezone_offset * 3600, 0, "kr.pool.ntp.org", "time.nist.gov");
  }

  force_update = true;    // 설정 저장 직후 백그라운드 태스크에 즉시 갱신 명령 전달
  redraw_current_page();  // 저장 즉시 현재 페이지 화면 레이아웃 갱신 반영

  server.send(200, "text/html", SUCCESS_HTML);
}

// 루프 상태 저장용 정적 변수
static bool prev_updating_states[WIDGET_COUNT] = {false};
static unsigned long last_animation_tick[WIDGET_COUNT] = {0};

void setup() {
  Serial.begin(115200);

  // 버튼 스위치 초기화 (내부 Pull-up 저항 사용)
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // 하드웨어 I2C 초기화 (핀 5, 6)
  Wire.begin(hw_sda_pin, hw_scl_pin);
  Wire.setClock(400000);

  // OLED 초기화 전 SW I2C 콜백 가로채기(Injection)
  u8g2_3.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;
  u8g2_4.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;

  // OLED 통신 시작
  u8g2_1.setI2CAddress(0x3C * 2);
  u8g2_1.begin();
  u8g2_2.setI2CAddress(0x3D * 2);
  u8g2_2.begin();
  u8g2_3.setI2CAddress(0x3C * 2);
  u8g2_3.begin();
  u8g2_4.setI2CAddress(0x3D * 2);
  u8g2_4.begin();

  // 모든 화면 초기 지우기
  u8g2_1.clearBuffer(); u8g2_1.sendBuffer();
  u8g2_2.clearBuffer(); u8g2_2.sendBuffer();
  u8g2_3.clearBuffer(); u8g2_3.sendBuffer();
  u8g2_4.clearBuffer(); u8g2_4.sendBuffer();

  // WiFi 정보 표시 (1번 화면)
  U8G2 &main_screen = getScreen(1);
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
  preferences.end();

  preferences.begin("screen_map", true);
  for (int i = 0; i < 12; i++) {
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

  // [입력] 버튼(9번 핀) 처리: 페이지 전환(Short) 또는 데이터 새로고침(Long)
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != last_button_reading) last_debounce_time = now_ms;

  if ((now_ms - last_debounce_time) > 50) {
    if (reading != button_state) {
      button_state = reading;
      
      if (button_state == LOW) { // 버튼 눌림 시작
        button_press_start_time = now_ms;
        is_long_press_triggered = false;
      } else { // 버튼 뗌
        if (!is_long_press_triggered) {
          // [Short Press] 페이지 전환 (길게 누름이 아닌 경우만)
          current_page = (current_page % MAX_PAGE) + 1;
          for (int w = 0; w < WIDGET_COUNT; w++) prev_updating_states[w] = false;
          redraw_current_page();
          Serial.printf("[BUTTON] Short press -> Page %d\n", current_page);
        }
      }
    }
  }

  // [Long Press] 누르고 있는 동안 감지 (Hold 기반)
  if (button_state == LOW && !is_long_press_triggered) {
    if (now_ms - button_press_start_time >= LONG_PRESS_TIME) {
      is_long_press_triggered = true;
      Serial.println("[BUTTON] Long press detected. Refreshing data...");
      force_update = true;
      if (is_waiting && dataTaskHandle != NULL) {
        xTaskAbortDelay(dataTaskHandle);
      }
    }
  }
  last_button_reading = reading;

  // [표시 1] 시계 전용 업데이트 (1초마다 무조건 갱신, 3페이지 제외)
  static unsigned long last_clock_ms = 0;
  if (now_ms - last_clock_ms >= 1000) {
    if (current_page != 4) {
      int start_idx = (current_page - 1) * 4;
      for (int i = start_idx; i < start_idx + 4; i++) {
         if (SCREEN_MAP[i] == W_TIME) display_clock_oled(getScreen(i + 1));
         if (SCREEN_MAP[i] == W_CALENDAR) display_calendar_oled(getScreen(i + 1));
      }
    }
    last_clock_ms = now_ms;
  }

  // [표시 2] 데이터 위젯 통합 상태 엔진 (1, 2, 3페이지만 해당)
  if (current_page != 4) {
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
