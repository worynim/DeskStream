// worynim@gmail.com
/**
 * @file config.h
 * @brief Page_Turner 전역 상수 (핀은 DeskStreamCore ds_board.h)
 */
#pragma once
#include <stdint.h>

#define PT_VERSION "1.0.0"
#define PT_BLE_NAME "DS Page Turner"
#define PT_BLE_NAME_SHORT "Page Turner"   // 화면 폭(64px)에 맞춘 표시용

constexpr uint32_t kPairWaitMs = 60000;   // 슬롯 페어링 대기: 이 시간 안에 새 기기가 없으면 이전 슬롯으로
