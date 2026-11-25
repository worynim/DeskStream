// worynim@gmail.com
/**
 * @file data_manager.cpp
 * @brief 실시간 외부 정보(날씨, 코인, 환율 등) 수집 및 파싱 구현
 * @details HTTPClient를 통한 API 요청 처리 및 JSON 데이터의 지능적 파싱 관리
 */
#include "data_manager.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <UrlEncode.h>
#include "stream_scanner.h"
#include "dust.h"
#include "weather.h"
#include "youtube.h"
#include "market.h"
#include "btc.h"
#include "usdkrw.h"
#include "world_clock.h"

// --- 전역 데이터 변수 실제 정의 (Definition) ---
String dust_location = "대전";
String dust_host = "http://apis.data.go.kr";
int dust_10_num = -1;   // -1: 아직 받지 못함 (화면에 "--")
int dust_2_5_num = -1;

String weather_location_code = "4211054500";
String weather_location_name = "";
String weather_host = "http://apis.data.go.kr";
String announcement_time = "0000";
Forecast forecast[3];

String intl_city = "";
float intl_lat = 0;
float intl_lon = 0;
IntlForecast intl_forecast[3];
bool intl_valid = false;

WorldClockCity wc[WC_SLOTS];

void wcSetDefault(int i) {
  wc[i] = {"New York", 40.71427f, -74.00597f, 0, false};
}

String youtube_channel = "";
String subscribe_num = "";
const unsigned long youtube_timeout = 15000;

MarketData kospi_data = {"0.00", "0.00", "0.00%"};
MarketData kosdaq_data = {"0.00", "0.00", "0.00%"};
MarketData snp500_data = {"0.00", "0.00", "0.00%"};
MarketData nasdaq_data = {"0.00", "0.00", "0.00%"};
MarketData kpi200_data = {"0.00", "0.00", "0.00%"};
MarketData futures_data = {"0.00", "0.00", "0.00%"};
MarketData btc_data = {"0.00", "0.00", "0.00%"};
MarketData usdkrw_data = {"0.00", "0.00", "0.00%"};
MarketData eth_data = {"0.00", "0.00", "0.00%"};
MarketData jpy_data = {"0.00", "0.00", "0.00%"};
MarketData eur_data = {"0.00", "0.00", "0.00%"};
MarketData cny_data = {"0.00", "0.00", "0.00%"};

// --- Smart_Info_Station.ino 에 정의된 메인 상태 참조 ---
extern WidgetType SCREEN_MAP[12];
extern int current_page;

// --- 전역 제어 상태 변수 정의 ---
volatile uint16_t update_flag = 0x00;
portMUX_TYPE updateMux = portMUX_INITIALIZER_UNLOCKED;
SemaphoreHandle_t dataMutex = NULL;   // setup()에서 생성 (data_manager.h DataLock)

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
volatile bool is_updating_intl = false;
volatile bool is_updating_fx = false;
volatile bool is_updating_eth = false;
volatile bool is_updating_wc = false;
volatile bool force_update = false;  
volatile bool is_waiting = false;    
TaskHandle_t dataTaskHandle = NULL;  

unsigned long last_finance_update = 0;
unsigned long last_slow_update = 0;

// --- 위젯 매핑 테이블 ---
const WidgetUpdateInfo widgets_info[] = {
  { W_KOSPI,   &is_updating_kospi,   0x08, "Updating KOSPI",   display_kospi_oled,   get_kospi,     true,  false },
  { W_KOSDAQ,  &is_updating_kosdaq,  0x10, "Updating KOSDAQ",  display_kosdaq_oled,  get_kosdaq,    true,  false },
  { W_KPI200,  &is_updating_kpi200,  0x400, "Updating KOSPI200", display_kpi200_oled, get_kpi200,    true,  false },
  { W_FUTURES, &is_updating_futures, 0x200, "Updating Futures", display_futures_oled, get_futures,   true,  false },
  { W_SNP500,  &is_updating_SnP500,  0x20, "Updating S&P 500", display_SnP500_oled,  get_SnP500,    true,  false },
  { W_NASDAQ,  &is_updating_nasdaq,  0x100, "Updating NASDAQ",  display_nasdaq_oled,  get_nasdaq,    true,  false },
  { W_BTC,     &is_updating_btc,     0x40, "Updating BTC",     display_btc_oled,     get_btc,       true,  false },
  { W_USDKRW,  &is_updating_usdkrw,  0x80, "Updating USD/KRW", display_usdkrw_oled,  get_usdkrw,    true,  false },
  { W_FX,      &is_updating_fx,      0x1000, "Updating FX",    display_fx_oled,      get_fx,        true,  false },
  { W_ETH,     &is_updating_eth,     0x2000, "Updating ETH",   display_eth_oled,     get_eth,       true,  false },
  // 세계 시계는 1초마다 따로 그린다 (isTickWidget) — 여기서는 30분마다 UTC 오프셋만 받는다
  { W_WORLD_CLOCK, &is_updating_wc,  0x4000, "Updating Clock", nullptr,              get_world_clock, false, false },
  { W_WEATHER, &is_updating_weather, 0x02, "Updating Weather", display_weather_oled, get_weather,   false, false },
  { W_INTL_WEATHER, &is_updating_intl, 0x800, "Updating World Wx", display_intl_weather_oled, get_intl_weather, false, false },
  { W_DUST,    &is_updating_dust,    0x01, "Updating Dust",    display_dust_oled,    get_dust,      false, true  },
  { W_YOUTUBE, &is_updating_youtube, 0x04, "Updating YouTube", display_youtube_oled, get_subscribe, false, true  }
};
const int WIDGET_COUNT = sizeof(widgets_info) / sizeof(widgets_info[0]);

