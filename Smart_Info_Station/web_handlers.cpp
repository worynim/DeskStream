#include "web_handlers.h"
#include "web_index.h"
#include "data_manager.h"
#include "youtube.h"
#include "dust.h"
#include "weather.h"

void handleRoot() {
  String html;
  html.reserve(10000);

  // 헤더 및 스타일 (static block)
  html = INDEX_HTML_START;

  html += "<div class=\"section\">";
  html += "<h3>📺 스마트 화면 배치 (Slot 1~" + String(MAX_DATA_PAGE * 4) + ")</h3>";
  html += "<p>물리적인 4개의 화면에 표시될 위젯을 페이지별로 설정하세요.</p>";

  const char *w_names[] = { "달력", "시계", "날씨", "미세먼지", "유튜브", "코스피", "코스닥", "코스피200", "선물지수", "S&P 500", "나스닥", "비트코인", "환율", "사용 안 함" };
  const char *pageTitles[MAX_DATA_PAGE] = {
    "[PAGE 1] 기본 화면",
    "[PAGE 2] 버튼 클릭 시 전환 화면",
    "[PAGE 3] 추가/심화 정보"
  };

  // 데이터 페이지별 위젯 선택 셀렉터 생성 (MAX_DATA_PAGE 기반 루프로 중복 제거)
  for (int page = 0; page < MAX_DATA_PAGE; page++) {
    int start_idx = page * 4;
    html += "<div class=\"page-title\">" + String(pageTitles[page]) + "</div>";
    html += "<div class=\"flex-container\">";
    for (int i = start_idx; i < start_idx + 4; i++) {
      int screenNum = (i - start_idx) + 1;  // 페이지 내 화면 번호 (1~4)
      html += "<div class=\"flex-item\">";
      html += "<span class=\"label\">Screen " + String(screenNum) + "</span>";
      html += "<select name=\"sm" + String(i) + "\">";
      for (int j = 0; j <= MAX_WIDGET_TYPE; j++) {
        html += "<option value=\"" + String(j) + "\"";
        if ((int)SCREEN_MAP[i] == j) html += " selected";
        html += ">" + String(w_names[j]) + "</option>";
      }
      html += "</select></div>";
    }
    html += "</div><br>";
  }

  // 4페이지 안내 (고정형)
  html += "<div class=\"page-title\">[PAGE 4] 시스템 정보 (IP 주소)</div>";
  html += "<p style=\"color: #666; font-size: 0.9em; margin-bottom: 20px;\">* 4페이지는 설정 웹서버 접속 주소(IP)를 4개의 화면에 크게 나누어 표시하는 시스템 고정 페이지입니다.</p>";

  // 5페이지 안내 (도움말)
  html += "<div class=\"page-title\">[PAGE 5] 버튼 기능 도움말</div>";
  html += "<p style=\"color: #666; font-size: 0.9em; margin-bottom: 20px;\">* 5페이지는 하드웨어 버튼(BTN 1~4)의 짧게/길게 누름 기능을 화면에 안내하는 페이지입니다.</p>";
  
  // 자동 페이지 루핑 설정 추가 (카드 내부 하단)
  html += "<br><div class=\"page-title\" style=\"background: #e1f5fe; color: #01579b;\">🔄 자동 페이지 루핑 (Loop Mode)</div>";
  html += "<p style=\"font-size: 0.85em; color: #666;\">1, 2, 3페이지를 5초마다 자동으로 순환하여 보여줍니다.</p>";
  html += "<select name=\"loop_mode\" style=\"width: 100%; border: 1px solid #81d4fa;\">";
  html += "<option value=\"0\"" + String(!isLoopingMode ? " selected" : "") + ">OFF (수동 전환)</option>";
  html += "<option value=\"1\"" + String(isLoopingMode ? " selected" : "") + ">ON (자동 순환)</option>";
  html += "</select>";
  
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
  if (server.hasArg("loop_mode")) {
    bool new_loop = server.arg("loop_mode").toInt() == 1;
    if (new_loop != isLoopingMode) {
        isLoopingMode = new_loop;
        changed = true;
    }
  }

  // 1~(MAX_DATA_PAGE*4)번 슬롯 스크린맵 파라미터 처리
  for (int i = 0; i < MAX_DATA_PAGE * 4; i++) {
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

  if (changed) {
    preferences.begin("settings", false);
    preferences.putString("channel", youtube_channel);
    preferences.putString("dust_loc", dust_location);
    preferences.putString("weather_code", weather_location_code);
    preferences.putInt("tz_offset", timezone_offset);
    preferences.putBool("loop", isLoopingMode);
    preferences.end();

    preferences.begin("screen_map", false);
    for (int i = 0; i < MAX_DATA_PAGE * 4; i++) {
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

  portENTER_CRITICAL(&updateMux);  // force_update 쓰기 보호
  force_update = true;             // 설정 저장 직후 백그라운드 태스크에 즉시 갱신 명령 전달
  portEXIT_CRITICAL(&updateMux);
  redraw_current_page();           // 저장 즉시 현재 페이지 화면 레이아웃 갱신 반영

  server.send(200, "text/html", SUCCESS_HTML);
}
