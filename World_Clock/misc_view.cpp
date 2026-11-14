// worynim@gmail.com
/**
 * @file misc_view.cpp
 * @brief 안내문 · 알림 딱지 · 정보 카드 구현
 */
#include "misc_view.h"

#include <stdio.h>
#include <stdlib.h>

#include "fonts.h"
#include "setup_view.h"

namespace {

const char* const kWeekdays[2][7] = {{"일", "월", "화", "수", "목", "금", "토"},
                                     {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}};
const char* const kHome[2] = {"기준 도시", "HOME"};
const char* const kFromHome[2] = {"기준 도시보다 %c%d:%02d", "%c%d:%02d from home"};
const char* const kDst[2] = {"  서머타임", "  DST"};

void drawCentered(u8g2_t* u, int y, const char* s) {
    u8g2_DrawUTF8(u, (128 - u8g2_GetUTF8Width(u, s)) / 2, y, s);
}

}  // namespace

void drawMessage(u8g2_t* u, const char* const* lines, int count) {
    u8g2_ClearBuffer(u);
    u8g2_SetFontMode(u, 1);
    u8g2_SetDrawColor(u, 1);
    u8g2_SetFont(u, kFontKr);
    for (int i = 0; i < count && i < 5; i++) {
        if (lines[i]) drawCentered(u, 11 + i * 12, lines[i]);
    }
}

void drawToast(u8g2_t* u, const char* text) {
    u8g2_SetFontMode(u, 1);
    u8g2_SetFont(u, kFontKr);
    const int w = u8g2_GetUTF8Width(u, text) + 16;
    const int x = (128 - w) / 2;
    u8g2_SetDrawColor(u, 0);
    u8g2_DrawBox(u, 0, 12, 128, 43);          // 큰 숫자 줄 전체를 지워 딱지 둘레에 숫자 조각이 남지 않게
    u8g2_SetDrawColor(u, 1);
    u8g2_DrawFrame(u, x, 23, w, 19);
    u8g2_DrawUTF8(u, x + 8, 36, text);
}

void drawInfoCard(u8g2_t* u, const InfoCard& c) {
    char date[32], utc[16], offset[32], diff[40];
    snprintf(date, sizeof(date), "%d-%02d-%02d %s", c.year, c.month, c.day, kWeekdays[c.lang][c.weekday % 7]);
    formatUtcOffset(utc, sizeof(utc), c.utcOffsetMin);
    snprintf(offset, sizeof(offset), "%s%s", utc, c.dst ? kDst[c.lang] : "");
    if (c.isHome) {
        snprintf(diff, sizeof(diff), "%s", kHome[c.lang]);
    } else {
        const int a = abs(c.homeDiffMin);
        snprintf(diff, sizeof(diff), kFromHome[c.lang], c.homeDiffMin < 0 ? '-' : '+', a / 60, a % 60);
    }
    const char* const lines[5] = {c.city, nullptr, date, offset, diff};
    drawMessage(u, lines, 5);
    u8g2_DrawHLine(u, 16, 16, 96);
}
