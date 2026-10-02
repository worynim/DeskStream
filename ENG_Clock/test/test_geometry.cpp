// worynim@gmail.com
/**
 * @file test_geometry.cpp
 * @brief CellGeometry 판별 로직 네이티브 단위 테스트
 * @details 빌드: g++ -std=c++11 -I.. test_geometry.cpp ../renderer_geometry.cpp -o test_geometry
 * @note renderer.cpp는 U8g2/LittleFS에 의존하므로 직접 컴파일할 수 없다.
 *       기하 판별은 순수 로직이므로 별도 TU(renderer_geometry.cpp)로 분리해 검증한다.
 *
 * @warning 회귀 검증의 핵심: 512B(현대 한글)와 256B(구 레거시)의 값이
 *          원본 renderer.cpp의 하드코딩과 **완전히 동일**해야 한다.
 *          값이 하나라도 바뀌면 한글판 OLED 출력이 조용히 달라진다.
 */
#include <cstdio>
#include <cstring>
#include "renderer_geometry.h"

static int g_pass = 0;
static int g_fail = 0;

static void check(const char* label, bool ok) {
    if (ok) { g_pass++; }
    else { g_fail++; printf("  FAIL  %s\n", label); }
}

static void testKoreanRegression() {
    printf("한글 폰트 회귀 검증 (원본 하드코딩과 동일해야 함)\n");

    // 원본: if (size <= 256) drawBitmap(x, y, 4, 64, data);
    //       else               drawBitmap(x - 16, y, 8, 64, data);
    const CellGeometry* k512 = geometryForSize(512);
    check("512B 존재", k512 != nullptr);
    if (k512) {
        check("512B bytesPerRow == 8", k512->bytesPerRow == 8);
        check("512B glyphH == 64",     k512->glyphH == 64);
        check("512B drawW == 64",      k512->drawW == 64);
        check("512B xOffset == -16 (원본 x-16)", k512->xOffset == -16);
    }

    // 원본: size <= 256 → 4, 64, xOffset 0
    const CellGeometry* k256 = geometryForSize(256);
    check("256B 존재", k256 != nullptr);
    if (k256) {
        check("256B bytesPerRow == 4", k256->bytesPerRow == 4);
        check("256B glyphH == 64",     k256->glyphH == 64);
        check("256B drawW == 32",      k256->drawW == 32);
        check("256B xOffset == 0 (원본 x)", k256->xOffset == 0);
    }
}

static void testEnglishGeometry() {
    printf("영어 기하 검증 (PLAN §3.1)\n");
    const CellGeometry* e64 = geometryForSize(64);
    check("64B 존재", e64 != nullptr);
    if (e64) {
        check("64B glyphW == 14",      e64->glyphW == 14);
        check("64B glyphH == 32",      e64->glyphH == 32);
        check("64B bytesPerRow == 2 (바이트 정렬)", e64->bytesPerRow == 2);
        check("64B drawW == 16",       e64->drawW == 16);
        check("64B xOffset == 0",      e64->xOffset == 0);
        check("64B maxPerLine == 9",   e64->maxPerLine == 9);
        // 화면 안에 들어가는가: 9 × 14 = 126 ≤ 128
        check("9자 폭 126 ≤ 128",      e64->maxPerLine * e64->glyphW <= 128);
    }

    // [여백 수정 v4 — PLAN §6.13] 확장 래스터: 잉크가 피치(14)를 넘어 옆 글자와 겹친다.
    printf("영어 확장 래스터 검증 (GEOM_192B)\n");
    const CellGeometry* e192 = geometryForSize(192);
    check("192B 존재", e192 != nullptr);
    if (e192) {
        check("192B glyphW == 14 (피치 불변)",   e192->glyphW == 14);
        check("192B glyphH == 32 (줄 밴드 유지)", e192->glyphH == 32);
        check("192B bytesPerRow == 6 (48px 래스터)", e192->bytesPerRow == 6);
        check("192B drawW == 48",      e192->drawW == 48);
        check("192B xOffset == -17 (잉크 중앙 = 피치 중앙)", e192->xOffset == -17);
        check("192B xOffset == -(drawW-glyphW)/2 (정합성)",
              e192->xOffset == -((int)e192->drawW - (int)e192->glyphW) / 2);
        check("192B maxPerLine == 9",  e192->maxPerLine == 9);
        check("192B 9자 폭 126 ≤ 128", e192->maxPerLine * e192->glyphW <= 128);
    }

    // [여백 수정 v5 — PLAN §6.14] 48×64 래스터: 잉크 합집합이 32px 밴드를 넘는 큰 폰트에서
    // 위가 잘리지 않도록 래스터를 화면 전체 높이로 늘렸다 (렌더러가 밴드 중앙에 놓는다).
    printf("영어 확장 래스터 검증 (GEOM_384B)\n");
    const CellGeometry* e384 = geometryForSize(384);
    check("384B 존재", e384 != nullptr);
    if (e384) {
        check("384B glyphW == 14 (피치 불변)",   e384->glyphW == 14);
        check("384B glyphH == 64 (화면 전체 높이)", e384->glyphH == 64);
        check("384B bytesPerRow == 6 (48px 래스터)", e384->bytesPerRow == 6);
        check("384B drawW == 48",      e384->drawW == 48);
        check("384B xOffset == -17 (잉크 중앙 = 피치 중앙)", e384->xOffset == -17);
        check("384B xOffset == -(drawW-glyphW)/2 (정합성)",
              e384->xOffset == -((int)e384->drawW - (int)e384->glyphW) / 2);
        check("384B maxPerLine == 9",  e384->maxPerLine == 9);
        check("384B 9자 폭 126 ≤ 128", e384->maxPerLine * e384->glyphW <= 128);
    }

    // [v5] 1줄 밴드 상단(16)에서 신형 384B는 화면 전체(0..64)를 쓴다 — 상단 클리핑 없음.
    if (e384) {
        const int yTop = 16 + (32 - (int)e384->glyphH) / 2;   // rasterTopY 공식 재현
        check("384B: 1줄 밴드에서 래스터가 0..64 (잉크 64px 수용)",
              yTop == 0 && yTop + (int)e384->glyphH == 64);
    }

    const CellGeometry& d = defaultGeometry();
    check("기본 기하는 64B 영어", d.bytesPerRow == 2 && d.glyphH == 32);
}

