#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// === [1] 하드웨어 핀 정의 (ESP32-C3 기반) ===
#define BTN1_PIN 1
#define BTN2_PIN 4
#define BTN3_PIN 10
#define BTN4_PIN 9

#define HW_SDA_PIN 5
#define HW_SCL_PIN 6
#define SW_SDA_PIN 2
#define SW_SCL_PIN 3

#define BUZZER_PIN 7

// === [2] 디스플레이 설정 ===
#define NUM_SCREENS 4
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define PAGES_PER_SCREEN 8
#define TILES_PER_PAGE 16

// === [3] I2C 성능 설정 ===
#define I2C_SPEED_HZ 800000 // 1MHz 고속 전송 (안정성을 위해 800kHz 사용)
#define I2C_TX_TIMEOUT_MS 50
#define I2C_CMD_TIMEOUT_MS 10
#define I2C_SYNC_TIMEOUT_MS 100
#define HW_I2C_BUF_SIZE 256

// === [4] RTOS 태스크 설정 ===
#define HW_TASK_STACK 4096
#define HW_TASK_PRIO 10
#define HW_TASK_CORE 0

// === [5] 기타 하드코딩 상수 ===
#define I2C_ADDR_HW_0 0x3C
#define I2C_ADDR_HW_1 0x3D

#endif
