// worynim@gmail.com
/**
 * @file test_bitbang_core.cpp
 * @brief BitBang I2C 순수 로직 회귀 테스트 (호스트 g++)
 * @details 빌드: g++ -std=c++11 -I.. test_bitbang_core.cpp -o /tmp/test_bitbang_core
 *          실행: /tmp/test_bitbang_core
 *
 * @note [BB v1.0.9] 이 파일은 **지금 살아 있는** 순수 로직만 검증한다.
 *       비트 순서·ACK 극성·4버스 열 구간 정렬 — 셋 다 값만 계산하는 문제라
 *       하드웨어 없이 여기서 고정한다.
 *       (v1.0.8까지 있던 bbPageCommand/bbBuildPagePayloads 회귀 테스트는
 *        그 함수들이 캡처·재생 방식으로 대체되며 함께 삭제했다. 명령 바이트는
 *        이제 U8g2가 만들고, `flushDirtyPages`의 손 페이로드 경로가 사라졌다.)
 */
#include <cstdio>
#include <cstring>
#include "bitbang_i2c_core.h"

static int g_pass = 0;
static int g_fail = 0;

static void expectU32(const char* label, uint32_t got, uint32_t want) {
    if (got == want) { g_pass++; }
    else { g_fail++; printf("  FAIL  [%s] got 0x%08X, want 0x%08X\n", label, got, want); }
}

static void expectU8(const char* label, uint8_t got, uint8_t want) {
    if (got == want) { g_pass++; }
    else { g_fail++; printf("  FAIL  [%s] got 0x%02X, want 0x%02X\n", label, got, want); }
}

/**
 * @brief 비트 마스크 8개(ones/zeros)를 **복조**해 원래 바이트를 되살린다
 * @details 실기에서 나올 전이 시퀀스를 그대로 역산하는 검증이다. 인코더와
 *          디코더가 같은 규칙을 쓰면 순서를 뒤집어도 통과하므로, 디코더는
 *          "MSB부터 8비트"라는 I2C 규칙만 알고 있게 독립적으로 적었다.
 */
static uint8_t demodulate(const uint32_t* ones, const uint32_t* zeros, uint8_t pin) {
    const uint32_t bit = (uint32_t)1u << pin;
    uint8_t b = 0;
    for (int i = 0; i < 8; i++) {
        b <<= 1;
        const bool isOne  = (ones[i]  & bit) != 0;
        const bool isZero = (zeros[i] & bit) != 0;
        if (isOne == isZero) return 0xFF;   // 둘 다이거나 둘 다 아님 = 잘못된 인코딩
        if (isOne) b |= 1;
    }
    return b;
}

