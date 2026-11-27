// worynim@gmail.com
/**
 * @file web_handlers.cpp
 * @brief 웹 서버 요청 핸들러 및 API 라우팅 구현
 * @details Wi-Fi 연결, 지연 저장 설정 및 실시간 모니터링을 위한 백엔드 로직 처리
 */
#include "web_handlers.h"
#include "web_index.h"
#include "data_manager.h"
#include "youtube.h"
#include "dust.h"
#include "weather.h"
#include "slot_city.h"
#include "stock.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <UrlEncode.h>
#include <DeskStreamCore.h>
extern ds::TimeSync timeSync;   // Smart_Info_Station.ino에서 정의 (NTP 재동기화 감독)

/** 사용자 입력값을 HTML 속성에 넣을 수 있게 특수 문자를 이스케이프한다 */
static String htmlEscape(const String &s) {
  String out;
  out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '&':  out += "&amp;"; break;
      case '<':  out += "&lt;"; break;
      case '>':  out += "&gt;"; break;
      case '"':  out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default:   out += c; break;
    }
  }
  return out;
}

/** 문자열을 스크립트 안의 "..." 값으로 — 따옴표·역슬래시·< 이스케이프 */
static String jsString(const String &s) {
  String out = "\"";
  for (size_t k = 0; k < s.length(); k++) {
    char c = s[k];
    if (c == '"' || c == '\\') out += '\\';
    if (c == '<') { out += "\\u003c"; continue; }
    out += c;
  }
  return out + "\"";
}

/**
 * 웹 페이지가 도시 검색으로 채운 이름(영문)·좌표를 읽는다 — 폼 이름은 <p>_name, <p>_lat, <p>_lon.
 * @return 이름이 있고 이름 40바이트 이하·좌표가 범위 안이면 true (이름이 비어 있으면 바꾸지 않는다는 뜻으로 false)
 */
static bool readCityArgs(const String &p, String &name, float &lat, float &lon) {
  name = server.arg(p + "_name");
  if (name.length() == 0) return false;
  lat = server.arg(p + "_lat").toFloat();
  lon = server.arg(p + "_lon").toFloat();
  if (name.length() <= 40 && lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) return true;
  Serial.printf("[WEB] Rejected %s city (name len %d, lat %.4f, lon %.4f)\n", p.c_str(), name.length(), lat, lon);
  return false;
}

