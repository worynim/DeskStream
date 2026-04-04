#include "data_manager.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <UrlEncode.h>
#include "dust.h"
#include "weather.h"
#include "youtube.h"
#include "market.h"
#include "btc.h"
#include "usdkrw.h"

// --- 전역 데이터 변수 실제 정의 (Definition) ---
String dust_location = "대전";
String dust_host = "http://apis.data.go.kr";
int dust_10_num = 0;
int dust_2_5_num = 0;

String weather_location_code = "4211054500";
String weather_host = "http://apis.data.go.kr";
String announcement_time = "0000";
Forecast forecast[3];

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

// --- Smart_Info_Station.ino 에 정의된 메인 상태 참조 ---
extern WidgetType SCREEN_MAP[12];
extern int current_page;

// --- 전역 제어 상태 변수 정의 ---
volatile uint16_t update_flag = 0x00;
portMUX_TYPE updateMux = portMUX_INITIALIZER_UNLOCKED;

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
  { W_WEATHER, &is_updating_weather, 0x02, "Updating Weather", display_weather_oled, get_weather,   false, false },
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

void get_dust() {
  if (WiFi.status() != WL_CONNECTED) return;

  for (int retry = 0; retry < 3; retry++) {
    bool success = false;
    String query = urlEncode(dust_location) + urlEncode(" 미세먼지");
    dust_host = "https://m.search.naver.com/search.naver?where=m&query=" + query;

    Serial.printf("\n[HTTP] get dust (Attempt %d/3)...\n", retry + 1);

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(5000);

    HTTPClient https;
    https.begin(client, dust_host);
    https.setTimeout(5000);
    https.setUserAgent("Mozilla/5.0 (iPhone; CPU iPhone OS 16_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.6 Mobile/15E148 Safari/604.1");

    int httpCode = https.GET();
    if (httpCode == 200) {
      WiFiClient *stream = https.getStreamPtr(); // 대용량 HTML 처리를 위한 스트림 획득
      unsigned long startTime = millis();

      // 서버 연결 유지 시간 동안 스트리밍 파싱 수행
      while (https.connected() && (millis() - startTime < 5000)) {
        if (stream->available()) {
          String line = stream->readStringUntil('\n'); // 라인 단위로 읽기
          Serial.print('.'); // 진행 상태 시리얼 출력

          // 네이버 모바일 미세먼지/초미세먼지 데이터 추출용 키워드
          String keyword1 = ">미세</span> <span class=\"num\">";
          String keyword2 = ">초미세</span> <span class=\"num\">";

          // 두 키워드가 한 라인에 모두 존재하는지 확인 (안정성)
          if (line.indexOf(keyword1) >= 0 && line.indexOf(keyword2) >= 0) {
            // [PM10] 미세먼지 수치 추출 (키워드 이후 4자리 추출 후 변환)
            int k1Pos = line.indexOf(keyword1);
            dust_10_num = line.substring(k1Pos + keyword1.length(), k1Pos + keyword1.length() + 4).toInt();

            // [PM2.5] 초미세먼지 수치 추출
            int k2Pos = line.indexOf(keyword2);
            dust_2_5_num = line.substring(k2Pos + keyword2.length(), k2Pos + keyword2.length() + 4).toInt();
            
            Serial.printf("\n[SUCCESS] dust 10um: %d\n", dust_10_num);
            Serial.printf("[SUCCESS] dust 2.5um: %d\n", dust_2_5_num);
            success = true;
            break; // 데이터 발견 시 즉시 종료
          }
          startTime = millis(); // 데이터 수신 시 타임아웃 갱신
        }
        vTaskDelay(1); // 시스템에 컨트롤 양도
      }
      if (success) Serial.println();
    }
    https.end();

    if (success) return;
    
    Serial.println("[ERROR] Dust: Fail to parse, retrying...");
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void get_weather() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  for (int retry = 0; retry < 3; retry++) {
    bool success = false;
    weather_host = "https://www.weather.go.kr/w/wnuri-fct2021/main/digital-forecast.do?code=" + weather_location_code;
    
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(5000);

    HTTPClient https;
    https.begin(client, weather_host);
    https.setTimeout(5000);
    https.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36");

    Serial.printf("\n[HTTP] get Weather (Attempt %d/3)...\n", retry + 1);
    int httpCode = https.GET();
    if (httpCode == 200) {
      int idx = 0;
      WiFiClient *stream = https.getStreamPtr();
      unsigned long startTime = millis();
      
      const String announcement_keyword = "3시간간격예보(현재부터 +일까지) : <span>";
      const String time_keyword = "hid\">시각: </span><span>";
      const String type_keyword = "hid\">날씨: </span><span class=\"wic DB";
      const String temp_keyword = "hid\">기온 : </span><span class=\"hid feel\">";

      while (https.connected() && (millis() - startTime < 5000)) {
        if (stream->available()) {
          String line = stream->readStringUntil('\n'); // 라인 단위로 읽기
          
          // 1. 발표 시각 추출
          if (line.indexOf(announcement_keyword) >= 0) {
            announcement_time = line.substring(line.indexOf(announcement_keyword) + announcement_keyword.length(), line.indexOf(announcement_keyword) + announcement_keyword.length() + 17);
          }
          // 2. 예보 시각 추출
          if (line.indexOf(time_keyword) >= 0) {
            forecast[idx].time = line.substring(line.indexOf(time_keyword) + time_keyword.length(), line.indexOf(time_keyword) + time_keyword.length() + 2).toInt();
          } 
          // 3. 날씨 아이콘 타입 추출
          else if (line.indexOf(type_keyword) >= 0) {
            forecast[idx].type = line.substring(line.indexOf(type_keyword) + type_keyword.length(), line.indexOf(type_keyword) + type_keyword.length() + 2).toInt();
          } 
          // 4. 기온 값 추출
          else if (line.indexOf(temp_keyword) >= 0) {
            forecast[idx].temp = line.substring(line.indexOf(temp_keyword) + temp_keyword.length(), line.indexOf(temp_keyword) + temp_keyword.length() + 3).toInt();
            idx++; // 한 시간대 데이터 수집 완료 시 인덱스 증가
            if (idx >= 3) {
              success = true; // 총 3개 시간대 데이터를 모두 찾으면 성공
              break;
            }
          }
          startTime = millis(); // 데이터 수신 시 타임아웃 갱신
        }
        vTaskDelay(1); // 백그라운드 태스크 양보
      }
    }
    https.end();

    if (success) {
      Serial.print("[SUCCESS] Weather Time: "); Serial.println(announcement_time);
      return;
    }
    Serial.println("[ERROR] Weather: Fail to fetch, retrying...");
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void get_subscribe() {
  if (WiFi.status() != WL_CONNECTED) return;

  for (int retry = 0; retry < 3; retry++) {
    bool success = false;
    String encoded_channel = youtube_channel.startsWith("@") ? "@" + urlEncode(youtube_channel.substring(1)) : urlEncode(youtube_channel);
    String url = "https://www.youtube.com/" + encoded_channel + "?hl=ko";

    Serial.printf("\n[HTTP] get YouTube (Attempt %d/3)...\n", retry + 1);

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(youtube_timeout); 

    HTTPClient https;
    https.begin(client, url);
    https.setTimeout(youtube_timeout);
    https.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36");
    https.addHeader("Accept-Language", "ko-KR");

    int httpCode = https.GET();
    if (httpCode == 200) {
      WiFiClient *stream = https.getStreamPtr();
      unsigned long startTime = millis();
      
      const String kw1 = "{\"content\":\"구독자 ";
      const String kw2 = "명\"},\"accessibilityLabel\":\"구독자";

      while (https.connected() && (millis() - startTime < youtube_timeout)) {
        if (stream->available()) {
          String line = stream->readStringUntil('\n');
          Serial.print('.');

          if (line.indexOf(kw1) >= 0 && line.indexOf(kw2) >= 0) {
            int start = line.indexOf(kw1) + kw1.length();
            int end = line.indexOf(kw2);
            String raw = line.substring(start, end);
            
            subscribe_num = formatSubscribers(raw);
            Serial.print("\n[SUCCESS] YouTube: "); Serial.println(subscribe_num);
            success = true;
            break;
          }
          startTime = millis();
        }
        vTaskDelay(1);
      }
    }
    https.end();

    if (success) {
      Serial.println();
      return;
    }
    Serial.println("\n[ERROR] YouTube: Fail to parse, retrying...");
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void fetchMarketData(const String& name, const String& url, MarketData& data) {
  if (WiFi.status() != WL_CONNECTED) return;

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
        
        Serial.printf("[SUCCESS] %s: %s\n", name.c_str(), data.price.c_str());
        success = true;
      }
    }
    https.end();

    if (success) return;
    
    Serial.printf("[ERROR] %s: Fail to fetch, retrying...\n", name.c_str());
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void get_kospi() { fetchMarketData("KOSPI", "https://polling.finance.naver.com/api/realtime/domestic/index/KOSPI", kospi_data); }
void get_kosdaq() { fetchMarketData("KOSDAQ", "https://polling.finance.naver.com/api/realtime/domestic/index/KOSDAQ", kosdaq_data); }
void get_kpi200() { fetchMarketData("KOSPI200", "https://polling.finance.naver.com/api/realtime/domestic/index/KPI200", kpi200_data); }
void get_futures() { fetchMarketData("FUTURES", "https://polling.finance.naver.com/api/realtime/domestic/index/FUT", futures_data); }
void get_SnP500() { fetchMarketData("S&P 500", "https://polling.finance.naver.com/api/realtime/worldstock/index/.INX", snp500_data); }
void get_nasdaq() { fetchMarketData("NASDAQ", "https://polling.finance.naver.com/api/realtime/worldstock/index/.IXIC", nasdaq_data); }

void get_btc() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  for (int retry = 0; retry < 3; retry++) {
    bool success = false;
    String url = "https://api.binance.com/api/v3/ticker/24hr?symbol=BTCUSDT";
    
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(5000);

    HTTPClient https;
    https.begin(client, url);
    https.setTimeout(5000);

    Serial.printf("\n[HTTP] get BTC (Attempt %d/3)...\n", retry + 1);
    int httpCode = https.GET();
    if (httpCode == 200) {
      String payload = https.getString();
      
      // 1. 현재 가격(lastPrice) 추출
      int price_idx = payload.indexOf("\"lastPrice\":\"");
      if (price_idx != -1) {
        int price_end = payload.indexOf("\"", price_idx + 13);
        String rawPrice = payload.substring(price_idx + 13, price_end);
        
        // 소수점 이하는 버리고 정수 부분만 사용 (가독성)
        int dot_idx = rawPrice.indexOf(".");
        if (dot_idx != -1) rawPrice = rawPrice.substring(0, dot_idx);
        btc_data.price = addCommas(rawPrice);

        // 2. 24시간 변동폭(priceChange) 추출
        int change_idx = payload.indexOf("\"priceChange\":\"");
        if (change_idx != -1) {
          int change_end = payload.indexOf("\"", change_idx + 15);
          String rawChange = payload.substring(change_idx + 15, change_end);
          int dot_c_idx = rawChange.indexOf(".");
          if (dot_c_idx != -1) rawChange = rawChange.substring(0, dot_c_idx);
          
          float chgVal = rawChange.toFloat();
          if (chgVal > 0) btc_data.change = "+" + addCommas(rawChange); 
          else if (chgVal < 0) {
            rawChange.replace("-", ""); 
            btc_data.change = "-" + addCommas(rawChange);
          } else btc_data.change = "0";
        }

        // 3. 24시간 변동률(priceChangePercent) 추출
        int percent_idx = payload.indexOf("\"priceChangePercent\":\"");
        if (percent_idx != -1) {
          int percent_end = payload.indexOf("\"", percent_idx + 22);
          float percent = payload.substring(percent_idx + 22, percent_end).toFloat();
          if (percent > 0) btc_data.percent = "+" + String(percent, 2) + "%";
          else if (percent < 0) btc_data.percent = String(percent, 2) + "%";
          else btc_data.percent = "0.00%";
        }
        Serial.printf("[SUCCESS] BTC: %s\n", btc_data.price.c_str());
        success = true;
      }
    }
    https.end();

    if (success) return;
    Serial.println("[ERROR] BTC: Fail to fetch, retrying...");
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void get_usdkrw() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  for (int retry = 0; retry < 3; retry++) {
    bool success = false;
    String url = "https://query1.finance.yahoo.com/v8/finance/chart/KRW=X?interval=1d";
    
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(5000);

    HTTPClient https;
    https.begin(client, url);
    https.setTimeout(5000);
    https.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36");

    Serial.printf("\n[HTTP] get USD/KRW (Attempt %d/3)...\n", retry + 1);
    int httpCode = https.GET();
    if (httpCode == 200) {
      String payload = https.getString();
      
      // 1. 현재 실시간 환율(regularMarketPrice) 추출
      int price_idx = payload.indexOf("\"regularMarketPrice\":");
      float currentPrice = 0;
      if (price_idx != -1) {
        int price_end = payload.indexOf(",", price_idx);
        String rawPrice = payload.substring(price_idx + 21, price_end);
        currentPrice = rawPrice.toFloat();
        usdkrw_data.price = String(currentPrice, 2); 

        // 2. 전일 종가(chartPreviousClose) 추출 (등락폭 계산용)
        int prev_idx = payload.indexOf("\"chartPreviousClose\":");
        float prevPrice = 0;
        if (prev_idx != -1) {
          int prev_end = payload.indexOf(",", prev_idx);
          String rawPrev = payload.substring(prev_idx + 21, prev_end);
          prevPrice = rawPrev.toFloat();
        }

        // 3. 등락폭 및 등락률 계산
        if (currentPrice > 0 && prevPrice > 0) {
          float changeVal = currentPrice - prevPrice;
          float percentVal = (changeVal / prevPrice) * 100.0;
          
          if (changeVal > 0) {
            usdkrw_data.change = "+" + String(changeVal, 2);
            usdkrw_data.percent = "+" + String(percentVal, 2) + "%";
          } else if (changeVal < 0) {
            usdkrw_data.change = String(changeVal, 2);
            usdkrw_data.percent = String(percentVal, 2) + "%";
          } else {
            usdkrw_data.change = "0.00";
            usdkrw_data.percent = "0.00%";
          }
        }
        Serial.printf("[SUCCESS] USD/KRW: %s\n", usdkrw_data.price.c_str());
        success = true;
      }
    }
    https.end();

    if (success) return;
    Serial.println("[ERROR] USD/KRW: Fail to fetch, retrying...");
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// --- 가시성 판단 로직 ---

bool isWidgetActive(WidgetType type) {
  for (int i = 0; i < MAX_DATA_PAGE * 4; i++) {
    if (SCREEN_MAP[i] == type) return true;
  }
  return false;
}

bool isWidgetVisible(WidgetType type) {
  if (current_page == MAX_PAGE) return false; 
  int start_idx = (current_page - 1) * 4;
  for (int i = start_idx; i < start_idx + 4; i++) {
    if (SCREEN_MAP[i] == type) return true;
  }
  return false;
}

// --- 시장 개패장 판단 로직 ---

bool isDomesticMarketOpen() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return true; // 실패 시 안전하게 업데이트 시도
  int wday = timeinfo.tm_wday;   
  int hour = timeinfo.tm_hour;
  int min  = timeinfo.tm_min;

  // 국내 증시 (KOSPI, KOSDAQ 등): 월~금 08:30 - 16:00 (장전 시간외 ~ 장후 정리 시간)
  if (wday >= 1 && wday <= 5) {
    if (hour >= 9 && hour < 16) return true;
    if (hour == 8 && min >= 30) return true;
    if (hour == 16 && min <= 10) return true;
  }
  return false;
}

bool isUsMarketOpen() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return true;
  int wday = timeinfo.tm_wday;   
  int hour = timeinfo.tm_hour;   

  // 미국 증시 (한국 시간 기준)
  // 주말 처리: 토요일 오전 8시 이후부터 일요일 전체는 휴장
  if (wday == 0) return false; 
  if (wday == 6 && hour >= 8) return false; 
  if (wday == 1 && hour < 17) return false; // 월요일 오후 5시 이전 휴장 (프리마켓 시작 전)

  // 그 외 시간 (야간 및 새벽)
  if (hour >= 17 || hour < 8) return true;
  return false;
}

// --- 🚀 백그라운드 데이터 수집 태스크 ---
void dataTask(void *pvParameters) {
  static unsigned long last_widget_fetches[MAX_WIDGET_RECORDS] = {0};

  for (;;) {
    unsigned long now = millis();
    if (WiFi.status() == WL_CONNECTED) {
      uint16_t processed_mask = 0;

      // --- 강제 업데이트 플래그 확인 ---
      portENTER_CRITICAL(&updateMux);
      bool local_force = force_update;
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
            if (type == W_NONE || type == W_TIME || type == W_CALENDAR) continue;
            for (int w = 0; w < WIDGET_COUNT; w++) {
              if (widgets_info[w].type == type) {
                *widgets_info[w].is_updating = true;   // OLED에 "Updating..." 표시
                widgets_info[w].fetch();
                *widgets_info[w].is_updating = false;  // 완료 후 해제
                last_widget_fetches[w] = now;
                portENTER_CRITICAL(&updateMux);
                update_flag |= widgets_info[w].flag_mask;
                portEXIT_CRITICAL(&updateMux);
                processed_mask |= widgets_info[w].flag_mask;
                break;
              }
            }
          }
        }

        // Pass 2: 나머지 모든 활성화된 위젯들 배경에서 조용히 수집 (시장 시간 무관)
        for (int w = 0; w < WIDGET_COUNT; w++) {
          if (!(processed_mask & widgets_info[w].flag_mask) && isWidgetActive(widgets_info[w].type)) {
            widgets_info[w].fetch();
            last_widget_fetches[w] = now;
            portENTER_CRITICAL(&updateMux);
            update_flag |= widgets_info[w].flag_mask;
            portEXIT_CRITICAL(&updateMux);
            processed_mask |= widgets_info[w].flag_mask;
          }
        }
        is_initial = false;
      }

      // --- 정기 주기별 개별 업데이트 체크 (현재 페이지 위젯 대상) ---
      // 주의: 강제 업데이트 시에는 이미 위에서 처리되었으므로 processed_mask에 의해 스킵됨
      if (current_page <= MAX_DATA_PAGE) {
        int start_idx = (current_page - 1) * 4;
        for (int i = start_idx; i < start_idx + 4; i++) {
          WidgetType type = SCREEN_MAP[i];
          if (type == W_NONE || type == W_TIME || type == W_CALENDAR) continue;

          for (int w = 0; w < WIDGET_COUNT; w++) {
            if (widgets_info[w].type == type) {
              if (processed_mask & widgets_info[w].flag_mask) break;

              unsigned long interval = widgets_info[w].is_finance ? INTERVAL_FINANCE : INTERVAL_SLOW;
              bool needs_update = (now - last_widget_fetches[w] >= interval);

              if (needs_update) {
                // 시장 개폐 시간에 따른 예외 처리 (정기 업데이트 시에만 적용)
                if ((type == W_KOSPI || type == W_KOSDAQ || type == W_KPI200 || type == W_FUTURES) && !isDomesticMarketOpen()) needs_update = false;
                if ((type == W_SNP500 || type == W_NASDAQ || type == W_USDKRW) && !isUsMarketOpen()) needs_update = false;
              }

              if (needs_update) {
                *widgets_info[w].is_updating = true;
                widgets_info[w].fetch();
                last_widget_fetches[w] = now;
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
    }
    portENTER_CRITICAL(&updateMux);  // force_update 쓰기 보호
    force_update = false;
    portEXIT_CRITICAL(&updateMux);
    is_waiting = true;
    vTaskDelay(pdMS_TO_TICKS(1000));
    is_waiting = false;
  }
}
