// worynim@gmail.com
/**
 * @file ds_logger.h
 * @brief 부팅 로그 (시리얼 + OLED 한 화면 4줄)
 * @details 시리얼에는 "[LOG] ..."로 찍고, 화면이 연결되어 있으면 최근 4줄을 그린다.
 *          updateLastLog()는 "WiFi Connecting..."처럼 점이 늘어나는 진행 표시를
 *          시리얼에 한 줄로 이어 찍는다.
 * @note 출처: Hangeul_Clock/logger.* (v2.10.0). display 전역 대신 attachDisplay()로 받는다.
 */
#ifndef DS_LOGGER_H
#define DS_LOGGER_H

#include <Arduino.h>
#include "../display/ds_display.h"

namespace ds {

class Logger {
public:
    /** 로그를 그릴 화면을 연결한다. 연결하지 않으면 시리얼에만 찍는다 */
    void attachDisplay(DisplayCore* dc, uint8_t screenIdx = 0, const uint8_t* font = u8g2_font_6x10_tf);

    /** 새 로그 줄을 추가한다 (4줄을 넘으면 위로 민다) */
    void addLog(const String& msg);

    /** 가장 최근 줄의 내용을 바꾼다 (진행 점 애니메이션용) */
    void updateLastLog(const String& msg);

private:
    void render();

    DisplayCore* _dc = nullptr;
    uint8_t _screenIdx = 0;
    const uint8_t* _font = nullptr;
    String _lines[4];
    int _count = 0;
};

}  // namespace ds

#endif
