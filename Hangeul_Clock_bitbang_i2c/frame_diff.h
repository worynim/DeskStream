// worynim@gmail.com
/**
 * @file frame_diff.h
 * @brief 프레임 버퍼 ↔ 셰도 버퍼 페이지 diff (**순수 로직**, Arduino 의존 없음)
 * @details 한 페이지(128열 = 16타일 × 8바이트)에서 바뀐 타일 구간을 찾고,
 *          바뀐 타일은 셰도 버퍼에 곧바로 복사한다.
 *
 *          [FPS 개선] 예전 diff는 바이트마다 `getShadowData()`/`setShadowData()`를 불렀다
 *          (다른 번역 단위의 함수 + 경계 검사 → 인라인 불가). 4화면 × 1024바이트를 매 프레임
 *          그렇게 비교하니, 벤치마크의 "그 외(diff·캡처)" 2.3ms 중 상당 부분이 여기였다.
 *          이제 타일(8바이트)을 32비트 두 번으로 비교한다.
 *
 * @note 두 버퍼는 **4바이트 정렬**이어야 한다 (`alignas(4)`). 타일 오프셋은 8의 배수라
 *       페이지·타일 시작이 모두 정렬된다. RISC-V(ESP32-C3)의 비정렬 접근은 느리거나 트랩이다.
 * @note 호스트 테스트: test/test_frame_diff.cpp
 */
#ifndef FRAME_DIFF_H
#define FRAME_DIFF_H

#include <stdint.h>
#include <string.h>

/** 한 타일의 바이트 수 (SSD13xx/SH1106 페이지의 8열) */
#define FD_TILE_BYTES 8

/**
 * @brief 한 페이지를 비교해 바뀐 타일 구간을 구하고, 바뀐 타일을 셰도에 복사한다
 * @param cur    이번 프레임의 페이지 시작 (tiles × 8바이트, 4바이트 정렬)
 * @param shadow 셰도 버퍼의 같은 페이지 시작 (4바이트 정렬) — **바뀐 타일만 갱신된다**
 * @param tiles  페이지의 타일 수 (128열이면 16)
 * @param first  [출력] 첫 번째로 바뀐 타일
 * @param count  [출력] 첫~마지막 바뀐 타일까지의 타일 수 (중간의 안 바뀐 타일 포함)
 * @return 바뀐 타일이 하나라도 있으면 1
 *
 * @details 예전 바이트 루프와 **결과가 같아야 한다**: 구간은 "처음 바뀐 타일 ~ 마지막
 *          바뀐 타일", 셰도는 바뀐 타일만 갱신 (안 바뀐 타일은 원래 같은 값이므로 차이 없음).
 */
static inline uint8_t fdDiffPage(const uint8_t* cur, uint8_t* shadow, uint8_t tiles,
                                 uint8_t* first, uint8_t* count) {
    const uint8_t* c = (const uint8_t*)__builtin_assume_aligned(cur, 4);
    uint8_t*       s = (uint8_t*)__builtin_assume_aligned(shadow, 4);
    int f = -1, l = -1;
    for (int t = 0; t < tiles; t++) {
        uint32_t c0, c1, s0, s1;
        memcpy(&c0, c + t * FD_TILE_BYTES,     4);   // 정렬이 보장되어 워드 로드 1회로 컴파일된다
        memcpy(&c1, c + t * FD_TILE_BYTES + 4, 4);
        memcpy(&s0, s + t * FD_TILE_BYTES,     4);
        memcpy(&s1, s + t * FD_TILE_BYTES + 4, 4);
        if ((c0 ^ s0) | (c1 ^ s1)) {
            memcpy(s + t * FD_TILE_BYTES,     &c0, 4);
            memcpy(s + t * FD_TILE_BYTES + 4, &c1, 4);
            if (f < 0) f = t;
            l = t;
        }
    }
    if (f < 0) { *first = 0; *count = 0; return 0; }
    *first = (uint8_t)f;
    *count = (uint8_t)(l - f + 1);
    return 1;
}

#endif  // FRAME_DIFF_H