// --- [구현부 이전] 유틸리티 및 수집 함수들 ---

String addCommas(String val) {
  int dotIdx = val.indexOf('.');
  String integerPart = (dotIdx == -1) ? val : val.substring(0, dotIdx);
  String fractionalPart = (dotIdx == -1) ? "" : val.substring(dotIdx);
  String result = "";
  int len = integerPart.length();
  for (int i = 0; i < len; i++) {
    if (i > 0 && (len - i) % 3 == 0 && integerPart[i-1] != '-') result += ",";
    result += integerPart[i];
  }
  return result + fractionalPart;
}

String formatSubscribers(String subscribers) {
  subscribers.replace("구독자 ", "");
  subscribers.replace("팔로워 ", "");
  subscribers.replace("명", "");
  subscribers.trim();
  int chon_idx = subscribers.indexOf("천");
  int man_idx = subscribers.indexOf("만");
  int eok_idx = subscribers.indexOf("억");
  double number = 0;
  if (chon_idx != -1) number = subscribers.substring(0, chon_idx).toFloat() * 1000;
  else if (man_idx != -1) number = subscribers.substring(0, man_idx).toFloat() * 10000;
  else if (eok_idx != -1) number = subscribers.substring(0, eok_idx).toFloat() * 100000000;
  else {
    String result = "";
    for(int i=0; i<subscribers.length(); i++) {
      if(isDigit(subscribers[i]) || subscribers[i] == '.') result += subscribers[i];
    }
    return result;
  }
  String formattedString = "";
  if (number >= 1000000000) formattedString = String(number / 1000000000.0, 2) + "B";
  else if (number >= 1000000) formattedString = String(number / 1000000.0, 2) + "M";
  else if (number >= 1000) formattedString = String(number / 1000.0, 2) + "k";
  else formattedString = String((long)number);
  if (formattedString.indexOf('.') != -1) {
    char suffix = formattedString.charAt(formattedString.length() - 1);
    if (!isDigit(suffix)) formattedString.remove(formattedString.length() - 1);
    else suffix = '\0';
    while (formattedString.endsWith("0")) formattedString.remove(formattedString.length() - 1);
    if (formattedString.endsWith(".")) formattedString.remove(formattedString.length() - 1);
    if (suffix != '\0') formattedString += suffix;
  }
  return formattedString;
}

/**
 * 응답 본문을 512바이트씩 읽어 scanner에 넣는다 — 줄 단위 String을 만들지 않는다.
 * @param timeout_ms 데이터가 이 시간 동안 들어오지 않으면 그만 읽는다
 * @return onValue가 멈춤을 요청했으면 true (원하는 값을 다 찾음)
 */
template <typename F>
static bool scanResponse(HTTPClient &https, StreamScanner &scanner, unsigned long timeout_ms, F onValue) {
  WiFiClient *stream = https.getStreamPtr();
  uint8_t buf[512];
  unsigned long last = millis();
  while ((https.connected() || stream->available()) && (millis() - last < timeout_ms)) {
    int avail = stream->available();
    if (avail <= 0) {
      vTaskDelay(1);   // 데이터를 기다리는 동안 양보
      continue;
    }
    int got = stream->read(buf, avail < (int)sizeof(buf) ? avail : (int)sizeof(buf));
    if (got <= 0) continue;
    last = millis();   // 데이터 수신 시 타임아웃 갱신
    if (!scanner.feed(buf, got, onValue)) return true;
  }
  return false;
}

