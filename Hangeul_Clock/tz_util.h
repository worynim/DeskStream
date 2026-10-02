// worynim@gmail.com
/**
 * @file tz_util.h
 * @brief POSIX TZ 문자열 검증 유틸리티 (Arduino 의존성 없음)
 * @details setenv("TZ", ...)로 직접 전달되는 값이므로 화이트리스트 검증을 강제한다.
 *          네이티브 단위 테스트가 가능하도록 config_manager와 분리했다.
 * @note [SYNC] ENG_Clock/tz_util.h 와 동일 (바이트 단위)
 */
#ifndef TZ_UTIL_H
#define TZ_UTIL_H

#include <stddef.h>

/** SystemSettings.timezone 배열 크기와 일치해야 한다 (config.h의 TIMEZONE_MAX_LEN). */
#ifndef TIMEZONE_MAX_LEN
#define TIMEZONE_MAX_LEN 48
#endif

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
 * @return 길이 3~(TIMEZONE_MAX_LEN-1), 약어 3자 이상, 허용 문자만 있으면 true
 * @note 이 검사를 통과해도 문법이 올바른 TZ는 아니다. 적용 후 localtime() 확인이 필요하다.
 */
bool isValidTimezone(const char* tz);

#endif
