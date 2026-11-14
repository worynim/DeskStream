// worynim@gmail.com
/**
 * @file fonts.cpp
 * @brief 한글 글꼴(갈무리7) 정의
 */
// 갈무리7 한글 글꼴은 64KB 를 넘어 U8g2 가 큰 글꼴 지원을 켜야 한다 — u8g2.h 보다 먼저 정의
#ifndef U8G2_USE_LARGE_FONTS
#define U8G2_USE_LARGE_FONTS
#endif
#include "fonts.h"

#include "clib/u8g2.h"

#include "u8g2_font_galmuri7_kr.h"

const uint8_t* const kFontKr = u8g2_font_galmuri7_kr;
