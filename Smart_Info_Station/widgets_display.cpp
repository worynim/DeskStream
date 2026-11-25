// worynim@gmail.com
/**
 * @file widgets_display.cpp
 * @brief 정보 타입별 전용 위젯 렌더링 구현
 * @details 날씨, 시세, 뉴스 등의 데이터를 각각의 UI 컴포넌트로 시각화하는 로직 처리
 */
// 갈무리7 한글 글꼴은 64KB 를 넘어 U8g2 가 큰 글꼴 지원을 켜야 한다 — U8g2lib.h 보다 먼저 정의
#ifndef U8G2_USE_LARGE_FONTS
#define U8G2_USE_LARGE_FONTS
#endif
#include "widgets_display.h"
#include <time.h>
#include <WiFi.h>

#include "market.h"
#include "weather.h"
#include "dust.h"
#include "youtube.h"
#include "btc.h"
#include "usdkrw.h"
#include "slot_city.h"
#include "u8g2_font_galmuri7_kr.h"   // 갈무리7 (SIL OFL 1.1) — 동네 날씨·미세먼지 윗줄의 지명, D-day 이름 (한글)

// --- 렌더링 함수 구현 ---

bool clock12h = false;
DdaySetting dday[DDAY_SLOTS];

static const time_t kTimeSyncedEpoch = 1600000000;   // 2020-09 이후면 NTP로 맞춰진 시각으로 본다

/** 설정이 없을 때의 안내 화면 (제목 + 두 줄) */
static void drawSetupHint(U8G2 &u8g2, const char *title, const char *line1, const char *line2) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 0, title);
  u8g2.drawStr(0, 22, line1);
  u8g2.drawStr(0, 34, line2);
}

/** 6x10 글꼴로 가운데 정렬 (UTF-8 라틴 문자 포함) */
static void drawCenteredUTF8(U8G2 &u8g2, int y, const char *s) {
  int x = (128 - u8g2.getUTF8Width(s)) / 2;
  u8g2.drawUTF8(x < 0 ? 0 : x, y, s);
}

/**
 * 시계 숫자를 그린다 — 24시간제 "14:05:09", 12시간제 "2:05:09" + 좌측 상단 AM/PM
 * @details AM/PM은 윗줄(y 0~12)과 시각 숫자(y 22~) 사이 빈자리(y 13)에 둔다
 */
static void drawClockTime(U8G2 &u8g2, const struct tm &t) {
  char buf[12];
  if (clock12h) {
    int h = t.tm_hour % 12;
    snprintf(buf, sizeof(buf), "%d:%02d:%02d", h == 0 ? 12 : h, t.tm_min, t.tm_sec);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 13, t.tm_hour < 12 ? "AM" : "PM");
  } else {
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
  }
  u8g2.setFont(MAIN_NUM_FONT);
  drawCenteredText(u8g2, buf, 22);
}

void display_clock_oled(U8G2 &u8g2) {
  u8g2_prepare(u8g2);
  time_t now = time(nullptr);
  String t_str = String(ctime(&now));
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_bytesize_tr);
  String dateLine = t_str.substring(20, 24) + " " + t_str.substring(4, 10) + " " + t_str.substring(0, 3);
  drawCenteredText(u8g2, dateLine, 0);
  struct tm lt;
  localtime_r(&now, &lt);
  drawClockTime(u8g2, lt);

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(10, 56, "Set: ");
  u8g2.drawStr(35, 56, WiFi.localIP().toString().c_str());
  display.push();
}

