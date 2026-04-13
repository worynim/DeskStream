// worynim@gmail.com
/**
 * @file config.h
 * @brief 스트리밍 성능 및 네트워크 설정 구성을 위한 상방 정의 헤더
 * @details WiFi 설정, I2C 속도, 디스플레이 해상도 및 스트리밍 프로토콜 관련 상수 관리
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==========================================
// [1] 디스플레이 및 I2C 통신 설정
// ==========================================
#define SCREEN_WIDTH        128         // OLED 개별 화면 가로 해상도
#define SCREEN_HEIGHT       64          // OLED 개별 화면 세로 해상도
#define NUM_SCREENS         4           // 총 연결된 OLED 화면 개수
#define PAGES_PER_SCREEN    (SCREEN_HEIGHT / 8)  // 화면당 페이지 수 (SSD1306은 8행 단위)
#define TILES_PER_PAGE      (SCREEN_WIDTH / 8)   // 페이지당 타일 수 (Dirty Check용)

#define CANVAS_WIDTH        (SCREEN_WIDTH * NUM_SCREENS)   // 가상 캔버스 가로 (512px)
#define CANVAS_HEIGHT       SCREEN_HEIGHT                   // 가상 캔버스 세로 (64px)
#define CANVAS_BYTES        (CANVAS_WIDTH * CANVAS_HEIGHT / 8) // 1-bit 버퍼 크기 (4096 bytes)

#define I2C_SPEED_HZ        1000000     // I2C 클럭 속도 (1MHz 초고속)
#define I2C_TX_TIMEOUT_MS   50          // 데이터 전송 최대 대기 시간 (ms)
#define I2C_CMD_TIMEOUT_MS  10          // 명령 전송 최대 대기 시간 (ms)
#define I2C_SYNC_TIMEOUT_MS 100         // HW/SW 전송 동기화 타임아웃

// I2C 핀 설정 (ESP32-C3 기준 — DeskStream 하드웨어)
#define HW_SDA_PIN          5           // 하드웨어 I2C SDA 핀 (화면 1, 2)
#define HW_SCL_PIN          6           // 하드웨어 I2C SCL 핀 (화면 1, 2)
#define SW_SDA_PIN          2           // 소프트웨어(Bit-bang) SDA 핀 (화면 3, 4)
#define SW_SCL_PIN          3           // 소프트웨어(Bit-bang) SCL 핀 (화면 3, 4)

// 그래픽 컬러 상수
#define COLOR_WHITE         1           // 픽셀 켜짐 (흰색)
#define COLOR_BLACK         0           // 픽셀 꺼짐 (검정)

// 백그라운드 태스크(RTOS) 설정
#define HW_TASK_PRIO        10          // HW 전송 태스크 우선순위
#define HW_TASK_STACK       4096        // 태스크 스택 크기 (바이트)
#define HW_TASK_CORE        0           // 실행 CPU 코어 (ESP32-C3: 싱글코어, 0번 명시)
#define I2C_GLITCH_FILTER   7           // I2C 신호 잡음 제거 필터 (0~15)
#define I2C_INTR_PRIO       3           // I2C 인터럽트 우선순위

// 내부 버퍼 사양
#define SCREEN_BUF_SIZE     1024        // 화면 1개당 픽셀 버퍼 크기 (128x64/8)
#define HW_I2C_BUF_SIZE     1030        // HW I2C 전송 패킷 버퍼 크기

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
#define BTN1_PIN            1           // BTN1: Short=미사용, Long=화면 플립
#define BTN2_PIN            4           // BTN2: Short=밝기DOWN, Long=도움말 토글
#define BTN3_PIN            10          // BTN3: Short=밝기UP, Long=미사용
#define BTN4_PIN            9           // BTN4: Short=미사용, Long=미사용 (확장 예비)
#define BUZZER_PIN          7           // 부저 피드백 핀

#define BTN_LONG_PRESS_MS   1000        // 길게 누름 판단 시간 (ms)
#define BTN_DEBOUNCE_MS     50          // 디바운스 시간 (ms)

// ==========================================
// [4] 시스템 설정
// ==========================================
#define BOOT_MESSAGE_DELAY_MS  2000     // 부팅 메시지 표시 시간 (ms)
#define TEXT_Y_OFFSET          20       // 안내 텍스트 Y축 위치

#endif // CONFIG_H