void handleRoot() {
  String html;
  html.reserve(10000);

  // 헤더 및 스타일 (static block)
  html = INDEX_HTML_START;

  html += "<div class=\"section\">";
  // 영문 문구는 data-en 속성에 둔다 — 브라우저가 언어 선택에 따라 바꿔 보인다 (web_index.h applyLang)
  const String slots = String(MAX_DATA_PAGE * 4);
  html += "<h3 data-en=\"📺 Screen Layout (Slot 1~" + slots + ")\">📺 스마트 화면 배치 (Slot 1~" + slots + ")</h3>";
  html += "<p data-en=\"Choose the widget shown on each of the 4 physical screens, page by page.\">물리적인 4개의 화면에 표시될 위젯을 페이지별로 설정하세요.</p>";

  // WidgetType 값 순서 (config.h)
  const char *w_names[] = { "달력", "한국 시계", "동네 날씨", "미세먼지", "유튜브", "코스피", "코스닥", "코스피200", "선물지수", "S&P 500", "나스닥", "비트코인", "환율(달러)", "사용 안 함",
                            "해외 날씨", "세계 시계", "환율(엔·유로·위안)", "이더리움", "D-day", "일출·일몰", "주간 날씨", "기온 그래프", "관심 종목" };
  const char *w_names_en[] = { "Calendar", "Korea Clock", "Local Weather", "Fine Dust", "YouTube", "KOSPI", "KOSDAQ", "KOSPI 200", "Futures", "S&amp;P 500", "NASDAQ", "Bitcoin", "USD/KRW", "None",
                               "Intl. Weather", "World Clock", "FX (JPY·EUR·CNY)", "Ethereum", "D-day", "Sunrise/Sunset", "Weekly Weather", "Temp Graph", "Stock" };
  // 목록 표시 순서 — 비슷한 위젯끼리 모으고 "사용 안 함"(13)은 맨 끝
  const int w_order[] = { W_CALENDAR, W_TIME, W_WORLD_CLOCK, W_DDAY, W_WEATHER, W_INTL_WEATHER, W_WEEKLY, W_TEMP_GRAPH, W_SUN, W_DUST, W_YOUTUBE,
                          W_KOSPI, W_KOSDAQ, W_KPI200, W_FUTURES, W_SNP500, W_NASDAQ, W_STOCK, W_BTC, W_ETH, W_USDKRW, W_FX, W_NONE };
  const char *pageTitles[MAX_DATA_PAGE] = {
    "[PAGE 1] 기본 화면",
    "[PAGE 2] 버튼 클릭 시 전환 화면",
    "[PAGE 3] 추가/심화 정보"
  };
  const char *pageTitlesEn[MAX_DATA_PAGE] = {
    "[PAGE 1] Main",
    "[PAGE 2] Shown on button press",
    "[PAGE 3] Extra info"
  };

  // 데이터 페이지별 위젯 선택 셀렉터 생성 (MAX_DATA_PAGE 기반 루프로 중복 제거)
  for (int page = 0; page < MAX_DATA_PAGE; page++) {
    int start_idx = page * 4;
    html += "<div class=\"page-title\" data-en=\"" + String(pageTitlesEn[page]) + "\">" + String(pageTitles[page]) + "</div>";
    html += "<div class=\"flex-container\">";
    for (int i = start_idx; i < start_idx + 4; i++) {
      int screenNum = (i - start_idx) + 1;  // 페이지 내 화면 번호 (1~4)
      html += "<div class=\"flex-item\">";
      html += "<span class=\"label\">Screen " + String(screenNum) + "</span>";
      html += "<select name=\"sm" + String(i) + "\">";
      for (int j : w_order) {
        html += "<option value=\"" + String(j) + "\"";
        if ((int)SCREEN_MAP[i] == j) html += " selected";
        html += " data-en=\"" + String(w_names_en[j]) + "\">" + String(w_names[j]) + "</option>";
      }
      html += "</select></div>";
    }
    html += "</div><br>";
  }

  // 4페이지 안내 (고정형)
  html += "<div class=\"page-title\" data-en=\"[PAGE 4] System info (IP address)\">[PAGE 4] 시스템 정보 (IP 주소)</div>";
  html += "<p style=\"color: #666; font-size: 0.9em; margin-bottom: 20px;\" data-en=\"* Page 4 is a fixed system page that shows this settings server's address (IP) in large digits across the 4 screens.\">* 4페이지는 설정 웹서버 접속 주소(IP)를 4개의 화면에 크게 나누어 표시하는 시스템 고정 페이지입니다.</p>";

  // 5페이지 안내 (도움말)
  html += "<div class=\"page-title\" data-en=\"[PAGE 5] Button help\">[PAGE 5] 버튼 기능 도움말</div>";
  html += "<p style=\"color: #666; font-size: 0.9em; margin-bottom: 20px;\" data-en=\"* Page 5 explains the short/long press functions of the hardware buttons (BTN 1~4).\">* 5페이지는 하드웨어 버튼(BTN 1~4)의 짧게/길게 누름 기능을 화면에 안내하는 페이지입니다.</p>";
  
  // 자동 페이지 루핑 설정 추가 (카드 내부 하단)
  html += "<br><div class=\"page-title\" style=\"background: #e1f5fe; color: #01579b;\" data-en=\"🔄 Auto Page Loop (Loop Mode)\">🔄 자동 페이지 루핑 (Loop Mode)</div>";
  html += "<p style=\"font-size: 0.85em; color: #666;\" data-en=\"Cycles through pages 1, 2 and 3 at the chosen interval.\">1, 2, 3페이지를 정한 간격마다 자동으로 순환하여 보여줍니다.</p>";
  html += "<select name=\"loop_mode\" style=\"width: 100%; border: 1px solid #81d4fa;\">";
  html += "<option value=\"0\"" + String(!isLoopingMode ? " selected" : "") + " data-en=\"OFF (manual)\">OFF (수동 전환)</option>";
  html += "<option value=\"1\"" + String(isLoopingMode ? " selected" : "") + " data-en=\"ON (auto cycle)\">ON (자동 순환)</option>";
  html += "</select>";
  html += "<select name=\"loop_sec\" style=\"width: 100%; border: 1px solid #81d4fa; margin-top: 8px;\">";
  for (int sec : LOOP_SECONDS) {
    html += "<option value=\"" + String(sec) + "\"" + String(sec == loopIntervalSec ? " selected" : "") + " data-en=\"Every " + String(sec) + " s\">" + String(sec) + "초마다 전환</option>";
  }
  html += "</select>";
  
  html += "</div>";

  // 나머지 입력 필드 템플릿 적용
  String settingsHtml = INDEX_HTML_SETTINGS;
  // 설정 칸을 보일 위젯 번호 (화면 배치에 있을 때만 보인다 — web_index.h updateSections)
  settingsHtml.replace("%WID_YOUTUBE%", String(W_YOUTUBE));
  settingsHtml.replace("%WID_DUST%", String(W_DUST));
  settingsHtml.replace("%WID_WEATHER%", String(W_WEATHER));
  settingsHtml.replace("%WID_INTL%", String(W_INTL_WEATHER));
  settingsHtml.replace("%WID_WC%", String(W_WORLD_CLOCK));
  settingsHtml.replace("%WID_SUN%", String(W_SUN));
  settingsHtml.replace("%WID_WEEKLY%", String(W_WEEKLY));
  settingsHtml.replace("%WID_TGRAPH%", String(W_TEMP_GRAPH));
  settingsHtml.replace("%WID_STOCK%", String(W_STOCK));
  settingsHtml.replace("%WID_DDAY%", String(W_DDAY));
  settingsHtml.replace("%WID_CLOCKS%", String(W_TIME) + "," + String(W_WORLD_CLOCK));
  settingsHtml.replace("%CLOCK24_SEL%", clock12h ? "" : "selected");
  settingsHtml.replace("%CLOCK12_SEL%", clock12h ? "selected" : "");
  settingsHtml.replace("%CHANNEL%", htmlEscape(youtube_channel));
  settingsHtml.replace("%LOCATION%", htmlEscape(dust_location));
  settingsHtml.replace("%WEATHER%", htmlEscape(weather_location_code));
  settingsHtml.replace("%INTL%", intl_city.length() > 0 ? htmlEscape(intl_city) : String("미설정"));
  settingsHtml.replace("%INTL_EN%", intl_city.length() > 0 ? htmlEscape(intl_city) : String("Not set"));
  // 칸별 설정 — 스크립트 배열. 화면별 도시 ["New York", "", ...] ("" = 위젯 기본 도시), D-day [{l:"이름", d:"YYYY-MM-DD"}, ...]
  String cities = "[", ddays = "[", stocks = "[";
  for (int i = 0; i < SCREEN_SLOTS; i++) cities += (i ? "," : "") + jsString(slotCity[i].name);
  for (int i = 0; i < SCREEN_SLOTS; i++) stocks += (i ? "," : "") + jsString(slotStock[i].name);
  for (int i = 0; i < DDAY_SLOTS; i++) ddays += String(i ? "," : "") + "{l:" + jsString(dday[i].label) + ",d:" + jsString(dday[i].date) + "}";
  settingsHtml.replace("%CITIES%", cities + "]");
  settingsHtml.replace("%DDAYS%", ddays + "]");
  settingsHtml.replace("%STOCKS%", stocks + "]");
  settingsHtml.replace("%TZ%", (timezone_offset >= 0 ? "+" : "") + String(timezone_offset));   // 예: +9, -5

  html += settingsHtml;
  server.send(200, "text/html", html);
}

