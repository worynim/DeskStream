// worynim@gmail.com
/**
 * @file setup_view.cpp
 * @brief 장소 설정 화면 렌더러 구현
 */
#include "setup_view.h"

#include <stdio.h>
#include <stdlib.h>

#include "clock_view.h"
#include "fonts.h"
#include "world_map.h"

namespace {

// 설정 화면 글자 {한글, 영문}
const char* const kWest[2] = {"서쪽", "WEST"};
const char* const kEast[2] = {"동쪽", "EAST"};
const char* const kSave[2] = {"누르면 저장", "PRESS TO SAVE"};
const char* const kCancel[2] = {"취소", "CANCEL"};
const char* const kDst[2] = {"  서머타임", "  DST"};
const char* const kHourDiff[2] = {"%s  %s %+d시간", "%s  %s %+dh"};

void beginScreen(u8g2_t* u) {
    u8g2_ClearBuffer(u);
    u8g2_SetFontMode(u, 1);
    u8g2_SetDrawColor(u, 1);
    u8g2_SetFont(u, kFontKr);
}

void drawCentered(u8g2_t* u, int y, const char* s) {
    u8g2_DrawUTF8(u, (128 - u8g2_GetUTF8Width(u, s)) / 2, y, s);
}

/** 반전된 띠 안에 가운데 맞춤 글자 (y = 띠의 위쪽, 높이 11) */
void drawLabelBar(u8g2_t* u, int x, int y, int w, const char* s) {
    u8g2_DrawBox(u, x, y, w, 11);
    u8g2_SetDrawColor(u, 0);
    u8g2_DrawUTF8(u, x + (w - u8g2_GetUTF8Width(u, s)) / 2, y + 9, s);
    u8g2_SetDrawColor(u, 1);
}

}  // namespace

SetupRole setupRole(int editScreen, int screen) {
    if (screen == editScreen) return SETUP_EDIT;
    static const SetupRole kOrder[3] = {SETUP_MAP, SETUP_WEST, SETUP_EAST};
    return kOrder[screen < editScreen ? screen : screen - 1];
}

void formatUtcOffset(char* out, int size, int offsetMin) {
    const int a = abs(offsetMin);
    if (a % 60 == 0) {
        snprintf(out, size, "UTC%c%d", offsetMin < 0 ? '-' : '+', a / 60);
    } else {
        snprintf(out, size, "UTC%c%d:%02d", offsetMin < 0 ? '-' : '+', a / 60, a % 60);
    }
}

void drawSetupPreview(u8g2_t* u, const SetupPreview& p) {
    beginScreen(u);
    if (p.frameOn) {
        u8g2_DrawFrame(u, 0, 0, 128, 64);
        u8g2_DrawFrame(u, 1, 1, 126, 62);
    }
    drawCentered(u, 13, p.city);
    drawDayNightIcon(u, 112, 11, p.valid ? isDaytime(p.hour) : true);

    char buf[48];
    if (p.valid) {
        const int h12 = p.hour % 12 == 0 ? 12 : p.hour % 12;
        snprintf(buf, sizeof(buf), "%s %d:%02d", p.hour < 12 ? "AM" : "PM", h12, p.minute);
    } else {
        snprintf(buf, sizeof(buf), "--:--");
    }
    u8g2_SetFont(u, u8g2_font_fub14_tr);
    u8g2_DrawStr(u, (128 - u8g2_GetStrWidth(u, buf)) / 2, 33, buf);

    u8g2_SetFont(u, kFontKr);
    char utc[16];
    formatUtcOffset(utc, sizeof(utc), p.utcOffsetMin);
    if (p.homeCity) {
        const int a = abs(p.homeDiffMin);
        if (a % 60 == 0) {
            snprintf(buf, sizeof(buf), kHourDiff[p.lang], utc, p.homeCity, p.homeDiffMin / 60);
        } else {
            snprintf(buf, sizeof(buf), "%s  %s %c%d:%02d", utc, p.homeCity, p.homeDiffMin < 0 ? '-' : '+', a / 60, a % 60);
        }
    } else {
        snprintf(buf, sizeof(buf), "%s%s", utc, p.dst ? kDst[p.lang] : "");
    }
    drawCentered(u, 46, buf);
    drawLabelBar(u, 4, 50, 120, kSave[p.lang]);
}

void drawSetupMove(u8g2_t* u, Lang lang, bool east, const char* nextCity, int nextUtcOffsetMin) {
    beginScreen(u);
    const int tipX = east ? 116 : 11, baseX = east ? 96 : 31;       // 화살표는 바깥쪽을 가리킨다
    u8g2_DrawTriangle(u, tipX, 20, baseX, 6, baseX, 34);
    const char* label = east ? kEast[lang] : kWest[lang];
    const int labelW = u8g2_GetUTF8Width(u, label);
    u8g2_DrawUTF8(u, east ? baseX - 8 - labelW : baseX + 8, 24, label);

    u8g2_DrawHLine(u, 0, 40, 128);
    char utc[16], buf[48];
    formatUtcOffset(utc, sizeof(utc), nextUtcOffsetMin);
    snprintf(buf, sizeof(buf), "%s  %s", nextCity, utc);
    drawCentered(u, 55, buf);
}

void drawSetupMap(u8g2_t* u, Lang lang, int mapX, int mapY, bool markerOn) {
    beginScreen(u);
    u8g2_SetBitmapMode(u, 0);
    u8g2_DrawXBMP(u, 0, 0, 128, 64, kWorldMapBits);
    u8g2_SetDrawColor(u, 2);                                         // XOR — 육지·바다 어디서나 보이는 세로선
    u8g2_DrawVLine(u, mapX, 0, 64);
    u8g2_SetDrawColor(u, 1);
    if (markerOn) {
        u8g2_SetDrawColor(u, 0);
        u8g2_DrawDisc(u, mapX, mapY, 4, U8G2_DRAW_ALL);
        u8g2_SetDrawColor(u, 1);
        u8g2_DrawDisc(u, mapX, mapY, 2, U8G2_DRAW_ALL);
        u8g2_DrawCircle(u, mapX, mapY, 4, U8G2_DRAW_ALL);
    }
    // 취소 딱지는 도시 점과 겹치지 않는 아래쪽 구석에 둔다
    const int w = u8g2_GetUTF8Width(u, kCancel[lang]) + 12;
    const int labelX = mapX < 64 ? 128 - w : 0;
    u8g2_SetDrawColor(u, 0);
    u8g2_DrawBox(u, labelX - 1, 51, w + 2, 13);
    u8g2_SetDrawColor(u, 1);
    drawLabelBar(u, labelX, 52, w, kCancel[lang]);
}
