// worynim@gmail.com
/**
 * @file canvas512.h
 * @brief 4화면을 512x64 한 장처럼 그리는 도우미
 * @details 화면 s의 U8g2 버퍼에 x를 -128*s 만큼 옮겨 그린다. 그리기 함수를 화면마다 한 번씩(4번) 부른다.
 *   - 글자: U8g2에 그대로 넘긴다 — 음수 x도 경계에 걸쳐 정상으로 그려진다 (P0 확인).
 *   - 직선: U8g2 좌표가 부호 없는 16비트라 음수 x에서 drawLine이 잘못 그린다 → clipLineX로 화면 범위에 잘라 넘긴다.
 *   - 점·가로선·상자·테두리: 화면 범위로 잘라 넘긴다.
 */
#ifndef WA_CANVAS512_H
#define WA_CANVAS512_H

#include <DeskStreamCore.h>

#include "analysis.h"

namespace wa {

class Canvas512 {
public:
    Canvas512(U8G2& u, int screen) : _u(u), _left(screen * ds::kScreenWidth) {}

    U8G2& u8g2() { return _u; }

    void text(int x, int y, const char* s) { _u.drawUTF8(x - _left, y, s); }
    int textWidth(const char* s) { return _u.getUTF8Width(s); }

    void line(int x0, int y0, int x1, int y1) {
        int a, b, c, d;
        if (clipLineX(x0, y0, x1, y1, _left, right(), a, b, c, d)) _u.drawLine(a - _left, b, c - _left, d);
    }
    void pixel(int x, int y) {
        if (x >= _left && x <= right()) _u.drawPixel(x - _left, y);
    }
    void hline(int x, int y, int w) {
        int x0, x1;
        if (clipSpan(x, w, x0, x1)) _u.drawHLine(x0 - _left, y, x1 - x0 + 1);
    }
    void box(int x, int y, int w, int h) {
        int x0, x1;
        if (clipSpan(x, w, x0, x1)) _u.drawBox(x0 - _left, y, x1 - x0 + 1, h);
    }
    void frame(int x, int y, int w, int h) {
        hline(x, y, w);
        hline(x, y + h - 1, w);
        box(x, y, 1, h);
        box(x + w - 1, y, 1, h);
    }

private:
    int right() const { return _left + ds::kScreenWidth - 1; }
    /** [x, x+w) 를 이 화면 범위로 자른 [x0, x1]. 겹치지 않으면 false */
    bool clipSpan(int x, int w, int& x0, int& x1) const {
        x0 = x < _left ? _left : x;
        x1 = x + w - 1 > right() ? right() : x + w - 1;
        return w > 0 && x0 <= x1;
    }

    U8G2& _u;
    int _left;
};

}  // namespace wa

#endif