void display_calendar_oled(U8G2 &u8g2) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  int year = timeinfo.tm_year + 1900;
  int month = timeinfo.tm_mon + 1;
  int day = timeinfo.tm_mday;

  u8g2_prepare(u8g2);
  u8g2.setFontPosBaseline(); 
  u8g2.clearBuffer();
  
  char buf[16];
  snprintf(buf, sizeof(buf), "%d.%02d", year, month);

  u8g2.setFont(u8g2_font_bytesize_tr); // 사용자가 수정한 폰트 반영
  int textWidth = u8g2.getUTF8Width(buf);
  int headerX = (128 - textWidth) / 2;
  if (headerX < 0) headerX = 0;
  u8g2.drawUTF8(headerX, 14, buf);
  
  static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
  int y_calc = year;
  if (month < 3) y_calc -= 1;
  int firstDayOfWeek = ( y_calc + y_calc/4 - y_calc/100 + y_calc/400 + t[month-1] + 1 ) % 7; 
  
  int daysInMonth = 31;
  if (month == 4 || month == 6 || month == 9 || month == 11) daysInMonth = 30;
  else if (month == 2) daysInMonth = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
  
  u8g2.setFont(u8g2_font_5x7_tr);
  const char* wdays[] = {"S", "M", "T", "W", "T", "F", "S"};
  int colW = 18;
  int xOffset = 1;
  
  for (int i = 0; i < 7; i++) {
      int x = xOffset + i * colW + (colW - u8g2.getUTF8Width(wdays[i])) / 2;
      u8g2.drawStr(x, 22, wdays[i]);
  }
  
  u8g2.drawLine(0, 23, 128, 23); 
  
  int currentDay = 1;
  for (int row = 0; row < 6; row++) {
      for (int col = 0; col < 7; col++) {
          if (row == 0 && col < firstDayOfWeek) continue;
          if (currentDay > daysInMonth) break;
          
          snprintf(buf, sizeof(buf), "%d", currentDay);
          int dw = u8g2.getUTF8Width(buf);
          int cx = xOffset + col * colW + colW / 2; 
          int dx = cx - dw / 2; 
          int dy = 31 + row * 8; 
          
          if (currentDay == day) { 
              u8g2.setDrawColor(1); 
              u8g2.drawBox(cx - 8, dy - 7, 16, 8); 
              u8g2.setDrawColor(0); 
              u8g2.drawStr(dx, dy, buf);
              u8g2.setDrawColor(1); 
          } else {
              u8g2.drawStr(dx, dy, buf);
          }
          currentDay++;
      }
      if (currentDay > daysInMonth) break;
  }
  display.push();
}

/**
 * 3칸 예보의 i번째 칸(폭 43px)에 시각·날씨 문구·아이콘·기온을 칸 가운데에 맞춰 그린다.
 * @param text nullptr이면 아직 받지 못한 칸 — 시각 줄에 "--"만 그린다
 */
static void drawForecastColumn(U8G2 &u8g2, int i, int hour, const char *text, int icon, int temp) {
  const int cx = 43 * i + 21;   // 칸 가운데 (128px를 3칸으로)
  char buf[12];
  u8g2.setFont(u8g2_font_6x10_tf);
  if (text == nullptr) {
    u8g2.drawStr(cx - u8g2.getStrWidth("--") / 2, 13, "--");
    return;
  }
  snprintf(buf, sizeof(buf), "%d:00", hour);
  u8g2.drawStr(cx - u8g2.getStrWidth(buf) / 2, 13, buf);
  u8g2.drawStr(cx - u8g2.getStrWidth(text) / 2, 23, text);
  snprintf(buf, sizeof(buf), "%d\xB0" "C", temp);   // 6x10_tf의 0xB0 = °
  u8g2.drawStr(cx - u8g2.getStrWidth(buf) / 2, 53, buf);
  u8g2.setFont(u8g2_font_open_iconic_weather_2x_t);
  u8g2.drawGlyph(cx - 8, 33, icon);   // 아이콘 폭 16px
}

void display_weather_oled(U8G2 &u8g2) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  // 윗줄: 동네 이름 (갈무리7) — 이름을 모르면(코드를 직접 입력) 코드를 보여 준다
  u8g2.setFont(u8g2_font_galmuri7_kr);
  u8g2.drawUTF8(0, 0, (weather_location_name.length() > 0 ? weather_location_name : weather_location_code).c_str());
  // 오른쪽: 기상청 발표 시각 — "11.26.(목) 03:50" → "11.26 03:50" (아직 받지 못했으면 생략)
  if (announcement_time.length() >= 10) {
    String ann = announcement_time.substring(0, 5) + " " + announcement_time.substring(announcement_time.length() - 5);
    u8g2.drawUTF8(128 - u8g2.getUTF8Width(ann.c_str()), 0, ann.c_str());
  }
  for (int i = 0; i < 3; i++) {
    int type = forecast[i].type;
    if (type == 0) {   // 아직 받지 못한 시간대 (날씨 코드는 1부터)
      drawForecastColumn(u8g2, i, 0, nullptr, 0, 0);
      continue;
    }
    const char *text = "Unknown";
    if (type == 1) text = "Clear";
    else if (type == 2) text = "Partly";
    else if (type == 3) text = "Mostly";
    else if (type == 4) text = "Cloudy";
    else if (type == 5) text = "Rain";
    else if (type <= 9) text = "Shower";
    int icon = 68;
    if (type == 1) icon = 69;
    else if (type <= 3) icon = 65;
    else if (type == 4) icon = 64;
    else if (type >= 5) icon = 67;
    drawForecastColumn(u8g2, i, forecast[i].time, text, icon, forecast[i].temp);
  }
  display.push();
}

