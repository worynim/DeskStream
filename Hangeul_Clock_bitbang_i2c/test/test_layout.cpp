// worynim@gmail.com
/**
 * @file test_layout.cpp
 * @brief layoutCharX() 네이티브 단위 테스트
 * @details [리뷰 §1.2 회귀] P0 버그의 재발을 막는다.
 *          원본 getCharData()는 `if (centered || count == 1)`과 else 두 갈래를 탔고,
 *          else 갈래의 마지막 글자를 계산과 무관하게 x=96에 고정했다.
 *          count==1이면 그 글자가 j==0이면서 동시에 "마지막"이라 가운데 정렬
 *          (startX=48)을 무시하고 x=96에 놓였다 — 1자일 때 화면 밖으로 나가는 분기.
 *          이 테스트는 그것이 되돌아오지 않는지 확인한다.
 */
#include <cstdio>
#include "../renderer_layout.h"

static int passed = 0, failed = 0;

static void chk(const char* name, bool ok) {
    if (ok) { printf("  ok   %s\n", name); passed++; }
    else    { printf("  FAIL %s\n", name); failed++; }
}

// 리뷰에서 지적한 원본 공식 — "고치지 않았다면" 어떤 값이 나오는지 보여주는 기준선.
static int originalX(int count, int j, bool centered) {
    if (centered || count == 1) return (LAYOUT_SCREEN_W - count * LAYOUT_CELL_W) / 2 + j * LAYOUT_CELL_W;
    const int startX = (LAYOUT_RIGHT_COL_X - (count - 1) * LAYOUT_CELL_W) / 2;
    return (j == count - 1) ? LAYOUT_RIGHT_COL_X : startX + j * LAYOUT_CELL_W;
}

int main() {
    printf("=== renderer_layout (한글판 글자 배치) 단위 테스트 ===\n\n");

    printf("[핵심 회귀] 1자는 가운데 정렬 (원본은 x=96으로 화면 밖에 놓았다)\n");
    chk("count=1, 비정렬 → x=48 (원본 96과 다름)", layoutCharX(1, 0, false) == 48);
    chk("count=1, 정렬 → x=48", layoutCharX(1, 0, true) == 48);

    printf("\n모든 글자가 화면 안(0~127)에 들어간다\n");
    // 회귀가 있으면 여기서 잡힌다: x가 음수이거나 127을 넘으면 표시되지 않는다.
    bool inBounds = true;
    for (int count = 1; count <= LAYOUT_MAX_CHARS; count++) {
        for (int j = 0; j < count; j++) {
            const int x = layoutCharX(count, j, false);
            if (x < 0 || x + LAYOUT_CELL_W > LAYOUT_SCREEN_W) {
                inBounds = false;
                printf("  FAIL count=%d j=%d → x=%d (글자 폭 %d)\n", count, j, x, LAYOUT_CELL_W);
            }
        }
    }
    chk("비정렬 배치 1~LAYOUT_MAX_CHARS자 전부 화면 안", inBounds);

    bool inBoundsC = true;
    for (int count = 1; count <= LAYOUT_MAX_CHARS; count++) {
        for (int j = 0; j < count; j++) {
            const int x = layoutCharX(count, j, true);
            if (x < 0 || x + LAYOUT_CELL_W > LAYOUT_SCREEN_W) {
                inBoundsC = false;
                printf("  FAIL 정렬 count=%d j=%d → x=%d\n", count, j, x);
            }
        }
    }
    chk("정렬 배치 1~LAYOUT_MAX_CHARS자 전부 화면 안", inBoundsC);

    printf("\n[보존] count >= 2 는 원본과 값이 같아야 한다 (표준 배치 유지)\n");
    bool sameAsOriginal = true;
    for (int count = 2; count <= LAYOUT_MAX_CHARS; count++) {
        for (int j = 0; j < count; j++) {
            for (int c = 0; c <= 1; c++) {
                const bool centered = (c == 1);
                if (layoutCharX(count, j, centered) != originalX(count, j, centered)) {
                    sameAsOriginal = false;
                    printf("  FAIL count=%d j=%d centered=%d: 새=%d 원본=%d\n",
                           count, j, centered, layoutCharX(count, j, centered), originalX(count, j, centered));
                }
            }
        }
    }
    chk("count 2~LAYOUT_MAX_CHARS × 정렬/비정렬 전부 원본과 동일", sameAsOriginal);

    printf("\n기대값 (비정렬 — 마지막 글자는 항상 우측 고정 열)\n");
    chk("1자 → [48]", layoutCharX(1, 0, false) == 48);
    chk("2자 → [32, 96]", layoutCharX(2, 0, false) == 32 && layoutCharX(2, 1, false) == 96);
    chk("3자 → [16, 48, 96]", layoutCharX(3, 0, false) == 16 && layoutCharX(3, 1, false) == 48
        && layoutCharX(3, 2, false) == 96);
    chk("4자 → [0, 32, 64, 96] (4칸이 화면 전체를 채운다)",
        layoutCharX(4, 0, false) == 0 && layoutCharX(4, 3, false) == 96);

    printf("\n기대값 (정렬 — 화면 가운데)\n");
    chk("1자 → [48]", layoutCharX(1, 0, true) == 48);
    chk("2자 → [32, 64]", layoutCharX(2, 0, true) == 32 && layoutCharX(2, 1, true) == 64);
    chk("4자 → [0, 32, 64, 96]", layoutCharX(4, 0, true) == 0 && layoutCharX(4, 3, true) == 96);

    printf("\n방어선\n");
    chk("count=0 → 0 (음수 좌표 없음)", layoutCharX(0, 0, false) == 0);
    chk("글자 간격이 항상 LAYOUT_CELL_W",
        layoutCharX(4, 1, true) - layoutCharX(4, 0, true) == LAYOUT_CELL_W);

    printf("\n=== 결과: %d passed, %d failed ===\n", passed, failed);
    return failed == 0 ? 0 : 1;
}