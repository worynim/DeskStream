#ifndef USDKRW_H
#define USDKRW_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// MarketData 구조체는 market.h에서 정의됨 (Smart_Info_Station.ino 포함 순서에 의해 사전 정의)
MarketData usdkrw_data;  // USD/KRW 환율 데이터
const unsigned long usdkrw_timeout = 5000; // 수집 및 파싱 타임아웃 (ms)

/**
 * Yahoo Finance API에서 USD/KRW 환율을 가져오는 함수
 * 현재가, 전일 대비 등락폭, 등락률을 계산하여 usdkrw_data에 저장합니다.
 */
void get_usdkrw() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  for (int retry = 0; retry < 3; retry++) {
    bool success = false;
    String url = "https://query1.finance.yahoo.com/v8/finance/chart/KRW=X?interval=1d";
    
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(usdkrw_timeout);

    HTTPClient https;
    https.begin(client, url);
    https.setTimeout(usdkrw_timeout);
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
        usdkrw_data.price = String(currentPrice, 2); // 소수점 2자리까지 저장

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

#endif