/** 문자열 JSON 값 "key":"..."를 from 이후에서 찾아 따옴표 안을 그대로(이스케이프 유지) 돌려준다 — 없으면 -1 */
static int jsonRaw(const String &s, int from, const char *key, String &out) {
  String k = String("\"") + key + "\":\"";
  int a = s.indexOf(k, from);
  if (a < 0) return -1;
  a += k.length();
  int b = a;
  while (b < (int)s.length() && s[b] != '"') b += s[b] == '\\' ? 2 : 1;   // \" 는 건너뛴다
  out = s.substring(a, b);
  return b;
}

/**
 * 관심 종목 이름 검색 — 네이버 종목 자동완성(ac.stock.naver.com)을 기기가 대신 부른다.
 * @details 브라우저에서 바로 부르면 네이버가 막는다(다른 사이트의 요청). 응답 [{n:이름, p:시세 경로, m:시장}, ...]
 */
void handleStockSearch() {
  String q = server.arg("q");
  q.trim();
  if (q.length() == 0 || q.length() > 40 || WiFi.status() != WL_CONNECTED) {
    server.send(400, "application/json", "[]");
    return;
  }
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(5000);
  HTTPClient https;
  https.begin(client, "https://ac.stock.naver.com/ac?target=stock&q=" + urlEncode(q));
  https.setTimeout(5000);
  https.setUserAgent("Mozilla/5.0");
  String payload = https.GET() == 200 ? https.getString() : "";
  https.end();
  if (payload.length() == 0) {
    server.send(502, "application/json", "[]");
    return;
  }
  // 항목마다 "name" → "typeName" → "url" 순서로 온다. url "/domestic/stock/005930/total" → 시세 경로 "domestic/stock/005930"
  String out = "[";
  int pos = payload.indexOf("\"items\":[");
  for (int n = 0; pos >= 0 && n < 10; n++) {
    String name, market, url;
    pos = jsonRaw(payload, pos, "name", name);
    if (pos < 0) break;
    pos = jsonRaw(payload, pos, "typeName", market);
    if (pos < 0) break;
    pos = jsonRaw(payload, pos, "url", url);
    if (pos < 0) break;
    String path = url.substring(1);
    if (path.endsWith("/total")) path.remove(path.length() - 6);
    if (!isStockPath(path)) continue;   // 종목이 아닌 결과는 뺀다
    if (out.length() > 1) out += ",";
    out += "{\"n\":\"" + name + "\",\"p\":\"" + path + "\",\"m\":\"" + market + "\"}";
  }
  server.send(200, "application/json", out + "]");
}

