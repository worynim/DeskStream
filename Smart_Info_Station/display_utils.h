#ifndef DISPLAY_UTILS_H
#define DISPLAY_UTILS_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "driver/gpio.h"

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

/**
 * 🚀 극한의 고속 SW I2C 커스텀 콜백 (Direct Register Access)
 */
extern "C" uint8_t u8x8_gpio_and_delay_esp32_c3_fast(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

#endif
