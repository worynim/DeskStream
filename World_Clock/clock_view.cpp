// worynim@gmail.com
/**
 * @file clock_view.cpp
 * @brief 시계 화면 렌더러 구현
 * @details 배치 (y): 0~10 도시 이름·날짜 / 14~51 AM·PM + 아이콘 + 큰 숫자 / 55~62 하루 막대.
 */
#include "clock_view.h"

#include <stdio.h>

#include "fonts.h"

namespace {

const uint8_t* const kAmPmFont = u8g2_font_fub17_tr;    // 대문자 높이 17px
const uint8_t* const kDigitFont = u8g2_font_logisoso28_tn;   // 숫자 높이 28px, 폭이 좁아 "12:00" 도 AM/PM 옆에 들어간다

const int kAmPmX = 1;
const int kAmBaseline = 31;     // AM 은 숫자 줄의 위쪽 자리
const int kPmBaseline = 51;     // PM 은 아래쪽 자리
const int kIconCx = 14;         // 아이콘은 AM/PM 이 비운 자리에 들어간다
const int kDigitRight = 127;    // 숫자는 오른쪽 맞춤
const int kDigitBaseline = 49;
const int kBarTop = 58, kBarHeight = 5;

const char* const kWeekdays[2][7] = {{"일", "월", "화", "수", "목", "금", "토"},
                                     {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}};

/** 맨 윗줄: 왼쪽에 도시 이름, 오른쪽에 날짜·요일·날짜 차이. 자리가 모자라면 요일 → 날짜 순으로 뺀다 */
void drawTopRow(u8g2_t* u, const ClockFace& f) {
    u8g2_SetFont(u, kFontKr);
    u8g2_DrawUTF8(u, 0, 9, f.city);
    if (!f.valid) return;
    char diff[12] = "";
    if (f.dayDiff != 0) snprintf(diff, sizeof(diff), f.lang == LANG_KO ? " %+d일" : " %+dd", f.dayDiff);
    char buf[3][32];
    snprintf(buf[0], sizeof(buf[0]), "%d/%d %s%s", f.month, f.day, kWeekdays[f.lang][f.weekday % 7], diff);
    snprintf(buf[1], sizeof(buf[1]), "%d/%d%s", f.month, f.day, diff);
    snprintf(buf[2], sizeof(buf[2]), "%s", diff);
    const int room = 128 - u8g2_GetUTF8Width(u, f.city) - 4;
    for (const auto& b : buf) {
        const int w = u8g2_GetUTF8Width(u, b);
        if (w <= room) {
            u8g2_DrawUTF8(u, 128 - w, 9, b);
            return;
        }
    }
}

/** 큰 숫자를 오른쪽 맞춤으로 그린다. ':' 는 깜빡여도 숫자가 움직이지 않게 자리를 고정한다 */
void drawDigits(u8g2_t* u, const ClockFace& f) {
    u8g2_SetFont(u, kDigitFont);
    char hh[4], mm[4];
    if (f.valid) {
        const int h12 = f.hour % 12 == 0 ? 12 : f.hour % 12;
        snprintf(hh, sizeof(hh), "%d", f.hour24 ? f.hour : h12);
        snprintf(mm, sizeof(mm), "%02d", f.minute);
    } else {
        snprintf(hh, sizeof(hh), "--");
        snprintf(mm, sizeof(mm), "--");
    }
    const int colonW = u8g2_GetStrWidth(u, ":");
    const int mmX = kDigitRight - u8g2_GetStrWidth(u, mm);
    const int colonX = mmX - colonW - 1;
    u8g2_DrawStr(u, mmX, kDigitBaseline, mm);
    if (f.colonOn || !f.valid) u8g2_DrawStr(u, colonX, kDigitBaseline, ":");
    u8g2_DrawStr(u, colonX - 1 - u8g2_GetStrWidth(u, hh), kDigitBaseline, hh);
}

/** 하루 막대: 낮 구간(6~18시)은 채우고 밤은 테두리만, 지금 시각에 세로 표시 */
void drawDayBar(u8g2_t* u, const ClockFace& f) {
    const int x6 = dayBarX(6, 0), x18 = dayBarX(18, 0);
    u8g2_DrawFrame(u, 0, kBarTop, 128, kBarHeight);
    u8g2_DrawBox(u, x6, kBarTop, x18 - x6, kBarHeight);
    if (!f.valid) return;
    int x = dayBarX(f.hour, f.minute);
    if (x < 2) x = 2;
    if (x > 125) x = 125;
    u8g2_SetDrawColor(u, 0);                                  // 표시 둘레를 비워 낮 구간에서도 보이게
    u8g2_DrawBox(u, x - 3, kBarTop - 3, 7, kBarHeight + 3);
    u8g2_SetDrawColor(u, 1);
    u8g2_DrawBox(u, x - 1, kBarTop - 3, 3, kBarHeight + 3);
}

}  // namespace

bool isDaytime(uint8_t hour) { return hour >= 6 && hour < 18; }

int dayBarX(uint8_t hour, uint8_t minute) { return (hour * 60 + minute) * 127 / 1439; }

void drawDayNightIcon(u8g2_t* u, int cx, int cy, bool day) {
    if (day) {
        u8g2_DrawDisc(u, cx, cy, 3, U8G2_DRAW_ALL);
        static const int8_t kRays[8][4] = {{0, -5, 0, -7}, {0, 5, 0, 7}, {-5, 0, -7, 0}, {5, 0, 7, 0},
                                           {-4, -4, -5, -5}, {4, -4, 5, -5}, {-4, 4, -5, 5}, {4, 4, 5, 5}};
        for (const auto& r : kRays) u8g2_DrawLine(u, cx + r[0], cy + r[1], cx + r[2], cy + r[3]);
    } else {
        u8g2_DrawDisc(u, cx, cy, 6, U8G2_DRAW_ALL);
        u8g2_SetDrawColor(u, 0);
        u8g2_DrawDisc(u, cx + 4, cy - 2, 6, U8G2_DRAW_ALL);   // 오른쪽 위를 깎아 초승달로
        u8g2_SetDrawColor(u, 1);
    }
}

void drawClock(u8g2_t* u, const ClockFace& f) {
    u8g2_ClearBuffer(u);
    u8g2_SetFontMode(u, 1);
    u8g2_SetDrawColor(u, 1);
    drawTopRow(u, f);
    drawDigits(u, f);
    if (f.valid) {
        const bool am = f.hour < 12;
        u8g2_SetFont(u, kAmPmFont);
        u8g2_DrawStr(u, kAmPmX, am ? kAmBaseline : kPmBaseline, am ? "AM" : "PM");
        drawDayNightIcon(u, kIconCx, am ? 44 : 22, isDaytime(f.hour));
    }
    drawDayBar(u, f);
}
