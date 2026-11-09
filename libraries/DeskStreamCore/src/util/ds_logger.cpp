// worynim@gmail.com
/**
 * @file ds_logger.cpp
 * @brief 부팅 로그 구현
 */
#include "ds_logger.h"

namespace ds {

void Logger::attachDisplay(DisplayCore* dc, uint8_t screenIdx, const uint8_t* font) {
    _dc = dc;
    _screenIdx = screenIdx;
    _font = font;
}

void Logger::addLog(const String& msg) {
    Serial.println("[LOG] " + msg);
    if (_count < 4) {
        _lines[_count++] = msg;
    } else {
        for (int i = 0; i < 3; i++) _lines[i] = _lines[i + 1];   // 기존 로그 위로 밀기
        _lines[3] = msg;
    }
    render();
}

static int countDots(const String& s) {
    int n = 0;
    for (size_t i = 0; i < s.length(); i++) if (s[i] == '.') n++;
    return n;
}

void Logger::updateLastLog(const String& msg) {
    if (_count == 0) return;
    const String oldMsg = _lines[_count - 1];
    _lines[_count - 1] = msg;

    // 시리얼: 이전 메시지에 덧붙인 형태면 줄바꿈 없이 추가분만 찍는다
    if (msg.startsWith(oldMsg) && msg.length() > oldMsg.length()) {
        Serial.print(msg.substring(oldMsg.length()));
    } else {
        // 점이 리셋된 경우(..... → .)에도 같은 베이스면 한 줄에 이어서 점을 찍는다
        const int lastDotIdx = oldMsg.indexOf('.');
        const String base = (lastDotIdx != -1) ? oldMsg.substring(0, lastDotIdx) : oldMsg;
        if (msg.startsWith(base)) {
            const int dotsNew = countDots(msg), dotsOld = countDots(oldMsg);
            if (dotsNew > dotsOld) {
                for (int d = 0; d < dotsNew - dotsOld; d++) Serial.print(".");
            } else if (dotsNew == 1 && dotsOld >= 4) {
                Serial.print(".");
            }
        } else {
            Serial.println("\n[LOG] " + msg);
        }
    }
    render();
}

void Logger::render() {
    if (!_dc) return;
    U8G2& u = _dc->screen(_screenIdx);
    u.clearBuffer();
    u.setFont(_font);
    for (int i = 0; i < _count; i++) u.drawUTF8(0, (i + 1) * 15, _lines[i].c_str());
    _dc->push();   // 셰도 버퍼와 동기화된 경로로 보낸다 (잔상 방지)
}

}  // namespace ds