static void testUnknownSize() {
    printf("알 수 없는 크기 처리\n");
    check("0B 거부",     geometryForSize(0) == nullptr);
    check("1B 거부",     geometryForSize(1) == nullptr);
    check("63B 거부",    geometryForSize(63) == nullptr);
    check("65B 거부",    geometryForSize(65) == nullptr);
    check("128B 거부",   geometryForSize(128) == nullptr);
    check("191B 거부",   geometryForSize(191) == nullptr);
    check("193B 거부",   geometryForSize(193) == nullptr);
    check("255B 거부",   geometryForSize(255) == nullptr);
    check("380B 거부",   geometryForSize(380) == nullptr);   // 384B 인접 크기 (v5)
    check("383B 거부",   geometryForSize(383) == nullptr);
    check("385B 거부",   geometryForSize(385) == nullptr);
    check("511B 거부",   geometryForSize(511) == nullptr);
    check("513B 거부",   geometryForSize(513) == nullptr);
    check("1024B 거부",  geometryForSize(1024) == nullptr);
}

/** 기하가 self-consistent한지: drawW는 bytesPerRow의 8배여야 한다 */
static void testSelfConsistency() {
    printf("기하 테이블 자체 정합성\n");
    const uint32_t sizes[] = {64, 192, 384, 256, 512};
    for (size_t i = 0; i < 5; i++) {
        const CellGeometry* g = geometryForSize(sizes[i]);
        if (!g) { check("크기 존재", false); continue; }
        char msg[96];
        snprintf(msg, sizeof(msg), "%uB: drawW == bytesPerRow*8", sizes[i]);
        check(msg, g->drawW == g->bytesPerRow * 8);
        snprintf(msg, sizeof(msg), "%uB: size == bytesPerRow*glyphH", sizes[i]);
        check(msg, sizes[i] == (uint32_t)g->bytesPerRow * g->glyphH);
        snprintf(msg, sizeof(msg), "%uB: maxPerLine*glyphW ≤ 128", sizes[i]);
        check(msg, g->maxPerLine * g->glyphW <= 128);
    }
}

/**
 * [간격 확장 §6.16] inkWidthOf() — 레이아웃 피치의 상한이 되는 실제 잉크 폭.
 * 비트 순서는 U8g2 drawBitmap 규약(MSB 우선)이라 0x80이 **가장 왼쪽 열**이다.
 * 이 순서를 뒤집으면 잉크 폭이 틀어져 큰 폰트의 간격이 과하게 벌어진다.
 */
