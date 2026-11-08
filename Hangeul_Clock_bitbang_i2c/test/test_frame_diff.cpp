// worynim@gmail.com
/**
 * @file test_frame_diff.cpp
 * @brief 셰도 버퍼 페이지 diff(fdDiffPage) 회귀 테스트 (호스트 g++)
 * @details 빌드: g++ -std=c++11 -I.. test_frame_diff.cpp -o /tmp/test_frame_diff
 *          실행: /tmp/test_frame_diff
 *
 * @note [FPS 개선] fdDiffPage()는 예전 바이트 루프(get/setShadowData)를 대체했다.
 *       구간·셰도 결과가 **예전 루프와 똑같아야** 한다 — 아래 reference가 그 루프를 그대로 옮긴 것.
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "frame_diff.h"

static int g_pass = 0;
static int g_fail = 0;

static void expectInt(const char* label, int got, int want) {
    if (got == want) { g_pass++; }
    else { g_fail++; printf("  FAIL  [%s] got %d, want %d\n", label, got, want); }
}

/** 예전 diffAndForEachPage의 한 페이지 처리 (바이트 단위) — 기준 구현 */
static int referenceDiff(const uint8_t* cur, uint8_t* shadow, int tiles, int* first, int* count) {
    int f = -1, l = -1;
    for (int t = 0; t < tiles; t++) {
        bool dirty = false;
        for (int x = 0; x < 8; x++) {
            const int i = t * 8 + x;
            if (cur[i] != shadow[i]) { dirty = true; shadow[i] = cur[i]; }
        }
        if (dirty) { if (f < 0) f = t; l = t; }
    }
    if (f < 0) { *first = 0; *count = 0; return 0; }
    *first = f; *count = l - f + 1;
    return 1;
}

int main() {
    alignas(4) uint8_t cur[128];
    alignas(4) uint8_t shA[128];   // fdDiffPage용 셰도
    alignas(4) uint8_t shB[128];   // 기준 구현용 셰도

    // --- ① 변경 없음 ---
    memset(cur, 0x5A, sizeof(cur)); memcpy(shA, cur, 128);
    uint8_t f = 9, c = 9;
    expectInt("변경 없음 → 0", fdDiffPage(cur, shA, 16, &f, &c), 0);
    expectInt("변경 없음 → count 0", c, 0);

    // --- ② 타일 경계: 각 타일의 마지막 바이트 한 개만 바뀌어도 잡아야 한다 (상위 워드 비교) ---
    for (int t = 0; t < 16; t++) {
        memset(cur, 0, 128); memset(shA, 0, 128);
        cur[t * 8 + 7] = 1;
        fdDiffPage(cur, shA, 16, &f, &c);
        if (f != t || c != 1 || shA[t * 8 + 7] != 1) { g_fail++; printf("  FAIL  [타일 %d 마지막 바이트]\n", t); }
        else g_pass++;
    }

    // --- ③ 구간은 첫~마지막 바뀐 타일 (중간 미변경 포함) ---
    memset(cur, 0, 128); memset(shA, 0, 128);
    cur[2 * 8] = 1; cur[9 * 8 + 3] = 1;
    fdDiffPage(cur, shA, 16, &f, &c);
    expectInt("구간 시작", f, 2);
    expectInt("구간 타일 수", c, 8);

    // --- ④ 무작위 대조: 결과·구간·셰도가 기준 구현과 완전히 같아야 한다 ---
    srand(12345);
    int mismatch = 0;
    for (int iter = 0; iter < 20000; iter++) {
        for (int i = 0; i < 128; i++) shA[i] = (uint8_t)rand();
        memcpy(shB, shA, 128);
        memcpy(cur, shA, 128);
        const int flips = rand() % 6;               // 0~5곳만 바꾼다 (부분 변경이 실제 상황)
        for (int k = 0; k < flips; k++) cur[rand() % 128] ^= (uint8_t)(1 + rand() % 255);
        int rf, rc;
        const int rr = referenceDiff(cur, shB, 16, &rf, &rc);
        const int ar = fdDiffPage(cur, shA, 16, &f, &c);
        if (rr != ar || rf != f || rc != c || memcmp(shA, shB, 128) != 0) mismatch++;
    }
    expectInt("무작위 20000회 기준 구현과 일치", mismatch, 0);

    printf("test_frame_diff: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