/** WMO 날씨 코드 → 표시 문구·아이콘 (open_iconic_weather_2x: 64 구름, 65 구름 조금, 67 비, 69 해) */
static void wmoLook(int code, const char *&text, int &icon) {
  if (code == 0)                                   { text = "Clear";  icon = 69; }
  else if (code <= 2)                              { text = "Partly"; icon = 65; }   // 1 대체로 맑음, 2 구름 조금
  else if (code == 3)                              { text = "Cloudy"; icon = 64; }
  else if (code == 45 || code == 48)               { text = "Fog";    icon = 64; }
  else if ((code >= 71 && code <= 77) || code == 85 || code == 86) { text = "Snow"; icon = 67; }
  else if (code >= 95)                             { text = "Storm";  icon = 67; }   // 95~99 뇌우
  else if (code >= 80)                             { text = "Shower"; icon = 67; }   // 80~82 소나기
  else                                             { text = "Rain";   icon = 67; }   // 51~67 이슬비·비
}

void display_intl_weather_oled(U8G2 &u8g2) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  if (intl_city.length() == 0) {   // 도시 미설정
    u8g2.drawStr(0, 0, "World Weather");
    u8g2.drawStr(0, 22, "Set a city on the");
    u8g2.drawStr(0, 34, "settings web page");
    display.push();
    return;
  }

  u8g2.drawUTF8(0, 0, intl_city.c_str());   // 영문 이름 (é 등 라틴 문자 포함)
  for (int i = 0; i < 3; i++) {
    if (!intl_valid) {   // 아직 받지 못함
      drawForecastColumn(u8g2, i, 0, nullptr, 0, 0);
      continue;
    }
    const char *text;
    int icon;
    wmoLook(intl_forecast[i].code, text, icon);
    drawForecastColumn(u8g2, i, intl_forecast[i].time, text, icon, intl_forecast[i].temp);
  }
  display.push();
}

void display_world_clock_oled(U8G2 &u8g2, int slot) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  const WorldClockTime &c = wcTime[slot];   // 칸마다 도시가 다르다 (기본 뉴욕)
  u8g2.setFont(u8g2_font_6x10_tf);
  drawCenteredUTF8(u8g2, 0, cityFor(slot, W_WORLD_CLOCK).name.c_str());

  time_t now = time(nullptr);
  u8g2.setFont(MAIN_NUM_FONT);
  if (!c.valid || now < kTimeSyncedEpoch) {   // 오프셋이나 시각을 아직 모름
    drawCenteredText(u8g2, "--:--:--", 22);
    display.push();
    return;
  }
  time_t t = now + c.offset;   // 도시 현지 시각 (서머타임은 30분마다 오프셋을 다시 받아 반영)
  struct tm lt;
  gmtime_r(&t, &lt);
  drawClockTime(u8g2, lt);

  // 아랫줄: 요일·날짜·UTC 오프셋 — 예: "Thu Nov 26  UTC-5", "UTC+5:30"
  char buf[24], date[16], off[12];
  strftime(date, sizeof(date), "%a %b %d", &lt);
  long m = labs(c.offset) / 60;
  if (m % 60) snprintf(off, sizeof(off), "UTC%c%ld:%02ld", c.offset < 0 ? '-' : '+', m / 60, m % 60);
  else snprintf(off, sizeof(off), "UTC%c%ld", c.offset < 0 ? '-' : '+', m / 60);
  snprintf(buf, sizeof(buf), "%s  %s", date, off);
  u8g2.setFont(u8g2_font_6x10_tf);
  drawCenteredUTF8(u8g2, 54, buf);
  display.push();
}

