// worynim@gmail.com
/**
 * @file ds_tz_util.h
 * @brief POSIX TZ 문자열 검증 유틸리티 (Arduino 의존 없음)
 * @details setenv("TZ", ...)로 직접 전달되는 값이므로 화이트리스트 검증을 강제한다.
 * @note 출처: Hangeul_Clock/tz_util.h (v2.10.0)
 */
#ifndef DS_TZ_UTIL_H
#define DS_TZ_UTIL_H

#include <stddef.h>

namespace ds {

/** TZ 문자열을 담는 버퍼 크기 (NUL 포함). 설정 구조체의 timezone 배열 크기와 일치해야 한다. */
constexpr size_t kTimezoneMaxLen = 48;

/**
 * @brief POSIX TZ 문자열 검증
 * @details 허용 문자: A-Z a-z 0-9 + - : , . /
 *          첫 필드(표준 시간대 약어)는 POSIX 규칙대로 알파벳 3자 이상이어야 한다.
 *          이 검사로 과거 펌웨어 버그로 첫 글자가 유실된 NVS 값("ST-9" 등)이
 *          부팅 시 거부되고 기본값으로 자가 치유된다.
 *          '/'는 DST 경계 시각 구분자로 필요하다 ("M3.5.0/1" = 3월 마지막 일요일 1시).
 *          단, tzdata가 tzfile 경로로 해석할 수 있으므로 경로 이동("..")은 거부한다.
 *          예) "KST-9", "UTC0", "IST-5:30", "EST5EDT,M3.2.0,M11.1.0"
 * @param tz 검증할 문자열 (NULL 허용 — false 반환)
 * @return 길이 3~(kTimezoneMaxLen-1), 약어 3자 이상, 허용 문자만 있으면 true
 * @note 이 검사를 통과해도 문법이 올바른 TZ는 아니다. 적용 후 localtime() 확인이 필요하다.
 */
bool isValidTimezone(const char* tz);

}  // namespace ds

#endif