void handleSet() {
  bool changed = false;
  bool tz_changed = false;

  {
    DataLock lock;   // 수집 설정·데이터는 dataTask도 읽고 쓴다 — 바꾸는 동안 잠근다
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
      dust_10_num = -1;   // 새 지역 값을 받기 전까지 "--" 표시
      dust_2_5_num = -1;
      changed = true;
    }
    if (server.hasArg("weather_code") && server.arg("weather_code").length() > 0) {
      weather_location_code = server.arg("weather_code");
      weather_location_name = server.arg("weather_name");   // 동네 검색으로 고르면 이름, 코드를 직접 넣으면 빈 값(코드 표시)
      announcement_time = "";
      changed = true;
    }
    // 해외 날씨·화면별 도시 — 웹 페이지가 도시 검색으로 채운 이름(영문)·좌표
    String name;
    float lat, lon;
    if (readCityArgs("intl", name, lat, lon)) {
      intl_city = name;
      intl_lat = lat;
      intl_lon = lon;
      intl_valid = false;   // 새 도시 값을 받기 전까지 "--" 표시
      changed = true;
    }
    for (int i = 0; i < SCREEN_SLOTS; i++) {   // 화면별 도시 (고른 칸만 값이 온다)
      if (!readCityArgs("city" + String(i), name, lat, lon)) continue;
      slotCity[i] = {name, lat, lon};
      clearSlotData(i);   // 새 도시 값을 받기 전까지 "--" 표시
      changed = true;
    }
    for (int i = 0; i < SCREEN_SLOTS; i++) {   // 관심 종목 (검색으로 고른 칸만 값이 온다)
      String p = "stock" + String(i);
      String path = server.arg(p + "_path"), sname = server.arg(p + "_name");
      if (path.length() == 0) continue;
      if (!isStockPath(path) || sname.length() == 0 || sname.length() > 60) {
        Serial.printf("[WEB] Rejected %s (path %s)\n", p.c_str(), path.c_str());
        continue;
      }
      slotStock[i] = {sname, path};
      stockQuote[i].valid = false;   // 새 종목 값을 받기 전까지 "--" 표시
      changed = true;
    }
  }
  if (server.hasArg("tz_offset") && server.arg("tz_offset").length() > 0) {
    // [보안] 범위 검사 — 실제 시간대는 UTC-12 ~ UTC+14다. 범위 밖 값은 무시하고 기존 값을 유지한다.
    const int tz = server.arg("tz_offset").toInt();
    if (tz >= -12 && tz <= 14) {
      timezone_offset = tz;
      changed = true;
      tz_changed = true;
    } else {
      Serial.printf("[WEB] Rejected tz_offset=%d (allowed -12..14)\n", tz);
    }
  }
  if (server.hasArg("loop_sec")) {   // 자동 전환 간격 — 목록에 있는 값만 받는다
    int sec = server.arg("loop_sec").toInt();
    for (int s : LOOP_SECONDS) {
      if (s == sec && sec != loopIntervalSec) {
        loopIntervalSec = sec;
        changed = true;
      }
    }
  }
  // D-day 칸별 — 날짜는 YYYY-MM-DD(브라우저 날짜 입력), 이름은 40바이트까지. 날짜를 지우면 미설정
  for (int i = 0; i < DDAY_SLOTS; i++) {
    String p = "dday" + String(i);
    if (!server.hasArg(p + "_date")) continue;
    String date = server.arg(p + "_date");
    String label = server.arg(p + "_label");
    bool date_ok = date.length() == 0 ||
                   (date.length() == 10 && date[4] == '-' && date[7] == '-' && date.substring(0, 4).toInt() > 0);
    if (date_ok && label.length() <= 40 && (date != dday[i].date || label != dday[i].label)) {
      dday[i] = {label, date};
      changed = true;
    }
  }
  if (server.hasArg("clock12")) {   // 한국 시계·세계 시계 12/24시간제
    bool v = server.arg("clock12") == "1";
    if (v != clock12h) {
      clock12h = v;
      changed = true;
    }
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
      if (newVal < 0 || newVal > MAX_WIDGET_TYPE) newVal = W_NONE;  // 범위 밖 값은 "사용 안 함"
      if ((int)SCREEN_MAP[i] != newVal) {
        SCREEN_MAP[i] = (WidgetType)newVal;
        changed = true;
      }
    }
  }

  bool saved = true;   // 플래시 저장 성공 여부 — 실패하면 지금은 적용돼도 재시작하면 사라진다
  if (changed) {
    preferences.begin("settings", false);
    // put*은 실패하면 0을 돌려준다 (저장 공간 부족 등)
    auto putStr = [&](const char* key, const String& v) { saved &= preferences.putString(key, v) == v.length(); };
    putStr("channel", youtube_channel);
    putStr("dust_loc", dust_location);
    putStr("weather_code", weather_location_code);
    putStr("weather_name", weather_location_name);
    putStr("intl_city", intl_city);
    saved &= preferences.putFloat("intl_lat", intl_lat) > 0;
    saved &= preferences.putFloat("intl_lon", intl_lon) > 0;
    // 칸별 값은 칸마다 문자열 하나 — 빈 칸은 키를 지워 저장 공간을 아낀다 (부팅 시 Smart_Info_Station.ino가 읽는다)
    //   화면별 도시 ctN = "위도|경도|이름", D-day ddN = "YYYY-MM-DD|이름"
    for (int i = 0; i < SCREEN_SLOTS; i++) {
      char key[8];
      snprintf(key, sizeof(key), "ct%d", i);
      if (slotCity[i].name.length() == 0) preferences.remove(key);
      else putStr(key, String(slotCity[i].lat, 5) + "|" + String(slotCity[i].lon, 5) + "|" + slotCity[i].name);
    }
    for (int i = 0; i < SCREEN_SLOTS; i++) {   // 관심 종목 skN = "시세 경로|이름"
      char key[8];
      snprintf(key, sizeof(key), "sk%d", i);
      if (slotStock[i].path.length() == 0) preferences.remove(key);
      else putStr(key, slotStock[i].path + "|" + slotStock[i].name);
    }
    for (int i = 0; i < DDAY_SLOTS; i++) {
      char key[8];
      snprintf(key, sizeof(key), "dd%d", i);
      if (dday[i].date.length() == 0 && dday[i].label.length() == 0) preferences.remove(key);
      else putStr(key, dday[i].date + "|" + dday[i].label);
    }
    saved &= preferences.putInt("tz_offset", timezone_offset) > 0;
    saved &= preferences.putBool("loop", isLoopingMode) > 0;
    saved &= preferences.putInt("loop_sec", loopIntervalSec) > 0;
    saved &= preferences.putBool("clock12", clock12h) > 0;
    preferences.end();

    preferences.begin("screen_map", false);
    for (int i = 0; i < MAX_DATA_PAGE * 4; i++) {
      char key[5];
      sprintf(key, "s%d", i);
      saved &= preferences.putInt(key, (int)SCREEN_MAP[i]) > 0;
    }
    preferences.end();

    Serial.println(saved ? "[SUCCESS] Settings saved to Preferences." : "[ERROR] Preferences write failed (NVS full?)");

    // 설정 변경 후 즉시 화면을 갱신하기 위해 현재 실행 중인 대기(delay)를 즉시 종료
    if (is_waiting && dataTaskHandle != NULL) {
      xTaskAbortDelay(dataTaskHandle);
    }
  }

  // 시간대 변경 시 즉시 재적용 — time_sync 단일 경로를 쓴다 (부팅·주기 재동기화와 동일).
  if (tz_changed) {
    timeSync.applyTimezone();
  }

  portENTER_CRITICAL(&updateMux);  // force_update 쓰기 보호
  force_update = true;             // 설정 저장 직후 백그라운드 태스크에 즉시 갱신 명령 전달
  portEXIT_CRITICAL(&updateMux);
  redraw_current_page();           // 저장 즉시 현재 페이지 화면 레이아웃 갱신 반영

  if (saved) {
    server.send(200, "text/html", SUCCESS_HTML);
    return;
  }
  String page = FPSTR(SUCCESS_HTML);   // 같은 페이지에 실패 문구만 바꿔 보낸다
  page.replace("✅ 저장 완료!", "⚠️ 저장 실패");
  page.replace("스마트 보드에 설정이 성공적으로 반영되었습니다.", "지금은 반영됐지만 저장 공간이 부족해 재시작하면 사라집니다.");
  page.replace("'✅ Saved!'", "'⚠️ Save failed'");
  page.replace("'Settings have been applied to the board.'", "'Applied for now, but storage is full — lost on restart.'");
  server.send(200, "text/html", page);
}
