// worynim@gmail.com
/**
 * @file ds_wifi_portal_util.h
 * @brief WiFi 설정 포털의 입력 검증·HTML 이스케이프 (순수 함수 — Arduino 의존 없음)
 * @details 포털 페이지에는 **주변 공유기의 SSID**(누구나 정할 수 있는 문자열)가 그대로 찍힌다.
 *          이스케이프 없이 넣으면 SSID 이름만으로 페이지에 스크립트를 심을 수 있으므로,
 *          출력 지점에서 반드시 htmlEscapeTo()를 거친다. 호스트 테스트 대상(test/test_wifi_portal.cpp).
 */
#ifndef DS_WIFI_PORTAL_UTIL_H
#define DS_WIFI_PORTAL_UTIL_H

#include <stddef.h>

namespace ds {

/** SSID 길이 상한 (802.11 규격 32바이트) */
constexpr size_t kWifiSsidMaxLen = 32;
/** WPA 비밀번호 길이 (8~63자 ASCII). 0자는 열린 네트워크 */
constexpr size_t kWifiPassMinLen = 8;
constexpr size_t kWifiPassMaxLen = 63;

/** SSID가 1~32바이트이고 제어문자가 없는가 */
bool wifiSsidValid(const char* ssid);

/** 비밀번호가 빈 문자열(열린 네트워크)이거나 8~63자이고 제어문자가 없는가 */
bool wifiPassValid(const char* pass);

/**
 * @brief HTML 본문·속성값에 넣을 수 있도록 이스케이프한다 (& < > " ')
 * @param out NUL 포함 cap 바이트까지 쓴다
 * @return 전부 들어갔으면 true. 모자라면 false (out은 잘린 채 NUL로 끝난다 — 쓰지 않는다)
 */
bool htmlEscapeTo(const char* in, char* out, size_t cap);

}  // namespace ds

#endif