int main() {
    const uint8_t sda[4] = {5, 6, 7, 8};   // config.h의 BB_SDA_PIN_1..4와 같은 배치

    // --- ① MSB 먼저: 0x80은 첫 비트만 HIGH ---
    {
        const uint8_t bytes[4] = {0x80, 0, 0, 0};
        uint32_t ones[8], zeros[8];
        bbByteBitMasks(bytes, sda, 4, 0x1, ones, zeros);
        expectU32("0x80 비트0 = HIGH(SDA5)", ones[0],  1u << 5);
        expectU32("0x80 비트0 = LOW 아님",   zeros[0], 0u);
        expectU32("0x80 비트1 = LOW",        zeros[1], 1u << 5);
        expectU32("0x80 비트7 = LOW",        zeros[7], 1u << 5);
    }

    // --- ①' 0x01은 마지막 비트만 HIGH (LSB로 뒤집힘 방지) ---
    {
        const uint8_t bytes[4] = {0x01, 0, 0, 0};
        uint32_t ones[8], zeros[8];
        bbByteBitMasks(bytes, sda, 4, 0x1, ones, zeros);
        expectU32("0x01 비트7 = HIGH", ones[7],  1u << 5);
        expectU32("0x01 비트0 = LOW",  zeros[0], 1u << 5);
    }

    // --- ①'' 4개 버스가 서로 다른 바이트를 동시에 실어도 자기 핀에만 걸린다 ---
    {
        const uint8_t bytes[4] = {0xA5, 0x5A, 0xFF, 0x00};
        uint32_t ones[8], zeros[8];
        bbByteBitMasks(bytes, sda, 4, 0xF, ones, zeros);
        for (int b = 0; b < 4; b++) {
            char label[64];
            snprintf(label, sizeof(label), "버스%d 왕복 복조", b);
            expectU8(label, demodulate(ones, zeros, sda[b]), bytes[b]);
        }
    }

    // --- ①''' 비트 마스크는 서로 겹치지 않는다 (합집합 = 참여 핀 전체) ---
    {
        const uint8_t bytes[4] = {0xAA, 0xAA, 0xAA, 0xAA};
        uint32_t ones[8], zeros[8];
        bbByteBitMasks(bytes, sda, 4, 0xF, ones, zeros);
        for (int i = 0; i < 8; i++) {
            char label[64];
            snprintf(label, sizeof(label), "비트%d ones/zeros 배타", i);
            expectU32(label, ones[i] & zeros[i], 0u);
            expectU32(label, ones[i] | zeros[i], 0xF << 5);   // 4핀 전부 (5,6,7,8)
        }
    }

    // --- ①'''' 비참여 버스는 마스크에 등장하지 않는다 (START/STOP 오발 방지) ---
    {
        const uint8_t bytes[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        uint32_t ones[8], zeros[8];
        bbByteBitMasks(bytes, sda, 4, 0x5, ones, zeros);   // 버스 0, 2만 참여
        for (int i = 0; i < 8; i++) {
            char label[64];
            snprintf(label, sizeof(label), "비참여 버스1 제외 (비트%d)", i);
            expectU32(label, ones[i] & (1u << 6), 0u);
            snprintf(label, sizeof(label), "비참여 버스3 제외 (비트%d)", i);
            expectU32(label, ones[i] & (1u << 8), 0u);
        }
    }

    // --- ② ACK 극성: SDA를 LOW로 당긴 버스만 ACK ---
    {
        // 버스0(SDA5)=LOW, 버스1(SDA6)=HIGH, 버스2(SDA7)=LOW, 버스3(SDA8)=HIGH
        const uint32_t inReg = (1u << 6) | (1u << 8);
        expectU8("ACK = LOW인 버스만 (0,2)", bbAckMask(inReg, sda, 4, 0xF), 0x5);
    }
    {
        const uint32_t inReg = 0;   // 전부 LOW = 전부 ACK
        expectU8("전부 LOW → 전부 ACK", bbAckMask(inReg, sda, 4, 0xF), 0xF);
    }
    {
        const uint32_t inReg = (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8);
        expectU8("전부 HIGH → 전부 NACK", bbAckMask(inReg, sda, 4, 0xF), 0x0);
    }
    {
        // 비참여 버스가 LOW여도 ACK로 세면 안 된다
        const uint32_t inReg = 0;
        expectU8("비참여 버스는 ACK 집계 제외", bbAckMask(inReg, sda, 4, 0x1), 0x1);
    }

    // --- ③ 열 구간 합집합: 정렬 가능한 공통 구간을 만들어야 한다 ---
    {
        const uint8_t first[4] = {2, 4, 0, 3};
        const uint8_t count[4] = {2, 2, 0, 1};   // 버스2는 이 페이지에 변경 없음
        uint8_t f = 0, c = 0;
        bbUnionColumnRange(first, count, 4, &f, &c);
        // 최소 시작 = 0(버스2 제외하면 2), 최대 끝 = 5(버스1) → [2,5] = 4타일
        expectU8("합집합 시작", f, 2);
        expectU8("합집합 타일 수", c, 4);
    }
    {
        const uint8_t first[4] = {9, 9, 9, 9};
        const uint8_t count[4] = {0, 0, 0, 0};
        uint8_t f = 1, c = 1;
        bbUnionColumnRange(first, count, 4, &f, &c);
        expectU8("참여 버스 없음 → 타일 0", c, 0);
    }
    {
        // 한 화면만 바뀌면 그 화면 구간 그대로 (불필요하게 넓히지 않는다)
        const uint8_t first[4] = {7, 0, 0, 0};
        const uint8_t count[4] = {2, 0, 0, 0};
        uint8_t f = 0, c = 0;
        bbUnionColumnRange(first, count, 4, &f, &c);
        expectU8("단일 버스 시작", f, 7);
        expectU8("단일 버스 타일 수", c, 2);
    }

    printf("test_bitbang_core: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
