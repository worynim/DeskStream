// worynim@gmail.com
/**
 * @file ds_canvas.cpp
 * @brief 512x64 캔버스 → 화면별 페이지 버퍼 전치 구현
 */
#include "ds_canvas.h"

namespace ds {

void canvasToScreenPages(const uint8_t* canvas, uint16_t canvasWidth, uint8_t column, uint8_t* out) {
    const int stride = canvasWidth / 8;            // 캔버스 한 행의 바이트 수
    const int x0 = column * 128;
    for (int p = 0; p < 8; p++) {
        const uint8_t* rows = canvas + (p * 8) * stride;   // 이 페이지의 첫 행
        for (int x = 0; x < 128; x++) {
            const int gx = x0 + x;
            const int byteIdx = gx >> 3;
            const uint8_t mask = (uint8_t)(0x80 >> (gx & 7));
            uint8_t v = 0;
            for (int b = 0; b < 8; b++) {
                if (rows[b * stride + byteIdx] & mask) v |= (uint8_t)(1 << b);
            }
            out[p * 128 + x] = v;
        }
    }
}

}  // namespace ds
