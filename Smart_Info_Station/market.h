#ifndef MARKET_H
#define MARKET_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

/**
 * 시장 데이터를 담는 공통 구조체
 * 가격, 등락폭, 등락률을 하나의 단위로 관리합니다.
 */
struct MarketData {
  String price   = "0.00";
  String change  = "0.00";
  String percent = "0.00";
};

const unsigned long market_timeout = 5000; // 수집 및 파싱 타임아웃 (ms)

// === 네이버 금융 API 기반 지수 데이터 (KOSPI, KOSDAQ, S&P 500, NASDAQ) ===
MarketData kospi_data;
MarketData kosdaq_data;
MarketData snp500_data;
MarketData nasdaq_data;
MarketData kpi200_data;
MarketData futures_data;

/**
 * 네이버 금융 API에서 지수 데이터를 가져오는 범용 함수
 * 국내 지수(KOSPI, KOSDAQ, 선물)와 해외 지수(S&P 500, NASDAQ) 모두
 * 동일한 JSON 구조를 사용하므로 하나의 함수로 통합 처리합니다.
 *
 * @param name  시리얼 로그에 표시할 지수 이름 (예: "KOSPI")
 * @param url   네이버 금융 API URL
 * @param data  파싱 결과를 저장할 MarketData 구조체 참조
 */
void fetchMarketData(const String& name, const String& url, MarketData& data) {
  if (WiFi.status() != WL_CONNECTED) return;

  for (int retry = 0; retry < 3; retry++) {
    bool success = false;
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(market_timeout);

    HTTPClient https;
    https.begin(client, url);
    https.setTimeout(market_timeout);
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


// === 개별 호출 래퍼 함수 (기존 코드 호출부 호환 유지) ===

void get_kospi() {
  fetchMarketData("KOSPI", "https://polling.finance.naver.com/api/realtime/domestic/index/KOSPI", kospi_data);
}

void get_kosdaq() {
  fetchMarketData("KOSDAQ", "https://polling.finance.naver.com/api/realtime/domestic/index/KOSDAQ", kosdaq_data);
}

void get_kpi200() {
  fetchMarketData("KOSPI200", "https://polling.finance.naver.com/api/realtime/domestic/index/KPI200", kpi200_data);
}

void get_futures() {
  fetchMarketData("FUTURES", "https://polling.finance.naver.com/api/realtime/domestic/index/FUT", futures_data);
}

void get_SnP500() {
  fetchMarketData("S&P 500", "https://polling.finance.naver.com/api/realtime/worldstock/index/.INX", snp500_data);
}

void get_nasdaq() {
  fetchMarketData("NASDAQ", "https://polling.finance.naver.com/api/realtime/worldstock/index/.IXIC", nasdaq_data);
}

#endif