bool get_dust() {
  if (WiFi.status() != WL_CONNECTED) return false;

  String location;
  { DataLock lock; location = dust_location; }  // 웹 설정 변경과 겹치지 않도록 사본으로 작업

  // 네이버 모바일 미세먼지/초미세먼지 수치 — 키 뒤 4바이트를 정수로 읽는다
  static const ScanKey keys[] = {
    {">미세</span> <span class=\"num\">", 4, 0},
    {">초미세</span> <span class=\"num\">", 4, 0},
  };

  for (int retry = 0; retry < 3; retry++) {
    String query = urlEncode(location) + urlEncode(" 미세먼지");
    dust_host = "https://m.search.naver.com/search.naver?where=m&query=" + query;

    Serial.printf("\n[HTTP] get dust (Attempt %d/3)...\n", retry + 1);

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(5000);

    HTTPClient https;
    https.begin(client, dust_host);
    https.setTimeout(5000);
    https.setUserAgent("Mozilla/5.0 (iPhone; CPU iPhone OS 16_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.6 Mobile/15E148 Safari/604.1");

    int pm10 = -1, pm25 = -1;
    bool success = false;
    if (https.GET() == 200) {
      StreamScanner scanner(keys, 2);
      success = scanResponse(https, scanner, 5000, [&](int k, const char *v) {
        if (k == 0 && pm10 < 0) pm10 = atoi(v);
        if (k == 1 && pm25 < 0) pm25 = atoi(v);
        return !(pm10 >= 0 && pm25 >= 0);   // 둘 다 찾으면 멈춘다
      });
    }
    https.end();

    if (success) {
      { DataLock lock; dust_10_num = pm10; dust_2_5_num = pm25; }
      Serial.printf("[SUCCESS] dust 10um: %d\n", pm10);
      Serial.printf("[SUCCESS] dust 2.5um: %d\n", pm25);
      return true;
    }

    Serial.println("[ERROR] Dust: Fail to parse, retrying...");
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  return false;
}

bool get_weather() {
  if (WiFi.status() != WL_CONNECTED) return false;

  String code;
  { DataLock lock; code = weather_location_code; }  // 웹 설정 변경과 겹치지 않도록 사본으로 작업

  // 기상청 디지털 예보 — 발표 시각 17바이트, 시간대별 시각 2·날씨 코드 2·기온 3바이트
  static const ScanKey keys[] = {
    {"3시간간격예보(현재부터 +일까지) : <span>", 17, 0},
    {"hid\">시각: </span><span>", 2, 0},
    {"hid\">날씨: </span><span class=\"wic DB", 2, 0},
    {"hid\">기온 : </span><span class=\"hid feel\">", 3, 0},
  };

  for (int retry = 0; retry < 3; retry++) {
    weather_host = "https://www.weather.go.kr/w/wnuri-fct2021/main/digital-forecast.do?code=" + code;
    
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(5000);

    HTTPClient https;
    https.begin(client, weather_host);
    https.setTimeout(5000);
    https.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36");

    Serial.printf("\n[HTTP] get Weather (Attempt %d/3)...\n", retry + 1);
    Forecast fc[3] = {};   // 3개 시간대를 모두 찾았을 때만 전역 forecast에 반영한다
    String ann = "";
    bool success = false;
    if (https.GET() == 200) {
      int idx = 0;
      StreamScanner scanner(keys, 4);
      success = scanResponse(https, scanner, 5000, [&](int k, const char *v) {
        if (k == 0) ann = v;                      // 1. 발표 시각
        else if (k == 1) fc[idx].time = atoi(v);  // 2. 예보 시각
        else if (k == 2) fc[idx].type = atoi(v);  // 3. 날씨 아이콘 타입
        else {                                    // 4. 기온 — 한 시간대 완료
          fc[idx].temp = atoi(v);
          if (++idx >= 3) return false;           // 3개 시간대를 모두 찾으면 멈춘다
        }
        return true;
      });
    }
    https.end();

    if (success) {
      {
        DataLock lock;
        for (int i = 0; i < 3; i++) forecast[i] = fc[i];
        if (ann.length() > 0) announcement_time = ann;
      }
      Serial.print("[SUCCESS] Weather Time: "); Serial.println(ann);
      return true;
    }
    Serial.println("[ERROR] Weather: Fail to fetch, retrying...");
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  return false;
}

// --- 해외 날씨: Open-Meteo 예보 (API 키 불필요, 응답 약 400B) ---

/** payload의 from 이후 key 배열에서 숫자 n개를 읽는다 — 예: "temperature_2m":[5.2,4.2,7.7] */
static bool parseNumberArray(const String &payload, int from, const char *key, float *out, int n) {
  int k = payload.indexOf(key, from);
  if (k < 0) return false;
  const char *p = payload.c_str() + k + strlen(key);
  for (int i = 0; i < n; i++) {
    char *end;
    out[i] = strtof(p, &end);
    if (end == p) return false;   // null 등 숫자가 아님
    p = end;
    if (i < n - 1) {
      if (*p != ',') return false;
      p++;
    }
  }
  return true;
}

/** "time":["2026-11-26T03:00",...]에서 시(hour) n개를 읽는다 */
static bool parseHourArray(const String &payload, int from, int *out, int n) {
  int k = payload.indexOf("\"time\":[", from);
  if (k < 0) return false;
  const char *p = payload.c_str() + k + 8;
  for (int i = 0; i < n; i++) {
    p = strchr(p, 'T');
    if (!p) return false;
    out[i] = atoi(++p);
  }
  return true;
}

bool get_intl_weather() {
  if (WiFi.status() != WL_CONNECTED) return false;

  String city;
  float lat, lon;
  { DataLock lock; city = intl_city; lat = intl_lat; lon = intl_lon; }
  if (city.length() == 0) return true;   // 도시 미설정 — 화면에 설정 안내만 표시

  // 3시간 간격 3개 시각의 기온·날씨 코드, 시각은 도시 현지 시각(timezone=auto)
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 4) +
               "&longitude=" + String(lon, 4) +
               "&hourly=temperature_2m,weather_code&forecast_hours=9&temporal_resolution=hourly_3&timezone=auto";

  for (int retry = 0; retry < 3; retry++) {
    Serial.printf("\n[HTTP] get World Weather %s (Attempt %d/3)...\n", city.c_str(), retry + 1);

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(5000);

    HTTPClient https;
    https.begin(client, url);
    https.setTimeout(5000);

    bool success = false;
    IntlForecast fc[3];
    if (https.GET() == 200) {
      String payload = https.getString();
      int h = payload.indexOf("\"hourly\":{");   // hourly_units에도 같은 키가 있어 hourly 뒤에서 찾는다
      int hours[3];
      float temps[3], codes[3];
      if (h >= 0 && parseHourArray(payload, h, hours, 3) &&
          parseNumberArray(payload, h, "\"temperature_2m\":[", temps, 3) &&
          parseNumberArray(payload, h, "\"weather_code\":[", codes, 3)) {
        for (int i = 0; i < 3; i++) fc[i] = {hours[i], (int)codes[i], (int)lroundf(temps[i])};
        success = true;
      }
    }
    https.end();

    if (success) {
      {
        DataLock lock;
        if (intl_city == city) {   // 수집 중 도시가 바뀌었으면 버린다 (다음 갱신에서 새 도시를 받는다)
          for (int i = 0; i < 3; i++) intl_forecast[i] = fc[i];
          intl_valid = true;
        }
      }
      Serial.printf("[SUCCESS] World Weather: %s %d°C\n", city.c_str(), fc[0].temp);
      return true;
    }
    Serial.println("[ERROR] World Weather: Fail to fetch, retrying...");
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  return false;
}

// --- 세계 시계: Open-Meteo가 도시 시간대로 계산한 현재 UTC 오프셋 (응답 약 200B) ---

/** 좌표의 현재 UTC 오프셋(초)을 받는다 */
static bool fetchUtcOffset(const String &city, float lat, float lon, long &offset) {
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 4) +
               "&longitude=" + String(lon, 4) + "&timezone=auto&forecast_days=1";

  for (int retry = 0; retry < 3; retry++) {
    Serial.printf("\n[HTTP] get World Clock %s (Attempt %d/3)...\n", city.c_str(), retry + 1);

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(5000);

    HTTPClient https;
    https.begin(client, url);
    https.setTimeout(5000);

    bool success = false;
    if (https.GET() == 200) {
      String payload = https.getString();
      int o = payload.indexOf("\"utc_offset_seconds\":");
      if (o >= 0) {
        offset = atol(payload.c_str() + o + 21);
        success = true;
      }
    }
    https.end();

    if (success) {
      Serial.printf("[SUCCESS] World Clock: %s UTC%+ld s\n", city.c_str(), offset);
      return true;
    }
    Serial.println("[ERROR] World Clock: Fail to fetch, retrying...");
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  return false;
}

bool get_world_clock() {
  if (WiFi.status() != WL_CONNECTED) return false;

  bool all_ok = true;
  bool fetched[WC_SLOTS] = {false};   // 이번에 받은 칸 — 같은 도시는 다시 받지 않는다
  long got[WC_SLOTS] = {0};
  for (int i = 0; i < WC_SLOTS; i++) {
    if (SCREEN_MAP[i] != W_WORLD_CLOCK) continue;

    String city;
    float lat, lon;
    { DataLock lock; city = wc[i].name; lat = wc[i].lat; lon = wc[i].lon; }

    long offset = 0;
    bool ok = false;
    for (int j = 0; j < i && !ok; j++) {
      if (!fetched[j]) continue;
      DataLock lock;
      if (wc[j].lat == lat && wc[j].lon == lon) { offset = got[j]; ok = true; }
    }
    if (!ok) ok = fetchUtcOffset(city, lat, lon, offset);
    if (!ok) { all_ok = false; continue; }

    fetched[i] = true;
    got[i] = offset;
    DataLock lock;
    if (wc[i].name == city) {   // 수집 중 도시가 바뀌었으면 버린다
      wc[i].offset = offset;
      wc[i].valid = true;
    }
  }
  return all_ok;
}

// --- YouTube 구독자 수: YouTube 웹 페이지가 쓰는 내부 요청(youtubei) — API 키 불필요 ---
// 채널 페이지(약 2.5MB, 구독자 문구가 1.62MB 지점)를 읽는 대신, 채널의 "검색" 탭을 요청한다.
// 검색 탭은 본문이 거의 비어 있어 채널 헤더(구독자 수)가 응답 앞 약 10KB 안에 온다.
static const char *YT_API = "https://www.youtube.com/youtubei/v1/";
static const char *YT_CONTEXT = "\"context\":{\"client\":{\"clientName\":\"WEB\",\"clientVersion\":\"2.20241126.01.00\",\"hl\":\"ko\"}}";
static const char *YT_SEARCH_TAB = "EgZzZWFyY2jyBgQKAloA";   // browse params: 채널의 "검색" 탭

static String yt_cached_channel = "";   // yt_cached_id를 얻은 채널 입력값 (dataTask 전용)
static String yt_cached_id = "";        // 채널 ID (UC…) — 채널이 바뀔 때만 다시 얻는다

/**
 * youtubei 요청을 보내고 응답을 scanner로 훑는다.
 * @return onValue가 멈춤을 요청했으면 true (원하는 값을 찾음)
 */
template <typename F>
static bool youtubePost(const char *endpoint, const String &body, StreamScanner &scanner, F onValue) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(youtube_timeout);

  HTTPClient https;
  https.begin(client, String(YT_API) + endpoint + "?prettyPrint=false");
  https.setTimeout(youtube_timeout);
  https.addHeader("Content-Type", "application/json");

  bool found = false;
  if (https.POST(body) == 200) found = scanResponse(https, scanner, youtube_timeout, onValue);
  https.end();
  return found;
}

/** 채널 입력값(@핸들, 핸들, channel/UC…, c/이름)을 채널 ID로 바꾼다 — navigation/resolve_url (응답 약 1KB) */
static bool resolveChannelId(const String &channel, String &id) {
  String encoded = channel.startsWith("@") ? "@" + urlEncode(channel.substring(1)) : urlEncode(channel);
  String body = String("{") + YT_CONTEXT + ",\"url\":\"https://www.youtube.com/" + encoded + "\"}";

  static const ScanKey keys[] = {{"\"browseId\":\"", 31, '"'}};
  StreamScanner scanner(keys, 1);
  return youtubePost("navigation/resolve_url", body, scanner, [&](int, const char *v) {
    id = v;
    return false;
  });
}

bool get_subscribe() {
  if (WiFi.status() != WL_CONNECTED) return false;

  String channel;
  { DataLock lock; channel = youtube_channel; }  // 웹 설정 변경과 겹치지 않도록 사본으로 작업

  // 채널 헤더의 구독자 문구 — 예: {"content":"구독자 4680만명"} → "4680만명"
  static const ScanKey keys[] = {
    {"{\"content\":\"구독자 ", 31, '"'},
  };

  for (int retry = 0; retry < 3; retry++) {
    Serial.printf("\n[HTTP] get YouTube (Attempt %d/3)...\n", retry + 1);

    if (channel != yt_cached_channel || yt_cached_id.length() == 0) {
      String id;
      if (!resolveChannelId(channel, id)) {
        Serial.println("[ERROR] YouTube: channel not found, retrying...");
        vTaskDelay(500 / portTICK_PERIOD_MS);
        continue;
      }
      yt_cached_channel = channel;
      yt_cached_id = id;
      Serial.printf("[YouTube] %s -> %s\n", channel.c_str(), id.c_str());
    }

    String body = String("{") + YT_CONTEXT + ",\"browseId\":\"" + yt_cached_id + "\",\"params\":\"" + YT_SEARCH_TAB + "\"}";
    String raw = "";
    StreamScanner scanner(keys, 1);
    bool success = youtubePost("browse", body, scanner, [&](int, const char *v) {
      String s = v;
      if (!s.endsWith("명")) return true;   // 구독자 수 문구가 아니면 계속 찾는다
      raw = s;
      return false;
    });

    if (success) {
      String subs = formatSubscribers(raw);
      { DataLock lock; subscribe_num = subs; }
      Serial.print("[SUCCESS] YouTube: "); Serial.println(subs);
      return true;
    }
    yt_cached_id = "";   // 채널 ID가 바뀌었을 수 있으니 다음 시도에서 다시 얻는다
    Serial.println("[ERROR] YouTube: Fail to parse, retrying...");
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  return false;
}

bool fetchMarketData(const String& name, const String& url, MarketData& target) {
  if (WiFi.status() != WL_CONNECTED) return false;

  for (int retry = 0; retry < 3; retry++) {
    bool success = false;
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(5000);

    HTTPClient https;
    https.begin(client, url);
    https.setTimeout(5000);
    https.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36");

    Serial.printf("\n[HTTP] get %s (Attempt %d/3)...\n", name.c_str(), retry + 1);
    int httpCode = https.GET();
    if (httpCode == 200) {
      String payload = https.getString();
      
      // 파싱 시도 (최소한 가격 정보는 있어야 성공으로 간주)
      int price_idx = payload.indexOf("\"closePriceRaw\":\"");
      if (price_idx != -1) {
        MarketData data;
        { DataLock lock; data = target; }   // 지역 사본에서 파싱 — 화면이 읽는 중인 String을 직접 바꾸지 않는다
        int price_end = payload.indexOf("\"", price_idx + 17);
        data.price = payload.substring(price_idx + 17, price_end);
        
        // 등락률 추출
        int percent_idx = payload.indexOf("\"fluctuationsRatioRaw\":\"");
        if (percent_idx != -1) {
          int percent_end = payload.indexOf("\"", percent_idx + 24);
          data.percent = payload.substring(percent_idx + 24, percent_end);
        }

        // 등락폭 추출
        int change_idx = payload.indexOf("\"compareToPreviousClosePriceRaw\":\"");
        if (change_idx != -1) {
          int change_end = payload.indexOf("\"", change_idx + 34);
          data.change = payload.substring(change_idx + 34, change_end);
        }

        // 부호 처리 (+, -)
        int compare_idx = payload.indexOf("\"compareToPreviousPrice\":{\"code\":\"");
        if (compare_idx != -1) {
          String code = payload.substring(compare_idx + 34, compare_idx + 35);
          if (code == "2") {
            if (!data.change.startsWith("+")) data.change = "+" + data.change;
            if (!data.percent.startsWith("+")) data.percent = "+" + data.percent;
          } else if (code == "5") {
            if (!data.change.startsWith("-")) data.change = "-" + data.change;
            if (!data.percent.startsWith("-")) data.percent = "-" + data.percent;
          }
        }

        if (data.percent.indexOf("%") == -1) data.percent = data.percent + "%";
        { DataLock lock; target = data; }

        Serial.printf("[SUCCESS] %s: %s\n", name.c_str(), data.price.c_str());
        success = true;
      }
    }
    https.end();

    if (success) return true;
    
    Serial.printf("[ERROR] %s: Fail to fetch, retrying...\n", name.c_str());
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  return false;
}

bool get_kospi() { return fetchMarketData("KOSPI", "https://polling.finance.naver.com/api/realtime/domestic/index/KOSPI", kospi_data); }
bool get_kosdaq() { return fetchMarketData("KOSDAQ", "https://polling.finance.naver.com/api/realtime/domestic/index/KOSDAQ", kosdaq_data); }
bool get_kpi200() { return fetchMarketData("KOSPI200", "https://polling.finance.naver.com/api/realtime/domestic/index/KPI200", kpi200_data); }
bool get_futures() { return fetchMarketData("FUTURES", "https://polling.finance.naver.com/api/realtime/domestic/index/FUT", futures_data); }
bool get_SnP500() { return fetchMarketData("S&P 500", "https://polling.finance.naver.com/api/realtime/worldstock/index/.INX", snp500_data); }
bool get_nasdaq() { return fetchMarketData("NASDAQ", "https://polling.finance.naver.com/api/realtime/worldstock/index/.IXIC", nasdaq_data); }

/** Binance 24시간 시세(USDT) — symbol 예: BTCUSDT. 가격은 정수 달러로 표시한다 */
static bool fetchBinance(const char *name, const char *symbol, MarketData &target) {
  if (WiFi.status() != WL_CONNECTED) return false;
  
  for (int retry = 0; retry < 3; retry++) {
    bool success = false;
    String url = String("https://api.binance.com/api/v3/ticker/24hr?symbol=") + symbol;
    
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(5000);

    HTTPClient https;
    https.begin(client, url);
    https.setTimeout(5000);

    Serial.printf("\n[HTTP] get %s (Attempt %d/3)...\n", name, retry + 1);
    int httpCode = https.GET();
    if (httpCode == 200) {
      String payload = https.getString();
      
      // 1. 현재 가격(lastPrice) 추출
      int price_idx = payload.indexOf("\"lastPrice\":\"");
      if (price_idx != -1) {
        MarketData data;
        { DataLock lock; data = target; }   // 지역 사본에서 파싱 — 화면이 읽는 중인 String을 직접 바꾸지 않는다
        int price_end = payload.indexOf("\"", price_idx + 13);
        String rawPrice = payload.substring(price_idx + 13, price_end);
        
        // 소수점 이하는 버리고 정수 부분만 사용 (가독성)
        int dot_idx = rawPrice.indexOf(".");
        if (dot_idx != -1) rawPrice = rawPrice.substring(0, dot_idx);
        data.price = addCommas(rawPrice);

        // 2. 24시간 변동폭(priceChange) 추출
        int change_idx = payload.indexOf("\"priceChange\":\"");
        if (change_idx != -1) {
          int change_end = payload.indexOf("\"", change_idx + 15);
          String rawChange = payload.substring(change_idx + 15, change_end);
          int dot_c_idx = rawChange.indexOf(".");
          if (dot_c_idx != -1) rawChange = rawChange.substring(0, dot_c_idx);
          
          float chgVal = rawChange.toFloat();
          if (chgVal > 0) data.change = "+" + addCommas(rawChange); 
          else if (chgVal < 0) {
            rawChange.replace("-", ""); 
            data.change = "-" + addCommas(rawChange);
          } else data.change = "0";
        }

        // 3. 24시간 변동률(priceChangePercent) 추출
        int percent_idx = payload.indexOf("\"priceChangePercent\":\"");
        if (percent_idx != -1) {
          int percent_end = payload.indexOf("\"", percent_idx + 22);
          float percent = payload.substring(percent_idx + 22, percent_end).toFloat();
          if (percent > 0) data.percent = "+" + String(percent, 2) + "%";
          else if (percent < 0) data.percent = String(percent, 2) + "%";
          else data.percent = "0.00%";
        }
        Serial.printf("[SUCCESS] %s: %s\n", name, data.price.c_str());
        { DataLock lock; target = data; }
        success = true;
      }
    }
    https.end();

    if (success) return true;
    Serial.printf("[ERROR] %s: Fail to fetch, retrying...\n", name);
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  return false;
}

bool get_btc() { return fetchBinance("BTC", "BTCUSDT", btc_data); }
bool get_eth() { return fetchBinance("ETH", "ETHUSDT", eth_data); }

/**
 * Yahoo 환율 차트 — symbol 예: KRW=X(USD/KRW), JPYKRW=X. scale배 해서 표시한다 (엔화는 100엔당 100).
 */
static bool fetchYahooFx(const char *name, const char *symbol, float scale, MarketData &target) {
  if (WiFi.status() != WL_CONNECTED) return false;
  
  for (int retry = 0; retry < 3; retry++) {
    bool success = false;
    String url = String("https://query1.finance.yahoo.com/v8/finance/chart/") + symbol + "?interval=1d";
    
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(5000);

    HTTPClient https;
    https.begin(client, url);
    https.setTimeout(5000);
    https.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36");

    Serial.printf("\n[HTTP] get %s (Attempt %d/3)...\n", name, retry + 1);
    int httpCode = https.GET();
    if (httpCode == 200) {
      String payload = https.getString();
      
      // 1. 현재 실시간 환율(regularMarketPrice) 추출
      int price_idx = payload.indexOf("\"regularMarketPrice\":");
      float currentPrice = 0;
      if (price_idx != -1) {
        MarketData data;
        { DataLock lock; data = target; }   // 지역 사본에서 파싱 — 화면이 읽는 중인 String을 직접 바꾸지 않는다
        int price_end = payload.indexOf(",", price_idx);
        String rawPrice = payload.substring(price_idx + 21, price_end);
        currentPrice = rawPrice.toFloat() * scale;
        data.price = String(currentPrice, 2); 

        // 2. 전일 종가(chartPreviousClose) 추출 (등락폭 계산용)
        int prev_idx = payload.indexOf("\"chartPreviousClose\":");
        float prevPrice = 0;
        if (prev_idx != -1) {
          int prev_end = payload.indexOf(",", prev_idx);
          String rawPrev = payload.substring(prev_idx + 21, prev_end);
          prevPrice = rawPrev.toFloat() * scale;
        }

        // 3. 등락폭 및 등락률 계산
        if (currentPrice > 0 && prevPrice > 0) {
          float changeVal = currentPrice - prevPrice;
          float percentVal = (changeVal / prevPrice) * 100.0;
          
          if (changeVal > 0) {
            data.change = "+" + String(changeVal, 2);
            data.percent = "+" + String(percentVal, 2) + "%";
          } else if (changeVal < 0) {
            data.change = String(changeVal, 2);
            data.percent = String(percentVal, 2) + "%";
          } else {
            data.change = "0.00";
            data.percent = "0.00%";
          }
        }
        Serial.printf("[SUCCESS] %s: %s\n", name, data.price.c_str());
        { DataLock lock; target = data; }
        success = true;
      }
    }
    https.end();

    if (success) return true;
    Serial.printf("[ERROR] %s: Fail to fetch, retrying...\n", name);
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  return false;
}

bool get_usdkrw() { return fetchYahooFx("USD/KRW", "KRW=X", 1, usdkrw_data); }

/** 엔·유로·위안 환율 위젯 — 세 통화를 모두 받아야 성공 (하나라도 실패하면 재시도 간격으로 다시 받는다) */
bool get_fx() {
  bool jpy = fetchYahooFx("JPY/KRW", "JPYKRW=X", 100, jpy_data);   // 100엔당
  bool eur = fetchYahooFx("EUR/KRW", "EURKRW=X", 1, eur_data);
  bool cny = fetchYahooFx("CNY/KRW", "CNYKRW=X", 1, cny_data);
  return jpy && eur && cny;
}

// --- 가시성 판단 로직 ---

bool isWidgetActive(WidgetType type) {
  for (int i = 0; i < MAX_DATA_PAGE * 4; i++) {
    if (SCREEN_MAP[i] == type) return true;
  }
  return false;
}

// --- 시장 개폐장 판단 로직 ---
// 장 시간은 각 시장의 현지 시각으로 판단한다 — 기기 시간대 설정(웹 tz_offset)과 무관하다.
// 시각이 아직 동기화되지 않았으면 true(업데이트 시도)를 돌려준다. 공휴일은 고려하지 않는다.

static const time_t kTimeSyncedEpoch = 1600000000;   // 2020-09 이후면 NTP로 맞춰진 시각으로 본다

/** 그레고리력 요일 (0=일요일) */
static int dayOfWeek(int y, int m, int d) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

/** 미국 동부 서머타임(EDT) 여부 — 3월 둘째 일요일 07:00 UTC ~ 11월 첫째 일요일 06:00 UTC */
static bool isUsEasternDst(time_t utc) {
  struct tm u;
  gmtime_r(&utc, &u);
  int y = u.tm_year + 1900, mon = u.tm_mon + 1, day = u.tm_mday;
  if (mon < 3 || mon > 11) return false;
  if (mon > 3 && mon < 11) return true;
  if (mon == 3) {
    int start = 8 + (7 - dayOfWeek(y, 3, 8)) % 7;   // 8일 이후 첫 일요일 = 둘째 일요일
    return day > start || (day == start && u.tm_hour >= 7);
  }
  int end = 1 + (7 - dayOfWeek(y, 11, 1)) % 7;      // 첫째 일요일
  return day < end || (day == end && u.tm_hour < 6);
}

/** UTC 시각을 고정 오프셋(시간 단위)의 현지 시각으로 바꾼다 */
static void localAt(time_t utc, int offset_hours, struct tm *out) {
  time_t t = utc + (time_t)offset_hours * 3600;
  gmtime_r(&t, out);
}

bool isDomesticMarketOpen() {
  time_t now = time(nullptr);
  if (now < kTimeSyncedEpoch) return true; // 시각 미동기 시 안전하게 업데이트 시도
  struct tm kst;
  localAt(now, 9, &kst);   // 한국 표준시 UTC+9 (서머타임 없음)
  int wday = kst.tm_wday, hour = kst.tm_hour, min = kst.tm_min;

  // 국내 증시 (KOSPI, KOSDAQ 등): 월~금 08:30 - 16:10 (장전 시간외 ~ 장후 정리 시간)
  if (wday >= 1 && wday <= 5) {
    if (hour >= 9 && hour < 16) return true;
    if (hour == 8 && min >= 30) return true;
    if (hour == 16 && min <= 10) return true;
  }
  return false;
}

bool isUsMarketOpen() {
  time_t now = time(nullptr);
  if (now < kTimeSyncedEpoch) return true;
  struct tm et;
  localAt(now, isUsEasternDst(now) ? -4 : -5, &et);   // 미국 동부 시각 (EDT/EST)

  // 미국 증시: 월~금 04:00 - 20:00 ET (프리마켓 ~ 애프터마켓)
  if (et.tm_wday < 1 || et.tm_wday > 5) return false;
  return et.tm_hour >= 4 && et.tm_hour < 20;
}

bool isFxMarketOpen() {
  time_t now = time(nullptr);
  if (now < kTimeSyncedEpoch) return true;
  struct tm et;
  localAt(now, isUsEasternDst(now) ? -4 : -5, &et);   // 외환시장 주간 개폐는 뉴욕 17:00 기준

  // 외환: 평일 24시간 — 금요일 17:00 ET 마감 ~ 일요일 17:00 ET 개장 사이만 쉰다
  if (et.tm_wday == 6) return false;
  if (et.tm_wday == 5 && et.tm_hour >= 17) return false;
  if (et.tm_wday == 0 && et.tm_hour < 17) return false;
  return true;
}

// --- 🚀 백그라운드 데이터 수집 태스크 ---

/**
 * 위젯 하나를 수집하고 다음 수집 예정 시각을 정한다.
 * @param show_progress true면 수집 중 OLED에 "Updating..."을 표시한다
 * @details 성공하면 정기 주기(금융 1분·기타 30분) 뒤, 실패하면 INTERVAL_RETRY 뒤에 다시 수집한다.
 *          실패해도 update_flag를 세워 "Updating..." 화면을 이전 값으로 되돌린다.
 */
static void fetchWidget(int w, bool show_progress, unsigned long next_due[]) {
  const WidgetUpdateInfo &info = widgets_info[w];
  if (show_progress) *info.is_updating = true;
  bool ok = info.fetch();
  if (show_progress) *info.is_updating = false;

  unsigned long interval = info.is_finance ? INTERVAL_FINANCE : INTERVAL_SLOW;
  next_due[w] = millis() + (ok ? interval : INTERVAL_RETRY);

  portENTER_CRITICAL(&updateMux);
  update_flag |= info.flag_mask;
  portEXIT_CRITICAL(&updateMux);
}

void dataTask(void *pvParameters) {
  static unsigned long next_due[MAX_WIDGET_RECORDS] = {0};   // 위젯별 다음 수집 예정 시각 (millis)

  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      uint16_t processed_mask = 0;

      // --- 강제 업데이트 플래그 확인 ---
      // 읽는 즉시 지운다 — 수집 도중 들어온 새 요청은 다음 바퀴에서 처리된다 (끝에서 지우면 그 요청이 사라진다)
      portENTER_CRITICAL(&updateMux);
      bool local_force = force_update;
      force_update = false;
      portEXIT_CRITICAL(&updateMux);

      // --- 초기 로딩 또는 강제 업데이트 시 처리 (모든 활성화된 위젯 대상) ---
      static bool is_initial = true;
      if (is_initial || local_force) {
        Serial.printf("[TASK] %s: Starting...\n", is_initial ? "Initial Loading" : "Forced Refresh");

        // Pass 1: 현재 페이지 위젯 우선 수집 (is_updating ON 으로 시각적 피드백 제공)
        if (current_page <= MAX_DATA_PAGE) {
          int start_idx = (current_page - 1) * 4;
          for (int i = start_idx; i < start_idx + 4; i++) {
            WidgetType type = SCREEN_MAP[i];
            if (type == W_NONE) continue;   // 수집할 데이터가 없는 위젯은 아래 표에 없어 건너뛴다
            for (int w = 0; w < WIDGET_COUNT; w++) {
              if (widgets_info[w].type == type) {
                if (!(processed_mask & widgets_info[w].flag_mask)) {
                  fetchWidget(w, true, next_due);
                  processed_mask |= widgets_info[w].flag_mask;
                }
                break;
              }
            }
          }
        }

        // Pass 2: 나머지 모든 활성화된 위젯들 배경에서 조용히 수집 (시장 시간 무관)
        for (int w = 0; w < WIDGET_COUNT; w++) {
          if (!(processed_mask & widgets_info[w].flag_mask) && isWidgetActive(widgets_info[w].type)) {
            fetchWidget(w, false, next_due);
            processed_mask |= widgets_info[w].flag_mask;
          }
        }
        is_initial = false;
      }

      // --- 정기 주기별 개별 업데이트 체크 (현재 페이지 위젯 대상) ---
      // 주의: 강제 업데이트 시에는 이미 위에서 처리되었으므로 processed_mask에 의해 스킵됨
      if (current_page <= MAX_DATA_PAGE) {
        unsigned long now = millis();
        int start_idx = (current_page - 1) * 4;
        for (int i = start_idx; i < start_idx + 4; i++) {
          WidgetType type = SCREEN_MAP[i];
          if (type == W_NONE) continue;   // 수집할 데이터가 없는 위젯은 아래 표에 없어 건너뛴다

          for (int w = 0; w < WIDGET_COUNT; w++) {
            if (widgets_info[w].type == type) {
              if (processed_mask & widgets_info[w].flag_mask) break;

              bool needs_update = (long)(now - next_due[w]) >= 0;   // millis 넘침에도 안전한 비교

              if (needs_update) {
                // 시장 개폐 시간에 따른 예외 처리 (정기 업데이트 시에만 적용)
                if ((type == W_KOSPI || type == W_KOSDAQ || type == W_KPI200 || type == W_FUTURES) && !isDomesticMarketOpen()) needs_update = false;
                if ((type == W_SNP500 || type == W_NASDAQ) && !isUsMarketOpen()) needs_update = false;
                if ((type == W_USDKRW || type == W_FX) && !isFxMarketOpen()) needs_update = false;
              }

              if (needs_update) {
                fetchWidget(w, true, next_due);
                processed_mask |= widgets_info[w].flag_mask;
              }
              break;
            }
          }
        }
      }
    }
    is_waiting = true;
    vTaskDelay(pdMS_TO_TICKS(1000));
    is_waiting = false;
  }
}
