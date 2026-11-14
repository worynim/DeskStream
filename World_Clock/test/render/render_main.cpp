// worynim@gmail.com
/**
 * @file render_main.cpp
 * @brief 화면 미리보기 도구 — 장치와 같은 장면(demo_pages)을 PC에서 그려 PBM 이미지로 저장
 * @details 사용: test/render/run.sh (결과는 인자로 준 폴더에 <이름>.pbm, 각 512x64 = 화면 4개를 이어 붙인 것)
 */
#include <stdio.h>
#include <string.h>
#include <string>

#include "demo_pages.h"

static uint8_t noopCb(u8x8_t*, uint8_t, uint8_t, void*) { return 1; }

// U8g2 기본 설정은 인스턴스들이 정적 버퍼 하나를 함께 쓴다 — U8g2 하나에 그리고 화면별로 복사해 둔다
static u8g2_t u8;
static u8g2_t* const u = &u8;
static uint8_t tiles[4][1024];

/** 방금 그린 버퍼를 화면 s 의 것으로 보관한다 */
static void keep(int s) { memcpy(tiles[s], u8g2_GetBufferPtr(u), sizeof(tiles[s])); }

/** 보관한 화면 4개(세로 8점 묶음, 128바이트 = 한 줄 띠)를 512x64 PBM 으로 쓴다 */
static void save(const std::string& dir, const char* name) {
    const std::string path = dir + "/" + name + ".pbm";
    FILE* fp = fopen(path.c_str(), "w");
    if (!fp) { perror(path.c_str()); return; }
    fprintf(fp, "P1\n512 64\n");
    for (int y = 0; y < 64; y++) {
        for (int s = 0; s < 4; s++) {
            const uint8_t* buf = tiles[s];
            for (int x = 0; x < 128; x++) fputc((buf[x + (y / 8) * 128] >> (y % 8)) & 1 ? '1' : '0', fp);
        }
        fputc('\n', fp);
    }
    fclose(fp);
}

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : ".";
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(u, U8G2_R0, noopCb, noopCb);
    for (int page = 0; page < kDemoPageCount; page++) {
        for (int s = 0; s < 4; s++) {
            drawDemoPage(page, s, u, true);
            keep(s);
        }
        save(dir, demoPageName(page));
    }
    return 0;
}