/** 달의 위상 0~1 (0 삭, 0.5 망) — 2000-01-06 18:14 UTC 삭 기준, 삭망월 29.530588853일 (오차 ±1일 이내) */
static double moonPhase(time_t utc) {
  double p = fmod((utc - 947182440L) / 86400.0 / 29.530588853, 1.0);
  return p < 0 ? p + 1 : p;
}

/**
 * 달 모양을 그린다 — 테두리 원 + 밝은 부분 채움 (OLED 흰색 = 빛나는 면)
 * @param south 남반구면 좌우를 뒤집는다 (보이는 모양이 반대)
 */
static void drawMoon(U8G2 &u8g2, int cx, int cy, int r, double phase, bool south) {
  u8g2.drawCircle(cx, cy, r);
  double k = cos(2 * M_PI * phase);   // 줄마다 빛나는 구간: 차는 달 [k·w, w], 기우는 달 [-w, -k·w]
  for (int dy = -r; dy <= r; dy++) {
    double w = sqrt((double)r * r - dy * dy);
    double x0, x1;
    if (phase < 0.5) { x0 = k * w; x1 = w; }
    else { x0 = -w; x1 = -k * w; }
    if (south) { double t = -x1; x1 = -x0; x0 = t; }
    int a = (int)lround(x0), b = (int)lround(x1);
    if (b > a) u8g2.drawHLine(cx + a, cy + dy, b - a + 1);
  }
}

void display_sun_oled(U8G2 &u8g2, int slot) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  SlotCity city = cityFor(slot, W_SUN);
  const SunInfo &s = sunInfo[slot];
  u8g2.setFont(u8g2_font_6x10_tf);
  drawCenteredUTF8(u8g2, 0, city.name.c_str());

  // 왼쪽: 해 뜨는·지는 시각, 낮 길이 (도시 현지 시각)
  char buf[16];
  snprintf(buf, sizeof(buf), "Rise %s", s.valid ? s.rise : "--:--");
  u8g2.drawStr(0, 16, buf);
  snprintf(buf, sizeof(buf), "Set  %s", s.valid ? s.set : "--:--");
  u8g2.drawStr(0, 28, buf);
  int day = (atoi(s.set) * 60 + atoi(s.set + 3)) - (atoi(s.rise) * 60 + atoi(s.rise + 3));
  if (s.valid && s.rise[0] != '-' && day > 0) snprintf(buf, sizeof(buf), "Day  %d:%02d", day / 60, day % 60);
  else snprintf(buf, sizeof(buf), "Day  --:--");
  u8g2.drawStr(0, 40, buf);

  // 오른쪽: 달 모양 + 이름, 왼쪽 아래: 밝은 면 비율 — 날짜로 계산 (네트워크 불필요)
  time_t now = time(nullptr);
  if (now >= kTimeSyncedEpoch) {
    double p = moonPhase(now);
    static const char *names[] = {"New", "Wax Cr", "1st Q", "Wax Gb", "Full", "Wan Gb", "3rd Q", "Wan Cr"};
    drawMoon(u8g2, 105, 30, 13, p, city.lat < 0);
    u8g2.setFont(u8g2_font_5x7_tr);
    const char *name = names[(int)(p * 8 + 0.5) % 8];
    u8g2.drawStr(105 - u8g2.getStrWidth(name) / 2, 48, name);
    u8g2.setFont(u8g2_font_6x10_tf);
    snprintf(buf, sizeof(buf), "Moon %d%%", (int)lround((1 - cos(2 * M_PI * p)) / 2 * 100));
    u8g2.drawStr(0, 52, buf);
  }
  display.push();
}

