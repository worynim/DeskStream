#include "widgets_display.h"
#include <time.h>
#include <WiFi.h>

#include "market.h"
#include "weather.h"
#include "dust.h"
#include "youtube.h"
#include "btc.h"
#include "usdkrw.h"

// --- 렌더링 함수 구현 ---

void display_clock_oled(U8G2 &u8g2) {
  u8g2_prepare(u8g2);
  time_t now = time(nullptr);
  String t_str = String(ctime(&now));
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_bytesize_tr);
  String dateLine = t_str.substring(20, 24) + " " + t_str.substring(4, 10) + " " + t_str.substring(0, 3);
  drawCenteredText(u8g2, dateLine, 5);
  u8g2.setFont(u8g2_font_maniac_tr);
  drawCenteredText(u8g2, t_str.substring(11, 19), 25);

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(10, 56, "Set: ");
  u8g2.drawStr(35, 56, WiFi.localIP().toString().c_str());
  u8g2.sendBuffer();
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
  u8g2.sendBuffer();
}

void display_weather_oled(U8G2 &u8g2) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 0, ("Ann.Time: " + announcement_time.substring(0, 5) + " " + announcement_time.substring(announcement_time.length() - 5)).c_str());
  for (int i = 0; i < 3; i++) {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(43 * i, 13, (String(forecast[i].time) + ":00").c_str());
    if (forecast[i].type == 1) u8g2.drawStr(43 * i, 23, "Clear");
    else if (forecast[i].type == 2) u8g2.drawStr(43 * i, 23, "Partly");
    else if (forecast[i].type == 3) u8g2.drawStr(43 * i, 23, "Mostly");
    else if (forecast[i].type == 4) u8g2.drawStr(43 * i, 23, "Cloudy");
    else if (forecast[i].type == 5) u8g2.drawStr(43 * i, 23, "Rain");
    else if (forecast[i].type <= 9) u8g2.drawStr(43 * i, 23, "Shower");
    else u8g2.drawStr(43 * i, 23, "Unknown");
    u8g2.setFont(u8g2_font_open_iconic_weather_2x_t);
    int icon = 68;
    if (forecast[i].type == 1) icon = 69;
    else if (forecast[i].type <= 3) icon = 65;
    else if (forecast[i].type == 4) icon = 64;
    else if (forecast[i].type >= 5) icon = 67;
    u8g2.drawGlyph(43 * i + 2, 33, icon);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(43 * i, 53);
    u8g2.print(forecast[i].temp);
    u8g2.write(0xB0);
    u8g2.print("C");
  }
  u8g2.sendBuffer();
}

void display_dust_oled(U8G2 &u8g2) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 0, "Air pollution ");
  u8g2.drawStr(0, 18, "PM10: ");
  u8g2.drawStr(0, 43, "PM2.5: ");
  if (dust_10_num <= 30) u8g2.drawStr(0, 28, "Good");
  else if (dust_10_num <= 80) u8g2.drawStr(0, 28, "Moderate");
  else if (dust_10_num <= 150) u8g2.drawStr(0, 28, "Bad");
  else u8g2.drawStr(0, 28, "Dangerous");
  if (dust_2_5_num <= 15) u8g2.drawStr(0, 54, "Good");
  else if (dust_2_5_num <= 35) u8g2.drawStr(0, 54, "Moderate");
  else if (dust_2_5_num <= 75) u8g2.drawStr(0, 54, "Bad");
  else u8g2.drawStr(0, 54, "Dangerous");
  u8g2.setFont(u8g2_font_maniac_tr);
  u8g2.drawStr(60, 14, String(dust_10_num).c_str());
  u8g2.drawStr(60, 40, String(dust_2_5_num).c_str());
  u8g2.sendBuffer();
}

void display_youtube_oled(U8G2 &u8g2) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  if (containsKorean(youtube_channel)) u8g2.setFont(u8g2_font_unifont_t_korean2);
  else u8g2.setFont(u8g2_font_bytesize_tr);
  drawCenteredText(u8g2, youtube_channel, 0);
  u8g2.setFont(u8g2_font_6x10_tf);
  drawCenteredText(u8g2, "YouTube Subscribers", 20);
  u8g2.setFont(u8g2_font_maniac_tr);
  drawCenteredText(u8g2, subscribe_num, 33);
  u8g2.sendBuffer();
}

void display_market_oled(U8G2 &u8g2, const String &title, const String &price, const String &change, const String &percent) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_bytesize_tr);
  drawCenteredText(u8g2, title, 5);
  u8g2.setFont(u8g2_font_maniac_tr);
  drawCenteredText(u8g2, price, 25);
  u8g2.setFont(u8g2_font_6x10_tf);
  String changeStr = change + " (" + percent + ")";
  drawCenteredText(u8g2, changeStr, 56);
  u8g2.sendBuffer();
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
void display_kpi200_oled(U8G2 &u8g2) {
  display_market_oled(u8g2, "KOSPI200", kpi200_data.price, kpi200_data.change, kpi200_data.percent);
}
