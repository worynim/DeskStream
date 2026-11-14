// worynim@gmail.com
/**
 * @file config.h
 * @brief World_Clock 프로젝트 상수
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define WORLD_CLOCK_VERSION "0.1.0"

/** WiFi 설정 포털의 AP 이름 */
const char* const kPortalApName = "World_Clock_Setup";

/** 깜빡이는 요소(':'·설정 테두리·지도 점)가 켜짐/꺼짐을 바꾸는 간격 */
const uint32_t kBlinkIntervalMs = 500;

/** 설정 화면에서 누르고 있으면 시간대 건너뛰기를 시작하기까지 / 건너뛰는 간격 */
const uint32_t kRepeatDelayMs = 500;
const uint32_t kRepeatIntervalMs = 150;

/** 설정 화면에서 입력이 없으면 저장하지 않고 시계로 돌아가는 시간 */
const uint32_t kSetupTimeoutMs = 20000;

/** 알림 딱지 / 정보 카드를 띄워 두는 시간 */
const uint32_t kToastMs = 1000;
const uint32_t kInfoCardMs = 3000;

/** 밝기 단계 (SSD1306 contrast 값) */
const uint8_t kBrightnessLevels[] = {10, 50, 128, 255};
const uint8_t kBrightnessCount = sizeof(kBrightnessLevels) / sizeof(kBrightnessLevels[0]);

#endif
