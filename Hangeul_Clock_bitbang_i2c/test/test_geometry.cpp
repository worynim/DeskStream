// worynim@gmail.com
/**
 * @file test_geometry.cpp
 * @brief renderer_geometry (글자 셀 기하 판별) 네이티브 단위 테스트
 * @details [리뷰 §1.1 회귀] P0 버그의 재발을 막는다.
 *          charBitWidth()가 `(size <= 256) ? 4 : 8`을 돌려주면 257~511바이트 파일이
 *          로드 검증을 통과한 뒤 그려질 때 8×64 = 512바이트로 읽혔다(malloc 크기는
 *          실제 파일 크기뿐 → 힙 경계 초과 읽음).
 *          geometryForSize()는 모르는 크기에 nullptr을 주므로 **초과 읽음이 로드 단계에서 차단**된다.
 */
#include <cstdio>
#include <cstdint>
#include "../renderer_geometry.h"

static int passed = 0, failed = 0;

static void chk(const char* name, bool ok) {
    if (ok) { printf("  ok   %s\n", name); passed++; }
    else    { printf("  FAIL %s\n", name); failed++; }
}

int main() {
    printf("=== renderer_geometry (한글판) 단위 테스트 ===\n\n");

    printf("알려진 크기\n");
    chk("256B → 32px 한글 (4B/행)", geometryForSize(256) != nullptr
        && geometryForSize(256)->bytesPerRow == 4
        && geometryForSize(256)->glyphH == 64
        && geometryForSize(256)->glyphW == 32
        && geometryForSize(256)->drawW == 32
        && geometryForSize(256)->xOffset == 0);
    chk("512B → 64px 한글 (8B/행)", geometryForSize(512) != nullptr
        && geometryForSize(512)->bytesPerRow == 8
        && geometryForSize(512)->glyphH == 64
        && geometryForSize(512)->glyphW == 64
        && geometryForSize(512)->drawW == 64
        && geometryForSize(512)->xOffset == -16);

    printf("\n[핵심 회귀] 알 수 없는 크기는 nullptr (초과 읽음 차단)\n");
    // 이것이 P0 버그의 근원이다. 257~511은 원본에서 8B/행으로 읽혔다.
    bool allRejected = true;
    for (uint32_t s = 257; s <= 511; s++) {
        if (geometryForSize(s) != nullptr) { allRejected = false; printf("  FAIL %uB가 통과했다\n", s); }
    }
    chk("257~511B 전부 거부된다 (원본은 512B로 읽어 넘침)", allRejected);

    // 그 밖의 흔한 오크기들
    const uint32_t rejected[] = {0, 1, 63, 65, 100, 128, 255, 384, 513, 600, 1024};
    bool others = true;
    for (size_t i = 0; i < sizeof(rejected)/sizeof(rejected[0]); i++) {
        if (geometryForSize(rejected[i]) != nullptr) {
            others = false;
            printf("  FAIL %uB가 통과했다\n", rejected[i]);
        }
    }
    chk("기타 알 수 없는 크기도 거부된다 (0/1/63/65/100/128/255/384/513/600/1024)", others);

    printf("\n기하 자체의 정합성\n");
    chk("drawW = bytesPerRow * 8",
        geometryForSize(256)->drawW == geometryForSize(256)->bytesPerRow * 8
        && geometryForSize(512)->drawW == geometryForSize(512)->bytesPerRow * 8);
    chk("크기 = bytesPerRow × glyphH (파일 크기와 기하가 일치)",
        (uint32_t)geometryForSize(256)->bytesPerRow * geometryForSize(256)->glyphH == 256
        && (uint32_t)geometryForSize(512)->bytesPerRow * geometryForSize(512)->glyphH == 512);
    chk("기본 기하는 32px 한글", defaultGeometry().bytesPerRow == 4 && defaultGeometry().glyphW == 32);
    chk("maxPerLine: 32px=4자, 64px=2자 (128px 화면 기준)",
        geometryForSize(256)->maxPerLine == 4 && geometryForSize(512)->maxPerLine == 2);

    printf("\n=== 결과: %d passed, %d failed ===\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