void display_weekly_oled(U8G2 &u8g2, int slot) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  const WeeklyInfo &w = weekly[slot];
  u8g2.setFont(u8g2_font_6x10_tf);
  drawCenteredUTF8(u8g2, 0, cityFor(slot, W_WEEKLY).name.c_str());
  if (!w.valid) {   // 아직 받지 못함
    drawCenteredUTF8(u8g2, 28, "--");
    display.push();
    return;
  }
  // 5칸(약 25px) — 요일(갈무리7, 첫 칸 "오늘")·날씨 아이콘·최고·최저 기온
  static const char *wdays[] = {"일", "월", "화", "수", "목", "금", "토"};
  for (int i = 0; i < WEEK_DAYS; i++) {
    int cx = (2 * i + 1) * 128 / (2 * WEEK_DAYS);   // 칸 가운데
    const char *day = i == 0 ? "오늘" : wdays[w.wday[i]];
    u8g2.setFont(u8g2_font_galmuri7_kr);
    u8g2.drawUTF8(cx - u8g2.getUTF8Width(day) / 2, 11, day);
    const char *text;
    int icon;
    wmoLook(w.code[i], text, icon);
    u8g2.setFont(u8g2_font_open_iconic_weather_2x_t);
    u8g2.drawGlyph(cx - 8, 21, icon);
    char buf[8];
    u8g2.setFont(u8g2_font_5x7_tf);
    snprintf(buf, sizeof(buf), "%d\xB0", w.tmax[i]);   // 5x7_tf의 0xB0 = °
    u8g2.drawStr(cx - u8g2.getStrWidth(buf) / 2, 41, buf);
    snprintf(buf, sizeof(buf), "%d\xB0", w.tmin[i]);
    u8g2.drawStr(cx - u8g2.getStrWidth(buf) / 2, 51, buf);
  }
  display.push();
}

/** 그레고리력 날짜의 일 번호 (1970-01-01 = 0) — 날짜 차이 계산용 */
static long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  long yoe = y - era * 400;
  long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

void display_dday_oled(U8G2 &u8g2, int slot) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  const String &dday_label = dday[slot].label;   // 칸마다 이름·날짜가 다르다
  const String &dday_date = dday[slot].date;
  if (dday_date.length() != 10) {
    drawSetupHint(u8g2, "D-day", "Set a date on the", "settings web page");
    display.push();
    return;
  }
  // 윗줄: 이름 (갈무리7, 한글 가능)
  u8g2.setFont(u8g2_font_galmuri7_kr);
  const char *label = dday_label.length() > 0 ? dday_label.c_str() : "D-day";
  int x = (128 - u8g2.getUTF8Width(label)) / 2;
  u8g2.drawUTF8(x < 0 ? 0 : x, 0, label);

  char buf[16];
  struct tm now;
  if (time(nullptr) < kTimeSyncedEpoch || !getLocalTime(&now, 0)) {   // 시각을 아직 모름 (기다리지 않는다)
    snprintf(buf, sizeof(buf), "D-?");
  } else {
    long diff = daysFromCivil(atoi(dday_date.c_str()), atoi(dday_date.c_str() + 5), atoi(dday_date.c_str() + 8)) -
                daysFromCivil(now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);
    if (diff == 0) snprintf(buf, sizeof(buf), "D-DAY");
    else if (diff > 0) snprintf(buf, sizeof(buf), "D-%ld", diff);    // 남은 날
    else snprintf(buf, sizeof(buf), "D+%ld", -diff);                 // 지난 날
  }
  u8g2.setFont(MAIN_NUM_FONT);
  drawCenteredText(u8g2, buf, 20);
  u8g2.setFont(u8g2_font_6x10_tf);
  drawCenteredUTF8(u8g2, 54, dday_date.c_str());
  display.push();
}

void display_fx_oled(U8G2 &u8g2) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_bytesize_tr);
  drawCenteredText(u8g2, "FX (KRW)", 2);
  // 한 줄에 통화·환율(오른쪽 맞춤 x=86)·등락률(오른쪽 끝)
  const struct { const char *label; const MarketData &d; } rows[] = {
    {"JPY100", jpy_data}, {"EUR", eur_data}, {"CNY", cny_data},
  };
  u8g2.setFont(u8g2_font_6x10_tf);
  for (int i = 0; i < 3; i++) {
    int y = 22 + i * 14;
    u8g2.drawStr(0, y, rows[i].label);
    u8g2.drawStr(86 - u8g2.getStrWidth(rows[i].d.price.c_str()), y, rows[i].d.price.c_str());
    u8g2.drawStr(128 - u8g2.getStrWidth(rows[i].d.percent.c_str()), y, rows[i].d.percent.c_str());
  }
  display.push();
}

