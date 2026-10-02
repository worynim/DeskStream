// worynim@gmail.com
/**
 * @file english_time_core.h
 * @brief 시간 → 영어 텍스트 변환 순수 로직 (Arduino 의존성 없음)
 * @details 네이티브 단위 테스트를 위해 Arduino/String을 사용하지 않는다.
 *          모든 함수는 호출자가 제공한 버퍼에 결과를 쓰고, 실패 시 빈 문자열을 남긴다.
 */
#ifndef ENGLISH_TIME_CORE_H
#define ENGLISH_TIME_CORE_H

#include <stddef.h>

/** 결과 버퍼 권장 크기. 최장 표현 "TWENTY THREE"(12자) + NUL 여유 포함. */
#define ENG_BUF_SIZE 20

namespace engtime {

/** 분/초 유효 범위 (0~59) */
bool isValidMinute(int n);

/** 12시간제 시 유효 범위 (1~12) */
bool isValidHour12(int n);

/** 24시간제 시 유효 범위 (0~23) */
bool isValidHour24(int n);

/**
 * @brief 0~59를 영어 단어로 변환
 * @param n    0~59 (범위 밖이면 false)
 * @param buf  결과 버퍼 (ENG_BUF_SIZE 이상 권장)
 * @return 성공 시 true, 실패 시 false (buf는 빈 문자열)
 */
bool numberToWords(int n, char* buf, size_t cap);

/**
 * @brief 시를 영어 단어로 변환
 * @param hour  0~23
 * @param is24h true면 24시간제(0→"ZERO"), false면 12시간제(0→"TWELVE")
 */
bool hourToWords(int hour, bool is24h, char* buf, size_t cap);

/** @brief 분을 변환. 0이면 "O'CLOCK" */
bool minuteToWords(int minute, char* buf, size_t cap);

/** @brief 초를 변환. 0이면 "O'CLOCK" */
bool secondToWords(int second, char* buf, size_t cap);

/** @brief 24시간제 시각을 "AM"/"PM"으로 변환 (hour 0~23) */
bool amPm(int hour24, char* buf, size_t cap);

/** @brief 0~59를 2자리 문자열로 변환 ("09") */
bool twoDigit(int n, char* buf, size_t cap);

/**
 * @brief 요일 번호를 영어 요일명으로 변환
 * @param day 0=일요일 ~ 6=토요일 (tm_wday 규약)
 * @note 최장 요일명은 `WEDNESDAY`(9자) — 1줄 9자 제한과 정확히 일치
 */
bool dayName(int day, char* buf, size_t cap);

/**
 * @brief 날짜를 "M/D" 형식으로 변환
 * @param mon         월 1~12 (tm_mon + 1 규약)
 * @param mday        일 1~31 (tm_mday 규약)
 * @param monthFirst  true = "월/일", false = "일/월"
 * @param buf         결과 버퍼
 * @return 성공 시 true, 실패 시 false (buf는 빈 문자열)
 * @details 선행 0은 붙이지 않는다 ("2/10", "12/5"). 최장 "31/12" = 5자.
 *
 * @note 24시간제 첫 화면이 "날짜 ⏎ 요일" 2줄이 되므로, 이 문자열은
 *       layoutWrap()의 공백 규칙("2/10 FRIDAY" → 2줄)에 합쳐 쓰인다.
 */
bool dateString(int mon, int mday, bool monthFirst, char* buf, size_t cap);

/** @brief 문자열 결합 (공백 한 칸). 어느 한쪽이 비어 있으면 빈 문자열. */
bool joinWords(const char* a, const char* b, char* buf, size_t cap);

} // namespace engtime

#endif
