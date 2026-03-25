#ifndef BTC_H
#define BTC_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// MarketData 구조체는 market.h에서 정의됨 (Smart_Info_Station.ino 포함 순서에 의해 사전 정의)
MarketData btc_data;  // 비트코인 시세 데이터
const unsigned long btc_timeout = 5000; // 수집 및 파싱 타임아웃 (ms)

/**
 * 천 단위 콤마 삽입 유틸리티 함수
 * @param numStr 숫자 문자열 (부호 포함 가능)
 * @return 천 단위 콤마가 삽입된 문자열
 */
String addCommas(String numStr) {
  int decPos = numStr.indexOf('.');
  String intPart = (decPos != -1) ? numStr.substring(0, decPos) : numStr;
  String decPart = (decPos != -1) ? numStr.substring(decPos) : "";
  
  bool isNegative = false;
  if (intPart.charAt(0) == '-') {
    isNegative = true;
    intPart.remove(0, 1);
  } else if (intPart.charAt(0) == '+') {
    intPart.remove(0, 1);
  }

  String result = "";
  int len = intPart.length();
  for (int i = 0; i < len; i++) {
    if (i > 0 && (len - i) % 3 == 0) {
      result += ",";
    }
    result += intPart.charAt(i);
  }
  
  return (isNegative ? "-" : "") + result + decPart;
}

/**
 * Binance API에서 비트코인(BTCUSDT) 시세를 가져오는 함수
 * 가격, 24시간 등락폭, 등락률을 파싱하여 btc_data에 저장합니다.
 */
void get_btc() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  for (int retry = 0; retry < 3; retry++) {
    bool success = false;
    String url = "https://api.binance.com/api/v3/ticker/24hr?symbol=BTCUSDT";
    
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(btc_timeout);

    HTTPClient https;
    https.begin(client, url);
    https.setTimeout(btc_timeout);

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
          if (chgVal > 0) btc_data.change = "+" + addCommas(rawChange); // 전일 대비 상승 시 + 기호 추가
          else if (chgVal < 0) {
            rawChange.replace("-", ""); // 중복 부호 방지를 위해 기존 - 제거 후 재조합
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

#endif
