// worynim@gmail.com
/**
 * @file ds_i2c_retry_policy.h
 * @brief HW I2C 전송 후 dirty 마스크 갱신 정책 (순수 함수 — Arduino 의존 없음)
 * @details "전송에 성공한 페이지는 비트만 지운다"는 결정은 버그를 만들기 쉬운 순수 규칙이므로
 *          분리해 호스트 단위 테스트(test/test_i2c_retry_policy.cpp)가 가능하게 한다.
 * @note 출처: Hangeul_Clock/i2c_retry_policy.h (v2.10.0)
 */
#ifndef DS_I2C_RETRY_POLICY_H
#define DS_I2C_RETRY_POLICY_H

#include <stdint.h>

namespace ds {

/**
 * @brief 한 번의 전송 시도 후 남길 dirty 마스크를 계산한다
 * @param mask      전송을 시도하려던 페이지 마스크
 * @param pageOk    페이지별 전송 성공 여부 (1 = 성공). 길이는 pageCount
 * @param pageCount mask가 표현하는 페이지 수
 * @return 성공한 페이지의 비트만 지운 마스크 (실패한 페이지는 남는다 → 재시도 대상)
 *
 * @details **배경 버그**: 이전 구현은 전송 실패를 한 번이라도 만나면 남은 페이지와
 *          다른 화면을 전부 건너뛰면서 dirty 마스크를 0으로 밀어 버렸다. 전송되지
 *          않은 변경이 셰도 버퍼와 일치한 채로 남기 때문에 다음 diff가 그 페이지를
 *          잡지 못했고, 해당 화면은 다음 글자가 바뀔 때까지 멈춰 있었다.
 *
 * @note 호출자는 실패한 페이지가 남았는지를 확인해 다음 알림을 보내야 한다.
 *       이 함수만으로는 재시도가 일어나지 않는다.
 */
static inline uint8_t dirtyMaskAfterSend(uint8_t mask, const uint8_t* pageOk, int pageCount) {
    uint8_t sent_mask = 0;
    for (int p = 0; p < pageCount; p++) {
        if (!(mask & (1 << p))) continue;   // 시도하지 않은 페이지는 그대로 둔다
        if (pageOk[p]) sent_mask |= (1 << p);
    }
    return mask & (uint8_t)~sent_mask;
}

}  // namespace ds

#endif
