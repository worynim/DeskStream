// worynim@gmail.com
/**
 * @file display_utils.h
 * @brief 디스플레이 유틸리티 함수 선언
 * @details 효율적인 OLED 렌더링을 위한 상수 및 헬퍼 함수 인터페이스 제공
 */
#ifndef DISPLAY_UTILS_H
#define DISPLAY_UTILS_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <DeskStreamCore.h>

/** 4-OLED 화면 엔진 (Smart_Info_Station.ino에서 정의) — 그린 뒤 display.push()로 바뀐 타일만 전송한다 */
extern ds::DisplayCore display;

/**
 * 문자열에 한글(UTF-8) 문자가 포함되어 있는지 확인합니다.
 */
bool containsKorean(String s);

/**
 * OLED 화면의 중앙에 텍스트를 출력합니다.
 */
void drawCenteredText(U8G2 &u8g2, String text, int y);

/**
 * U8g2 그래픽 그리기를 위한 공통 설정을 초기화합니다.
 */
void u8g2_prepare(U8G2 &u8g2);

/**
 * 업데이트 중일 때 OLED 화면에 안내 메시지를 표시합니다.
 */
void showUpdateMessage(U8G2 &u8g2, String msg);

#endif