static void testInkWidth() {
    printf("잉크 폭 측정 (inkWidthOf)\n");

    check("NULL → 0",        inkWidthOf(NULL, 6, 64) == 0);
    check("bytesPerRow 0 → 0", inkWidthOf((const uint8_t*)"\xFF", 0, 64) == 0);
    check("glyphH 0 → 0",    inkWidthOf((const uint8_t*)"\xFF", 6, 0) == 0);

    // 6바이트(=48열) × 1행 버퍼를 만들어 열 단위로 검증한다.
    uint8_t row[6];
    memset(row, 0, sizeof(row));

    check("완전히 빈 글리프 → 0", inkWidthOf(row, 6, 1) == 0);

    row[0] = 0x80;   // 0열
    check("0열만 → 1",  inkWidthOf(row, 6, 1) == 1);

    row[0] = 0x01;   // 7열
    check("MSB 우선 — 0x01은 7열", inkWidthOf(row, 6, 1) == 1);
    row[0] = 0x81;   // 0열 + 7열
    check("0..7열 → 8", inkWidthOf(row, 6, 1) == 8);

    memset(row, 0, sizeof(row));
    row[0] = 0x80;   // 0열
    row[5] = 0x01;   // 47열 (48px 래스터의 마지막 열)
    check("0..47열 → 48 (래스터 전체)", inkWidthOf(row, 6, 1) == 48);

    memset(row, 0, sizeof(row));
    row[1] = 0x40;   // 9열
    row[4] = 0x02;   // 38열
    check("9..38열 → 30", inkWidthOf(row, 6, 1) == 30);

    // 세로로만 긴 글리프(세로선) — 폭은 1이어야 한다
    memset(row, 0, sizeof(row));
    uint8_t col[6 * 4];
    memset(col, 0, sizeof(col));
    for (int r = 0; r < 4; r++) col[r * 6] = 0x10;   // 3열만 4행
    check("세로선(3열 × 4행) → 1", inkWidthOf(col, 6, 4) == 1);

    // 중간 열에 구멍이 뚫린 글리프 — 폭은 양끝 기준이어야 한다
    memset(col, 0, sizeof(col));
    col[0] = 0x80;   // 0열
    col[1] = 0x01;   // 15열
    check("구멍이 있어도 양끝 기준 → 16", inkWidthOf(col, 6, 2) == 16);

    // 384B 기하(6B × 64행) 크기 버퍼 — 실제 업로드 형식과 같은 형태
    static uint8_t glyph384[6 * 64];
    memset(glyph384, 0, sizeof(glyph384));
    for (int r = 0; r < 64; r++) glyph384[r * 6 + 2] = 0xF0;   // 17..20열
    check("384B 기하 17..20열 → 4", inkWidthOf(glyph384, 6, 64) == 4);
    check("384B 기하 48px 넘지 않음", inkWidthOf(glyph384, 6, 64) <= 48);

    // [회귀] 행 우선 순회의 함정 — 위쪽이 넓고 아래쪽이 좁은 글리프("W" 모양).
    //   순회가 행 우선이므로 "마지막으로 만난 잉크 픽셀"은 최우열이 아니라
    //   **마지막 잉크 행의** 우끝이다. 예전 구현은 그걸 최우열로 착각했다.
    //   조건이 중요하다: 하단 4행에도 잉크가 있어야 한다(마지막 잉크 행 자체가
    //   좁아야 함). 하단을 통째로 비우면 마지막 잉크 행이 넓은 윗행이 되어
    //   버그가 드러나지 않는다 — 처음에 그렇게 짜서 회귀 테스트가 통과해 버렸다.
    //   col은 4행 버퍼라 8행을 담을 수 없다 — 여기를 건드리면 스택이 깨진다.
    uint8_t wedge[6 * 8];
    memset(wedge, 0, sizeof(wedge));
    for (int r = 0; r < 4; r++) {          // 상단: 0, 8, 16, 32, 40열
        wedge[r * 6 + 0] = 0x80;           // 0열
        wedge[r * 6 + 1] = 0x80;           // 8열
        wedge[r * 6 + 2] = 0x80;           // 16열
        wedge[r * 6 + 4] = 0x80;           // 32열
        wedge[r * 6 + 5] = 0x80;           // 40열 ← 상단에만
    }
    for (int r = 4; r < 8; r++) {          // 하단: 40열이 없고 잉크는 남아 있음
        wedge[r * 6 + 0] = 0x80;
        wedge[r * 6 + 1] = 0x80;
        wedge[r * 6 + 2] = 0x80;
        wedge[r * 6 + 4] = 0x80;
    }
    check("회귀: 위가 넓은 글리프 0..40열 → 41 (구버그는 33)", inkWidthOf(wedge, 6, 8) == 41);

    // 좌끝도 min이어야 한다 — 최좌열이 **아래쪽 행에만** 있는 글리프.
    memset(wedge, 0, sizeof(wedge));
    wedge[0 * 6 + 1] = 0x20;               // 10열 — 최상단에만
    for (int r = 1; r < 8; r++) {          // 7..30열 — 나머지 행
        wedge[r * 6 + 0] = 0x01;           // 7열   ← 최좌열이 여기에만
        wedge[r * 6 + 1] = 0xFF;           // 8..15열
        wedge[r * 6 + 2] = 0xFF;           // 16..23열
        wedge[r * 6 + 3] = 0xFE;           // 24..30열
    }
    check("회귀: 좌끝도 min — 7..30열 → 24 (구버그는 21)", inkWidthOf(wedge, 6, 8) == 24);
}

int main() {
    printf("=== CellGeometry 단위 테스트 ===\n\n");
    testKoreanRegression();
    testEnglishGeometry();
    testUnknownSize();
    testSelfConsistency();
    testInkWidth();

    printf("\n=== 결과: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
