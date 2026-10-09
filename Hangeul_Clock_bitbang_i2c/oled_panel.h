// worynim@gmail.com
/**
 * @file oled_panel.h
 * @brief `OLED_DRIVER` 선택에 따라 U8g2 클래스를 고르는 얇은 헤더
 * @details 컨트롤러(SH1106 / SSD1315 / SSD1306)마다 U8G2 클래스가 다르지만
 *          생성자 시그니처는 `(rotation, clock, data, reset)` 로 동일하다.
 *          그래서 typedef 하나로 갈아끼울 수 있다.
 *
 *          config.h는 U8g2를 포함하지 않으므로 이 헤더를 따로 둔다 —
 *          DisplayManager와 진단 코드가 **같은 타입**을 쓰게 하는 정의처다.
 */
#ifndef OLED_PANEL_H
#define OLED_PANEL_H

#include <U8g2lib.h>
#include "config.h"

#if OLED_DRIVER == OLED_DRIVER_SH1106
typedef U8G2_SH1106_128X64_NONAME_F_SW_I2C OledPanel;
#elif OLED_DRIVER == OLED_DRIVER_SSD1315
typedef U8G2_SSD1315_128X64_NONAME_F_SW_I2C OledPanel;
#elif OLED_DRIVER == OLED_DRIVER_SSD1306
typedef U8G2_SSD1306_128X64_NONAME_F_SW_I2C OledPanel;
#else
#error "OLED_DRIVER 값이 올바르지 않습니다"
#endif

#endif  // OLED_PANEL_H
