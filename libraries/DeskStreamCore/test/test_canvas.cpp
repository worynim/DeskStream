// worynim@gmail.com
/**
 * @file test_canvas.cpp
 * @brief 512x64 캔버스 → SSD1306 페이지 전치 회귀 테스트
 * @details 실행: bash test/run_all.sh
 * @note 고정하는 것: 행 우선·MSB 먼저 캔버스의 픽셀 (x, y)가 화면 x/128 의 페이지 버퍼
 *       바이트 (y/8)*128 + x%128 의 비트 y%8 로 간다 (Sound_Visualizer·Game_Of_Life 전치와 같은 규칙).
 */
#include <cstdio>
#include <cstring>
#include "display/ds_canvas.h"

using namespace ds;

static int g_pass = 0, g_fail = 0;
static void check(const char* label, bool ok) {
    if (ok) g_pass++;
    else { g_fail++; printf("  FAIL  %s\n", label); }
}

static uint8_t canvas[64 * 64];   // 512x64 / 8
static void setPx(int x, int y) { canvas[y * 64 + (x >> 3)] |= (uint8_t)(0x80 >> (x & 7)); }
static bool pagePx(const uint8_t* out, int x, int y) { return out[(y >> 3) * 128 + x] & (1 << (y & 7)); }

int main() {
    uint8_t out[1024];

    memset(canvas, 0, sizeof(canvas));
    setPx(0, 0); setPx(127, 63); setPx(128, 5); setPx(300, 9); setPx(511, 63);

    canvasToScreenPages(canvas, 512, 0, out);
    check("화면0 (0,0) → 페이지0 바이트0 비트0", out[0] == 0x01);
    check("화면0 (127,63) → 페이지7 바이트127 비트7", out[7 * 128 + 127] == 0x80);
    int on = 0; for (int i = 0; i < 1024; i++) on += __builtin_popcount(out[i]);
    check("화면0 켜진 픽셀은 2개뿐 (옆 화면 픽셀이 새지 않는다)", on == 2);

    canvasToScreenPages(canvas, 512, 1, out);
    check("화면1 (128,5) → 로컬 (0,5)", pagePx(out, 0, 5));
    canvasToScreenPages(canvas, 512, 2, out);
    check("화면2 (300,9) → 로컬 (44,9)", pagePx(out, 300 - 256, 9));
    canvasToScreenPages(canvas, 512, 3, out);
    check("화면3 (511,63) → 로컬 (127,63)", pagePx(out, 127, 63));

    // 전 픽셀 왕복: 무작위 패턴을 전치한 뒤 픽셀 단위로 원본과 대조
    unsigned seed = 12345;
    for (int i = 0; i < (int)sizeof(canvas); i++) { seed = seed * 1103515245u + 12345u; canvas[i] = (uint8_t)(seed >> 16); }
    bool all = true;
    for (int c = 0; c < 4 && all; c++) {
        canvasToScreenPages(canvas, 512, (uint8_t)c, out);
        for (int y = 0; y < 64 && all; y++)
            for (int x = 0; x < 128 && all; x++) {
                const int gx = c * 128 + x;
                const bool src = canvas[y * 64 + (gx >> 3)] & (0x80 >> (gx & 7));
                if (src != pagePx(out, x, y)) all = false;
            }
    }
    check("무작위 패턴 4화면 전 픽셀 일치", all);

    printf("test_canvas: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
