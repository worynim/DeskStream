#ifndef DUST_H
#define DUST_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <UrlEncode.h>

/**
 * 전역 변수 설정
 */
String dust_location; 
String dust_host;     
int dust_10_num = -1;
int dust_2_5_num = -1;
const unsigned long dust_timeout = 5000; // 수집 및 파싱 타임아웃 (ms)

/**
 * 네이버 모바일 검색 결과에서 미세먼지 정보를 가져오는 함수 (v1.7.7)
 * 최신 HTML 구조에 맞춘 키워드 스트리밍 파싱 방식을 사용합니다.
 */
void get_dust() {
  if (WiFi.status() != WL_CONNECTED) return;

  for (int retry = 0; retry < 3; retry++) {
    bool success = false;
    String query = urlEncode(dust_location) + urlEncode(" 미세먼지");
    dust_host = "https://m.search.naver.com/search.naver?where=m&query=" + query;

    Serial.printf("\n[HTTP] get dust (Attempt %d/3)...\n", retry + 1);

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(dust_timeout);

    HTTPClient https;
    https.begin(client, dust_host);
    https.setTimeout(dust_timeout);
    https.setUserAgent("Mozilla/5.0 (iPhone; CPU iPhone OS 16_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.6 Mobile/15E148 Safari/604.1");

    int httpCode = https.GET();
    if (httpCode == 200) {
      WiFiClient *stream = https.getStreamPtr(); // 대용량 HTML 처리를 위한 스트림 획득
      unsigned long startTime = millis();

      // 서버 연결 유지 시간 동안 스트리밍 파싱 수행
      while (https.connected() && (millis() - startTime < dust_timeout)) {
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

#endif