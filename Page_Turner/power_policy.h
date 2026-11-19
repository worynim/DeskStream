// worynim@gmail.com
/**
 * @file power_policy.h
 * @brief 마지막 버튼 입력 후 경과 시간 → 절전 단계 (하드웨어 무관 — 호스트 테스트 대상)
 */
#pragma once
#include <stdint.h>

enum class PowerStage : uint8_t { Active, ScreenOff, Sleep };

constexpr uint32_t kScreenOffMs = 60000;   // 1분: 화면 끄기
constexpr uint32_t kSleepMs = 300000;      // 5분: 딥슬립

// 부호 없는 뺄셈이라 millis() 랩(약 49.7일)을 걸쳐도 맞다
inline PowerStage powerStage(uint32_t nowMs, uint32_t lastInputMs) {
    uint32_t idle = nowMs - lastInputMs;
    if (idle >= kSleepMs) return PowerStage::Sleep;
    if (idle >= kScreenOffMs) return PowerStage::ScreenOff;
    return PowerStage::Active;
}
