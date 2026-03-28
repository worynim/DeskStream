#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==========================================
// [1] 디스플레이 및 I2C 통신 설정
// ==========================================
#define SCREEN_WIDTH        128         // OLED 개별 화면 가로 해상도
#define SCREEN_HEIGHT       64          // OLED 개별 화면 세로 해상도
#define NUM_SCREENS         4           // 총 연결된 OLED 화면 개수
#define PAGES_PER_SCREEN    (SCREEN_HEIGHT / 8) // 화면당 메모리 페이지 수 (SSD1306은 8행 단위 관리)
#define TILES_PER_PAGE      (SCREEN_WIDTH / 8)  // 페이지당 가로 타일 수 (8픽셀 단위 Dirty Check용)

#define CANVAS_WIDTH        (SCREEN_WIDTH * NUM_SCREENS) // 전체 가상 캔버스 가로 크기 (512px)
#define CANVAS_HEIGHT       SCREEN_HEIGHT                // 전체 가상 캔버스 세로 크기 (64px)

#define I2C_SPEED_HZ        1000000     // I2C 통신 클럭 속도 (1MHz 초고속 모드)
#define I2C_TX_TIMEOUT_MS   50          // 데이터 전송 시 최대 대기 시간 (ms)
#define I2C_CMD_TIMEOUT_MS  10          // 명령 전송 시 최대 대기 시간 (ms)
#define I2C_SYNC_TIMEOUT_MS 100         // 하드웨어/소프트웨어 전송 동기화 타임아웃

// I2C 핀 설정 (ESP32-C3 기준)
#define HW_SDA_PIN          5           // 하드웨어 I2C 전용 SDA 핀 (화면 1, 2)
#define HW_SCL_PIN          6           // 하드웨어 I2C 전용 SCL 핀 (화면 1, 2)
#define SW_SDA_PIN          2           // 소프트웨어(Bit-bang) SDA 핀 (화면 3, 4)
#define SW_SCL_PIN          3           // 소프트웨어(Bit-bang) SCL 핀 (화면 3, 4)

// 그래픽 테마 (비트맵 컬러 방식)
#define COLOR_WHITE         1           // 픽셀 켜짐 (흰색)
#define COLOR_BLACK         0           // 픽셀 꺼짐 (검정색)

// 백그라운드 태스크(RTOS) 설정
#define HW_TASK_PRIO        10          // 하드웨어 전송 태스크 우선순위 (높을수록 우선됨)
#define HW_TASK_STACK       4096        // 태스크에 할당된 스택 메모리 크기 (바이트)
#define HW_TASK_CORE        0           // 태스크를 실행할 CPU 코어 (ESP32-C3는 싱글코어지만 명시적 0번 사용)
#define I2C_GLITCH_FILTER   7           // I2C 신호 잡음 제거 필터 강도 (0~15)
#define I2C_INTR_PRIO       3           // I2C 인터럽트 우선순위

// 내부 버퍼 사양
#define SCREEN_BUF_SIZE     1024        // 화면 1개당 픽셀 버퍼 크기 (128x64/8)
#define HW_I2C_BUF_SIZE     1030        // 하드웨어 I2C 전송 패킷 버퍼 크기 (명령어 오버헤드 포함)

// ==========================================
// [2] 오디오 분석 및 시각화 설정
// ==========================================
#define TARGET_FPS          70          // 목표 프레임 레이트 (초당 40프레임)
#define TARGET_FRAME_US     (1000000 / TARGET_FPS) // 1프레임당 목표 시간 (마이크로초)

#define DEFAULT_SAMPLES       1024       // FFT 샘플 수 (결과가 클수록 해상도가 높아지나 연산량 증가)
#define DEFAULT_SAMPLING_FREQ 40000.0f  // 마이크로폰 샘플링 주파수 (10kHz = 최대 5kHz 대역 감지)
#define DEFAULT_NUM_BANDS     128        // 화면에 표시할 주파수 막대(밴드) 총 개수

// 주파수 매핑 설정
#define MAPPING_MODE          0         // 0: 선형(Linear), 1: 로그(Octave), 2: 거듭제곱(Power/Box-Cox)
#define MAPPING_POWER         2.0f      // 거듭제곱 계수 (1:선형, 2.5:음악적 최적, 3 이상:저역대 극강조)
#define FFT_WINDOW_TYPE       2         // 0:Rectangle, 1:Hamming, 2:Hann, 3:Blackman, 4:Blackman_Harris, 5:Flat_top
#define LOG_MIN_FREQ          130       // 매핑 시작 주파수 
#define LOG_MAX_FREQ          10000.0f  // 매핑 종료 주파수 (Hz)

#define BAR_DROP_SPEED      3           // 막대 애니메이션 하강 속도 (픽셀 단위)
#define PEAK_DROP_SPEED     1           // 피크 점 하강 속도 (픽셀 단위)
#define PEAK_HOLD_TIME      10          // 피크 대기 지속 프레임 수
#define PEAK_DOT_HEIGHT     3           // 피크 점의 두께 (픽셀 단위)

#define AGC_DECAY_RATE      0.95f       // 자동 이득 제어(AGC) 감쇠율 (낮을수록 빠르게 반응)
#define AGC_MIN_AMPLITUDE   64.0f       // AGC가 동작하기 위한 최소 신호 크기
#define AGC_INITIAL_MAX_AMP 2000.0f     // 초기 AGC 기준값 (동적으로 변경됨)

#define NOISE_FLOOR_FACTOR  1.1f        // 측정된 노이즈 레벨에 곱할 마진 비율 (1.1 = +10%)
#define NOISE_FLOOR_OFFSET  50.0f       // 노이즈 필터링 시 추가로 적용할 상수 오프셋
#define DEFAULT_NOISE_FLOOR 150.0f      // 캘리브레이션 전 기본 노이즈 레벨

#define CAL_ITERATIONS      120         // 캘리브레이션 시 소음을 측정할 횟수
#define CAL_BUTTON_PIN      9           // 캘리브레이션 실행용 버튼 핀 번호 (GPIO 9)
#define BTN_LONG_PRESS_MS   1000        // 버튼을 길게 누르는 판단 시간 (2초)

#define ADC_DMA_BUF_COUNT   4           // ADC DMA 내부 링버퍼 프레임 깊이
#define BOOT_MESSAGE_DELAY_MS 2000      // 부팅 초기 메시지 노출 시간
#define CAL_START_DELAY_MS    1000      // 캘리브레이션 시작 전 대기 (버튼 소음 방지)
#define CAL_DONE_DELAY_MS     1000      // 캘리브레이션 완료 메시지 노출 시간
#define TEXT_Y_OFFSET         20        // 안내 텍스트가 표시될 Y축 높이

#endif // CONFIG_H
