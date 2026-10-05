// worynim@gmail.com
/**
 * @file test_i2c_retry_policy.cpp
 * @brief I2C dirty 마스크 갱신 정책 회귀 테스트
 * @details 빌드: g++ -std=c++11 -I.. test_i2c_retry_policy.cpp -o /tmp/test_i2c_retry_policy
 *          실행: /tmp/test_i2c_retry_policy
 *
 * @note 이 테스트는 리뷰 §2.1에서 확인된 정지 버그의 회귀 테스트다.
 *       재현되는 버그: 한 페이지의 I2C 전송이 실패하면 남은 페이지와 다른 화면을
 *       전부 건너뛰고 dirty 마스크까지 0으로 밀어, 전송되지 않은 갱신이 셰도 버퍼와
 *       일치한 채로 남아 해당 화면이 다음 글자 변경 때까지 멈춰 있었다.
 *       → 실패한 페이지는 비트를 남겨 재시도되어야 한다.
 */
#include <cstdio>
#include "i2c_retry_policy.h"

static int g_pass = 0;
static int g_fail = 0;

static void expectMask(const char* label, uint8_t got, uint8_t want) {
    if (got == want) {
        g_pass++;
    } else {
        g_fail++;
        printf("  FAIL  [%s] got 0x%02X, want 0x%02X\n", label, got, want);
    }
}

int main() {
    const int PAGES = 8;   // 실제 PAGES_PER_SCREEN과 같아야 한다 (SSD1306 128x64)

    // pageOk[]는 **페이지 인덱스**로 접근된다. 항상 PAGES칸을 채운다.

    // --- 회귀: 한 페이지만 실패하면 그 비트만 남아야 한다 ---
    {
        const uint8_t mask = 0b1011;              // 페이지 0,1,3 예정
        const uint8_t ok[] = {1, 1, 1, 0, 1, 1, 1, 1};   // 페이지 3만 전송 실패
        expectMask("page3 실패 → page3만 재시도 대상",
                   dirtyMaskAfterSend(mask, ok, PAGES), 0b1000);
    }

    // --- 전부 성공이면 마스크가 0 ---
    {
        const uint8_t mask = 0xFF;
        const uint8_t ok[] = {1, 1, 1, 1, 1, 1, 1, 1};
        expectMask("전부 성공 → 마스크 0", dirtyMaskAfterSend(mask, ok, PAGES), 0x00);
    }

    // --- 전부 실패하면 마스크가 그대로 ---
    //   재시도 루프가 붙지만 _error_count가 50을 넘으면 recoverBus()가 다리로 들어온다.
    {
        const uint8_t mask = 0xFF;
        const uint8_t ok[] = {0, 0, 0, 0, 0, 0, 0, 0};
        expectMask("전부 실패 → 마스크 유지", dirtyMaskAfterSend(mask, ok, PAGES), 0xFF);
    }

    // --- 전송하지 않은 페이지는 건드리지 않는다 ---
    //   mask에 없는 페이지의 ok 값이 0이어도 결과가 바뀌면 안 된다.
    {
        const uint8_t mask = 0b0001;                     // 페이지 0만 예정
        const uint8_t ok[] = {1, 0, 0, 0, 0, 0, 0, 0};   // 나머지는 실패로 채워도 무관
        expectMask("미전송 페이지는 결과에 영향 없음",
                   dirtyMaskAfterSend(mask, ok, PAGES), 0x00);
    }

    // --- 실패 페이지는 다음 회차에 다시 시도된다 ---
    //   1회차: 페이지 2 실패 → 마스크에 2만 남음. 2회차 성공 → 0.
    {
        const uint8_t mask = 0b0100;
        const uint8_t fail[]   = {1, 1, 0, 1, 1, 1, 1, 1};
        const uint8_t succeed[] = {1, 1, 1, 1, 1, 1, 1, 1};
        const uint8_t afterFail = dirtyMaskAfterSend(mask, fail, PAGES);
        expectMask("1회차 실패 후 재시도 대상 유지", afterFail, 0b0100);
        expectMask("2회차 성공 후 마스크 0", dirtyMaskAfterSend(afterFail, succeed, PAGES), 0x00);
    }

    // --- 빈 마스크는 그대로 (아무것도 예약되지 않은 회차) ---
    {
        const uint8_t ok[] = {0, 0, 0, 0, 0, 0, 0, 0};
        expectMask("빈 마스크 → 변화 없음", dirtyMaskAfterSend(0x00, ok, PAGES), 0x00);
    }

    printf("test_i2c_retry_policy: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}