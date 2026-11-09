// worynim@gmail.com
/**
 * @file ds_json_lite.h
 * @brief 평평한(1단계) JSON 객체에서 키 값을 꺼내는 경량 파서 (순수 함수 — Arduino 의존 없음)
 * @details 웹 설정 POST 본문 `{"anim_mode":3,"is_flipped":true,"timezone":"KST-9"}` 같은
 *          단순 객체만 다룬다. ArduinoJson을 쓰지 않는 이유는 앱 이미지 여유가 작은
 *          프로젝트(시계 3종 98~99%)가 있어서다. 호스트 단위 테스트 대상(test/test_json_lite.cpp).
 *
 *          키 찾기 규칙: `"key"` 뒤에 공백과 `:`가 와야 키로 인정한다. 문자열 값 안의
 *          `\"key\"`(이스케이프된 따옴표)는 키로 보지 않는다.
 * @note 출처: Hangeul_Clock/web_manager.cpp 의 parseVal·parseBool·문자열 오프셋 파싱을 대체.
 *       예전 문자열 파싱은 키 리터럴 길이를 잘못 더해 첫 글자가 유실되는 버그("KST-9"→"ST-9")가
 *       있었다 — 여기서는 키 위치를 한 함수(findValue)만 계산한다.
 */
#ifndef DS_JSON_LITE_H
#define DS_JSON_LITE_H

#include <stddef.h>
#include <stdint.h>

namespace ds {

/** @brief 키의 값 시작 위치(공백 건너뜀)를 돌려준다. 없으면 nullptr */
const char* jsonFindValue(const char* body, const char* key);

/** @brief 정수 값 (`-`와 숫자만, 따옴표 문자열은 거부). 성공하면 true */
bool jsonGetInt(const char* body, const char* key, long* out);

/** @brief `true` / `false` 리터럴. 성공하면 true */
bool jsonGetBool(const char* body, const char* key, bool* out);

/**
 * @brief 문자열 값을 out에 복사한다 (NUL 포함 cap 바이트까지)
 * @details 이스케이프 `\"` `\\` `\/`만 풀고, 그 밖의 이스케이프(`\n`, `\uXXXX` 등)·닫는 따옴표
 *          없음·cap 초과는 **거부**한다 (잘린 값을 설정으로 쓰지 않기 위해서다).
 *          UTF-8 바이트는 그대로 복사한다.
 */
bool jsonGetString(const char* body, const char* key, char* out, size_t cap);

}  // namespace ds

#endif
