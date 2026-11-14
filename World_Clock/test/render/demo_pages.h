// worynim@gmail.com
/**
 * @file demo_pages.h
 * @brief 화면 미리보기용 고정 장면 — 실제 시각 대신 정해 둔 값으로 각 화면을 그린다 (PC 전용)
 */
#ifndef DEMO_PAGES_H
#define DEMO_PAGES_H

#include "clib/u8g2.h"

const int kDemoPageCount = 9;

/** 장면 이름 (영문, 파일 이름·시리얼 출력용) */
const char* demoPageName(int page);

/** 장면 page 의 화면 screen(0~3)을 그린다. blinkOn = 깜빡이는 요소(':'·테두리·지도 점)의 현재 상태 */
void drawDemoPage(int page, int screen, u8g2_t* u, bool blinkOn);

#endif
