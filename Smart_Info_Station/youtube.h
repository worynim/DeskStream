#ifndef YOUTUBE_H
#define YOUTUBE_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <UrlEncode.h>

// 전역 변수 설정
String youtube_channel; 
String subscribe_num = "";
const unsigned long youtube_timeout = 15000; // 수집 및 파싱 타임아웃 (ms)

/**
 * 한글 단위(천, 만, 억)가 포함된 구독자 수 문자열을 k, M, B 단위의 영문 문자열로 변환합니다.
 * @param subscribers 원본 구독자 수 문자열 (예: "15.4만", "1.2억")
 * @return 포맷팅된 영문 단위 문자열 (예: "154k", "120M")
 */
String formatSubscribers(String subscribers) {
  // 1. 숫자 추출을 방해하는 한글 수식어 제거
  subscribers.replace("구독자 ", "");
  subscribers.replace("팔로워 ", "");
  subscribers.replace("명", "");
  subscribers.trim();

  // 2. 한글 단위 위치 찾기
  int chon_idx = subscribers.indexOf("천");
  int man_idx = subscribers.indexOf("만");
  int eok_idx = subscribers.indexOf("억");

  double number = 0;

  // 3. 각 단위별 숫자 환산 (천=10^3, 만=10^4, 억=10^8)
  if (chon_idx != -1) {
    String numPart = subscribers.substring(0, chon_idx);
    number = numPart.toFloat() * 1000;
  } else if (man_idx != -1) {
    String numPart = subscribers.substring(0, man_idx);
    number = numPart.toFloat() * 10000;
  } else if (eok_idx != -1) {
    String numPart = subscribers.substring(0, eok_idx);
    number = numPart.toFloat() * 100000000;
  } else {
    // 한글 단위가 없는 경우 순수 숫자만 추출
    String result = "";
    for(int i=0; i<subscribers.length(); i++) {
      if(isDigit(subscribers[i]) || subscribers[i] == '.') result += subscribers[i];
    }
    return result;
  }

  // 4. 국제 표준 영문 단위(k, M, B)로 재포맷팅
  String formattedString = "";
  if (number >= 1000000000) {
    formattedString = String(number / 1000000000.0, 2) + "B"; // Billion
  } else if (number >= 1000000) {
    formattedString = String(number / 1000000.0, 2) + "M";    // Million
  } else if (number >= 1000) {
    formattedString = String(number / 1000.0, 2) + "k";      // kilo
  } else {
    formattedString = String((long)number);
  }

  // 5. 소수점 처리 (불필요한 .0 또는 0 제거)
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
 * 유튜브 데스크톱 페이지에서 구독자 정보를 가져오는 함수 (v1.7.1 복구)
 * 데이터량이 많은 데스크톱 구조를 고려하여 넉넉한 타임아웃과 안정적인 키워드 파싱을 사용합니다.
 */
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

#endif
