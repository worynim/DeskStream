// worynim@gmail.com
/**
 * @file ds_utf8.h
 * @brief UTF-8 리딩 바이트 판정 (순수 함수 — Arduino 의존 없음)
 * @details 프로젝트마다 UTF-8 길이 판정을 따로 가지고 있었고 한글판에는 실제 버그가
 *          있었다 — `(c & 0xE0) == 0xE0`은 0xE0~0xFF에 참이라 4바이트 리딩 바이트를
 *          3바이트로 잘랐다. 판정 규칙을 한 곳에 모아 모든 프로젝트가 같은 함수를 쓰게 한다.
 * @note 출처: Hangeul_Clock/utf8_len.h (v2.10.0)
 */
#ifndef DS_UTF8_H
#define DS_UTF8_H

#include <stdint.h>

namespace ds {

/**
 * @brief UTF-8 리딩 바이트로부터 해당 문자의 바이트 길이를 구한다
 * @param p         문자열의 현재 위치 (리딩 바이트를 가리켜야 함)
 * @param remaining p부터 끝까지 남은 바이트 수
 * @return 1~4. 잘못된 선행 바이트나 남은 바이트보다 긴 요청이면 잘림 방지 값으로 보정된다
 *
 * @details 마스크가 틀리면 **앞 분기가 뒤 분기를 삼키므로** 각 조건의 마스크는
 *          정확히 그 길이 구간만 참이어야 한다.
 *          2바이트 0xC0~0xDF → (c & 0xE0) == 0xC0
 *          3바이트 0xE0~0xEF → (c & 0xF0) == 0xE0
 *          4바이트 0xF0~0xF7 → (c & 0xF8) == 0xF0
 * @note 반환값이 **0이 되지 않음**을 보장한다 — 호출부가 `i += len`으로 전진하므로
 *       0이 나오면 무한 루프가 된다.
 */
static inline uint8_t utf8CharLen(const char* p, int remaining) {
    if (remaining <= 0) return 1;
    unsigned char c = (unsigned char)*p;

    uint8_t len;
    if (c < 0x80)                          len = 1;
    else if ((c & 0xE0) == 0xC0)           len = 2;
    else if ((c & 0xF0) == 0xE0)           len = 3;
    else if ((c & 0xF8) == 0xF0)           len = 4;
    else                                   len = 1;   // 컨티뉴레이션 바이트 등 잘못된 선행 바이트

    if (len > (uint8_t)remaining) len = (uint8_t)remaining;   // 잘린 멀티바이트 방지
    return len;
}

}  // namespace ds

#endif
