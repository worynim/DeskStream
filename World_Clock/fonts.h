// worynim@gmail.com
/**
 * @file fonts.h
 * @brief 한글 글꼴(갈무리7) 선언 — 글꼴 데이터(166KB)가 한 번만 링크되도록 fonts.cpp 에서만 정의한다
 */
#ifndef FONTS_H
#define FONTS_H

#include <stdint.h>

/** 갈무리7: ASCII + 한글 음절 전체, 글자 높이 약 8px (기준선에서 위로 9px 잡으면 된다) */
extern const uint8_t* const kFontKr;

#endif
