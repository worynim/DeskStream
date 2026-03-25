#ifndef WEATHER_H
#define WEATHER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
String weather_location_code; // 웹 설정에서 입력받는 법정동 코드
String weather_host;          // 날씨 정보를 요청할 조합된 URL
const unsigned long weather_timeout = 5000; // 수집 및 파싱 타임아웃 (ms)

// 기상청 단기예보 HTML에서 찾을 핵심 키워드 목록
const String announcement_keyword = "3시간간격예보(현재부터 +일까지) : <span>"; // 예보 발표 시간
const String time_keyword = "hid\">시각: </span><span>";                        // 예보 시간 (시각)
const String type_keyword = "hid\">날씨: </span><span class=\"wic DB";          // 날씨 아이콘 타입
const String temp_keyword = "hid\">기온 : </span><span class=\"hid feel\">";    // 예상 기온

String announcement_time; // 파싱된 발표 시간을 저장

// 개별 시간대별 예보 데이터를 담기 위한 구조체
struct Forecast {
  int time; // 예보 시각 (시 단위, 0~23)
  int type; // 날씨 상태 아이콘 번호 (기상청 고유 코드)
  int temp; // 해당 시각의 예상 기온 (체감 온도)
};

Forecast forecast[3]; // OLED 화면 1칸에 표시할 총 3개 시간대(3시간 간격)의 예보 배열

// 날씨 정보 읽어오기 함수 (v1.7.x 가시성 기반 최적화 적용)
void get_weather() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  for (int retry = 0; retry < 3; retry++) {
    bool success = false;
    weather_host = "https://www.weather.go.kr/w/wnuri-fct2021/main/digital-forecast.do?code=" + weather_location_code;
    
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(weather_timeout);

    HTTPClient https;
    https.begin(client, weather_host);
    https.setTimeout(weather_timeout);
    https.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36");

    Serial.printf("\n[HTTP] get Weather (Attempt %d/3)...\n", retry + 1);
    int httpCode = https.GET();
    if (httpCode == 200) {
      int idx = 0;
      WiFiClient *stream = https.getStreamPtr();
      unsigned long startTime = millis();

      while (https.connected() && (millis() - startTime < weather_timeout)) {
        if (stream->available()) {
          String line = stream->readStringUntil('\n');
          
          // 1. 발표 시각 추출 (예: "03월 12일 20시 발표")
          if (line.indexOf(announcement_keyword) >= 0) {
            announcement_time = line.substring(line.indexOf(announcement_keyword) + announcement_keyword.length(), line.indexOf(announcement_keyword) + announcement_keyword.length() + 17);
          }
          // 2. 예보 시각 추출 (HIDDEN 태그 내 시각 정보)
          if (line.indexOf(time_keyword) >= 0) {
            forecast[idx].time = line.substring(line.indexOf(time_keyword) + time_keyword.length(), line.indexOf(time_keyword) + time_keyword.length() + 2).toInt();
          } 
          // 3. 날씨 아이콘 타입 추출 (wic DB 뒤의 숫자를 통해 날씨 상태 판별)
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
        vTaskDelay(1); // 백그라운드 태스크 양보를 위한 최소 대기
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

#endif