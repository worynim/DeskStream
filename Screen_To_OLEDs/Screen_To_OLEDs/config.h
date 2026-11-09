// worynim@gmail.com
/**
 * @file config.h
 * @brief 스트리밍 성능 및 네트워크 설정 구성을 위한 상방 정의 헤더
 * @details WiFi 설정, I2C 속도, 디스플레이 해상도 및 스트리밍 프로토콜 관련 상수 관리
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <DeskStreamCore.h>   // 4-OLED 화면 엔진·버튼·부저·WiFi (libraries/DeskStreamCore)

// ==========================================
// [1] 디스플레이 및 I2C 통신 설정
// ==========================================
#define SCREEN_WIDTH        128         // OLED 개별 화면 가로 해상도
#define SCREEN_HEIGHT       64          // OLED 개별 화면 세로 해상도
#define NUM_SCREENS         4           // 총 연결된 OLED 화면 개수

#define CANVAS_WIDTH        (SCREEN_WIDTH * NUM_SCREENS)   // 가상 캔버스 가로 (512px)
#define CANVAS_HEIGHT       SCREEN_HEIGHT                   // 가상 캔버스 세로 (64px)
#define CANVAS_BYTES        (CANVAS_WIDTH * CANVAS_HEIGHT / 8) // 1-bit 버퍼 크기 (4096 bytes)


// I2C 핀 설정 (ESP32-C3 기준 — DeskStream 하드웨어)

// 그래픽 컬러 상수
#define COLOR_WHITE         1           // 픽셀 켜짐 (흰색)
#define COLOR_BLACK         0           // 픽셀 꺼짐 (검정)

// 백그라운드 태스크(RTOS) 설정

// 내부 버퍼 사양

// ==========================================
// [2] UDP 네트워크 설정
// ==========================================
#define UDP_PORT            12345       // 화면 데이터 수신 UDP 포트
#define DISCOVERY_PORT      12346       // ESP32 IP 자동 검색 UDP 포트
#define DISCOVERY_MSG       "ESP32_OLED_OFFER"  // Discovery 패킷 식별 문자열
#define DISCOVERY_INTERVAL  3000        // Discovery 브로드캐스트 간격 (ms)

// UDP 청크 프로토콜 사양 (Python 전송측과 반드시 일치)
#define CHUNK_PAYLOAD_SIZE  1024        // 청크당 페이로드 크기 (byte)
#define HEADER_SIZE         3           // 헤더: [Frame ID][Total Chunks][Chunk Index]
#define MAX_PACKET_SIZE     (CHUNK_PAYLOAD_SIZE + HEADER_SIZE)
#define TOTAL_CHUNKS        (CANVAS_BYTES / CHUNK_PAYLOAD_SIZE)  // = 4

// 프레임 수신 타임아웃
#define FRAME_RECV_TIMEOUT_MS  3000     // 연결 끊김 판단 시간 (ms)

// WiFi 체크 주기
#define WIFI_CHECK_INTERVAL    2000     // WiFi 상태 체크 주기 (ms)

// ==========================================
// [3] 버튼 및 부저 설정
// ==========================================

// 핀·I2C 속도(HW 1.1MHz·고속 SW I2C)는 DeskStreamCore ds_board.h·ds::DisplayConfig가 정한다.
#define BTN_IDX_FLIP        0           // BTN1 (GPIO 1): Long=화면 플립
#define BTN_IDX_BRIGHT      1           // BTN2 (GPIO 4): Short=밝기 순환, Long=도움말 토글
#define BTN_LONG_PRESS_MS   1000        // 길게 누름 판단 시간 (ms)

// ==========================================
// [4] 시스템 설정
// ==========================================
#define BOOT_MESSAGE_DELAY_MS  2000     // 부팅 메시지 표시 시간 (ms)
#define TEXT_Y_OFFSET          20       // 안내 텍스트 Y축 위치

#endif // CONFIG_H
