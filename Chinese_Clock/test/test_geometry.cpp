// worynim@gmail.com
/**
 * @file test_geometry.cpp
 * @brief 글자 셀 기하(288B) 판별 검증
 * @details 빌드: g++ -std=c++11 -Wall -Wextra -I.. test_geometry.cpp renderer_geometry.cpp -o /tmp/test_geometry
 *          실행: /tmp/test_geometry
 *
 * @note 이 테스트는 PLAN §5.3·§5.4의 기하를 **수치로 고정**한다.
 *       기하는 웹 Font Studio와 JS 미러가 함께 쓰는 값이므로, 여기서 어긋나면
 *       브라우저 미리보기와 실제 OLED가 다른 글자를 그린다 (조용한 불일치).
 */
#include <cstdio>
#include <cstring>
#include "renderer_geometry.h"

static int g_pass = 0;
static int g_fail = 0;

static void checkUInt(const char* what, uint32_t got, uint32_t want) {
    if (got == want) { g_pass++; return; }
    g_fail++;
    printf("  FAIL  %-34s got %u, want %u\n", what, got, want);
}

static void checkInt(const char* what, int got, int want) {
    if (got == want) { g_pass++; return; }
    g_fail++;
    printf("  FAIL  %-34s got %d, want %d\n", what, got, want);
}

int main() {
    // === 1. 288B 매핑 (PLAN §5.4 표) ===
    const CellGeometry* g = geometryForSize(288);
    if (!g) {
        printf("  FAIL  geometryForSize(288) 가 nullptr을 반환했다\n");
        return 1;
    }
    g_pass++;
    checkUInt("288B glyphW",        g->glyphW,      32);
    checkUInt("288B glyphH",        g->glyphH,      48);
    checkUInt("288B bytesPerRow",   g->bytesPerRow, 6);
    checkUInt("288B drawW",         g->drawW,       48);
    checkInt ("288B xOffset",      g->xOffset,     -8);
    checkUInt("288B maxPerLine",    g->maxPerLine,  4);

    // 기하의 자기 정합성: drawW는 반드시 bytesPerRow × 8이어야 한다
    checkUInt("drawW == bytesPerRow × 8", g->drawW, (uint32_t)g->bytesPerRow * 8);

    // ★ 32px 피치 설계의 핵심 불변식 — 이게 깨지면 한 줄 4자가 화면을 벗어난다
    checkUInt("maxPerLine × glyphW == 화면 폭 128", (uint32_t)g->maxPerLine * g->glyphW, 128);
    // ★ 잉크가 래스터 안에 있어야 한다 (좌우 클리핑 배제 — ENG판 v4 재발 방어)
    checkInt ("xOffset == (glyphW − drawW) / 2", g->xOffset, (g->glyphW - g->drawW) / 2);
    // ★ 슬라이더 상한 40px가 래스터를 넘지 않아야 한다 (상하 클리핑 배제 — ENG판 v5 재발 방어)
    checkInt ("래스터 높이 48 ≥ 슬라이더 상한 40", g->glyphH, 48);

    // === 2. 경계: 287 / 289 는 거부, ENG판의 다른 크기도 모두 거부 ===
    const uint32_t rejected[] = { 0, 64, 192, 256, 287, 289, 384, 512, 1000 };
    const char* why[] = { "0(빈 파일)", "64(ENG 구형)", "192(ENG v4)", "256(ENG 구형)",
                          "287(1B 부족)", "289(1B 초과)", "384(ENG v5)", "512(한글판)",
                          "1000(알 수 없음)" };
    for (size_t i = 0; i < sizeof(rejected) / sizeof(rejected[0]); i++) {
        if (geometryForSize(rejected[i]) != nullptr) {
            g_fail++;
            printf("  FAIL  %uB (%s) 를 거부하지 않았다\n", rejected[i], why[i]);
        } else {
            g_pass++;
        }
    }

    // === 3. defaultGeometry는 288B와 동일한 값 ===
    const CellGeometry& d = defaultGeometry();
    checkUInt("defaultGeometry glyphW",     d.glyphW,      32);
    checkUInt("defaultGeometry glyphH",     d.glyphH,      48);
    checkUInt("defaultGeometry bytesPerRow",d.bytesPerRow, 6);
    checkInt ("defaultGeometry xOffset",    d.xOffset,     -8);
    checkUInt("defaultGeometry maxPerLine", d.maxPerLine,  4);
    // defaultGeometry가 반환하는 값은 테이블 항목 그 자체여야 한다 (복사본이 아니라 참조)
    if (&d != geometryForSize(288)) {
        g_fail++;
        printf("  FAIL  defaultGeometry가 테이블 항목의 참조가 아니다\n");
    } else {
        g_pass++;
    }

    // === 4. inkWidthOf — 디더·줌 애니메이션이 잉크 중심을 잡을 때 쓴다 (PLAN §6.5) ===
    {
        // (a) 빈 데이터 → 0
        uint8_t empty[288] = {0};
        checkUInt("inkWidthOf(전부 0)", inkWidthOf(empty, 6, 48), 0);

        // (b) 잘못된 인자 방어
        checkUInt("inkWidthOf(NULL)",  inkWidthOf(NULL, 6, 48), 0);
        checkUInt("inkWidthOf(bytes=0)", inkWidthOf(empty, 0, 48), 0);
        checkUInt("inkWidthOf(h=0)",   inkWidthOf(empty, 6, 0), 0);

        // (c) 첫 행 0x80만 켜짐 → 1열
        uint8_t one[288] = {0};
        one[0] = 0x80;
        checkUInt("inkWidthOf(1열)", inkWidthOf(one, 6, 48), 1);

        // (d) 첫 행 0xF0 → 4열 (MSB 우선 규약)
        uint8_t four[288] = {0};
        four[0] = 0xF0;
        checkUInt("inkWidthOf(MSB 4열)", inkWidthOf(four, 6, 48), 4);

        // (e) ★ 행 우선 순회 회귀 — 위쪽이 넓고 아래가 좁은 글리프(한자 '三'/'王' 형태)
        //     잉크 폭은 전체 48행에 걸친 min~max 열(0~39 = 40열)이다.
        //     "처음/마지막으로 만난 픽셀"만 쓰는 나쁜 구현은 마지막 행의 18열을
        //     최우열로 보고 20열로 잘못 잰다 → 겹침 해소가 안 되는 원인 (ENG §6.16).
        uint8_t wide[288] = {0};
        for (int b = 0; b < 5; b++) wide[0 * 6 + b] = 0xFF;   // 1행: 0~39열
        wide[47 * 6 + 2] = 0x20;                              // 마지막 행: 18열
        checkUInt("inkWidthOf(넓은 위/좁은 아래)", inkWidthOf(wide, 6, 48), 40);

        // (f) 둘째 바이트까지 건너뛴 경우 (빈 바이트 스킵이 동작하는지)
        uint8_t gap[288] = {0};
        gap[0 * 6 + 0] = 0x80;   // 0열
        gap[0 * 6 + 5] = 0x01;   // 47열
        checkUInt("inkWidthOf(빈 바이트 스킵)", inkWidthOf(gap, 6, 48), 47 - 0 + 1);
    }

    printf("test_geometry: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}