void display_dust_oled(U8G2 &u8g2) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFont(u8g2_font_galmuri7_kr);   // 윗줄: "가산동 Air pollution" (지명은 한글)
  u8g2.drawUTF8(0, 0, (dust_location + " Air pollution").c_str());
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 18, "PM10: ");
  u8g2.drawStr(0, 43, "PM2.5: ");
  if (dust_10_num < 0 || dust_2_5_num < 0) {   // 아직 받지 못함 — 등급 대신 "--"
    u8g2.setFont(MAIN_NUM_FONT);
    u8g2.drawStr(60, 14, "--");
    u8g2.drawStr(60, 40, "--");
    display.push();
    return;
  }
  if (dust_10_num <= 30) u8g2.drawStr(0, 28, "Good");
  else if (dust_10_num <= 80) u8g2.drawStr(0, 28, "Moderate");
  else if (dust_10_num <= 150) u8g2.drawStr(0, 28, "Bad");
  else u8g2.drawStr(0, 28, "Dangerous");
  if (dust_2_5_num <= 15) u8g2.drawStr(0, 54, "Good");
  else if (dust_2_5_num <= 35) u8g2.drawStr(0, 54, "Moderate");
  else if (dust_2_5_num <= 75) u8g2.drawStr(0, 54, "Bad");
  else u8g2.drawStr(0, 54, "Dangerous");
  u8g2.setFont(MAIN_NUM_FONT);
  u8g2.drawStr(60, 14, String(dust_10_num).c_str());
  u8g2.drawStr(60, 40, String(dust_2_5_num).c_str());
  display.push();
}

void display_youtube_oled(U8G2 &u8g2) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  if (containsKorean(youtube_channel)) u8g2.setFont(u8g2_font_unifont_t_korean2);
  else u8g2.setFont(u8g2_font_bytesize_tr);
  drawCenteredText(u8g2, youtube_channel, 0);
  u8g2.setFont(u8g2_font_6x10_tf);
  drawCenteredText(u8g2, "YouTube Subscribers", 20);
  u8g2.setFont(MAIN_NUM_FONT);
  drawCenteredText(u8g2, subscribe_num, 33);
  display.push();
}

void display_market_oled(U8G2 &u8g2, const String &title, const String &price, const String &change, const String &percent) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_bytesize_tr);
  drawCenteredText(u8g2, title, 5);
  u8g2.setFont(MAIN_NUM_FONT);
  drawCenteredText(u8g2, price, 25);
  u8g2.setFont(u8g2_font_6x10_tf);
  String changeStr = change + " (" + percent + ")";
  drawCenteredText(u8g2, changeStr, 56);
  display.push();
}

void display_kospi_oled(U8G2 &u8g2) {
  display_market_oled(u8g2, "KOSPI", kospi_data.price, kospi_data.change, kospi_data.percent);
}
void display_kosdaq_oled(U8G2 &u8g2) {
  display_market_oled(u8g2, "KOSDAQ", kosdaq_data.price, kosdaq_data.change, kosdaq_data.percent);
}
void display_SnP500_oled(U8G2 &u8g2) {
  display_market_oled(u8g2, "S&P 500", snp500_data.price, snp500_data.change, snp500_data.percent);
}
void display_btc_oled(U8G2 &u8g2) {
  display_market_oled(u8g2, "Bitcoin(BTC $)", btc_data.price, btc_data.change, btc_data.percent);
}
void display_usdkrw_oled(U8G2 &u8g2) {
  display_market_oled(u8g2, "USD/KRW", usdkrw_data.price, usdkrw_data.change, usdkrw_data.percent);
}
void display_nasdaq_oled(U8G2 &u8g2) {
  display_market_oled(u8g2, "NASDAQ", nasdaq_data.price, nasdaq_data.change, nasdaq_data.percent);
}
void display_futures_oled(U8G2 &u8g2) {
  display_market_oled(u8g2, "FUTURES", futures_data.price, futures_data.change, futures_data.percent);
}
void display_eth_oled(U8G2 &u8g2) {
  display_market_oled(u8g2, "Ethereum(ETH $)", eth_data.price, eth_data.change, eth_data.percent);
}
void display_kpi200_oled(U8G2 &u8g2) {
  display_market_oled(u8g2, "KOSPI200", kpi200_data.price, kpi200_data.change, kpi200_data.percent);
}
