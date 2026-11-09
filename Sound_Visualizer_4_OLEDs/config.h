// worynim@gmail.com
/**
 * @file config.h
 * @brief 사운드 비주얼라이저 전역 설정 및 하드웨어 구성
 * @details I2S 핀 배열, FFT 해상도, 디스플레이 속도 및 시각화 스타일 매개변수 관리
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <DeskStreamCore.h>   // 4-OLED 화면 엔진·버튼·부저 (libraries/DeskStreamCore)

// ==========================================
// [1] 디스플레이 및 I2C 통신 설정
// ==========================================
#define SCREEN_WIDTH        128         // OLED 개별 화면 가로 해상도
#define SCREEN_HEIGHT       64          // OLED 개별 화면 세로 해상도
#define NUM_SCREENS         4           // 총 연결된 OLED 화면 개수

#define CANVAS_WIDTH        (SCREEN_WIDTH * NUM_SCREENS) // 전체 가상 캔버스 가로 크기 (512px)
#define CANVAS_HEIGHT       SCREEN_HEIGHT                // 전체 가상 캔버스 세로 크기 (64px)


// I2C 핀 설정 (ESP32-C3 기준)

// 그래픽 테마 (비트맵 컬러 방식)
// 핀·I2C 속도(HW 1.1MHz·고속 SW I2C)는 DeskStreamCore ds_board.h·ds::DisplayConfig가 정한다.
// 버튼: BTN1(GPIO 1)=화면 플립, BTN2(GPIO 4)=도움말, BTN4(GPIO 9)=모드/캘리브레이션 — ds::kButtonPins[0/1/3]
#define BTN_IDX_FLIP        0           // BTN1 짧게: 화면 180° 플립
#define BTN_IDX_HELP        1           // BTN2 길게: 버튼 도움말 표시/숨김
#define BTN_IDX_CAL         3           // BTN4 짧게: 시각화 모드 / 길게: 노이즈 캘리브레이션
#define COLOR_WHITE         1           // 픽셀 켜짐 (흰색)
#define COLOR_BLACK         0           // 픽셀 꺼짐 (검정색)

// 백그라운드 태스크(RTOS) 설정

// 내부 버퍼 사양

// ==========================================
// [2] 오디오 분석 및 시각화 설정
// ==========================================
#define TARGET_FPS          70          // 목표 프레임 레이트 (초당 70프레임)
#define TARGET_FRAME_US     (1000000 / TARGET_FPS) // 1프레임당 목표 시간 (마이크로초)

#define DEFAULT_SAMPLES       512       // FFT 샘플 수 (결과가 클수록 해상도가 높아지나 연산량 증가)
#define DEFAULT_SAMPLING_FREQ 20000.0f  // 마이크로폰 샘플링 주파수 (40kHz = 최대 20kHz 대역 감지)
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

// 박자 판정 (도트 매트릭스 플래시용, BASS_BANDS는 DEFAULT_NUM_BANDS·매핑 설정에 따라 저역 범위가 달라짐)
#define BASS_BANDS          12          // 저역(베이스)으로 취급할 하위 밴드 개수
#define BEAT_RATIO          1.5f        // 저역 세기가 평균의 몇 배를 넘으면 박자로 판정
#define BEAT_MIN_LEVEL      16          // 박자 판정 최소 저역 세기 (0~CANVAS_HEIGHT, 잡음 방지)
#define BEAT_COOLDOWN       10          // 박자 판정 뒤 다음 박자까지 최소 프레임 수

// 도트 매트릭스 모드 (DOT_PITCH 간격마다 DOT_SIZE 크기의 점, 열 수 = CANVAS_WIDTH/DOT_PITCH → 밴드 수와 같게 둘 것)
#define DOT_PITCH           4           // 점 간격 (px)
#define DOT_SIZE            3           // 켜진 점 한 변 (px, DOT_PITCH보다 작게)
#define DOT_FLASH_FRAMES    2           // 박자 때 화면 반전 유지 프레임 수 (0이면 플래시 끔)

// 파티클 모드 (스펙트럼 막대가 높은 밴드마다 그 주파수 위치(x)에서 확률적으로 위에서 떨어져 바닥에서 튕김)
#define PARTICLE_MAX        256          // 동시에 존재하는 파티클 최대 개수
#define PARTICLE_FIRST_BAND 0           // 파티클을 만드는 시작 밴드 (0 = 전 대역: 왼쪽 OLED1 저음 → 오른쪽 OLED4 고음)
#define PARTICLE_MIN_LEVEL  12          // 생성 최소 막대 세기 (0~CANVAS_HEIGHT, 스펙트럼 막대 높이와 같은 기준 → 잡음 밴드 제외)
#define PARTICLE_RATE_PCT   80          // 막대가 최대(CANVAS_HEIGHT)일 때 프레임당 생성 확률 (%, 세기에 비례해 증가)
#define PARTICLE_SPAWN_PER_FRAME 6      // 프레임당 최대 생성 개수
#define PARTICLE_GRAVITY    6           // 중력 (1/16 px/프레임², 6 ≈ 0.375px)
#define PARTICLE_SPAWN_VX   6           // 생성 시 가로 속도 범위 ±(1/16 px/프레임, 6 ≈ ±0.4px)
#define PARTICLE_BOUNCE_VX  24          // 바닥에서 튕길 때마다 가로 속도에 더해지는 무작위 범위 ±(1/16 px/프레임, 24 = ±1.5px)
#define PARTICLE_MAX_VX     48          // 가로 속도 상한 (1/16 px/프레임, 48 = 3px)
#define PARTICLE_BOUNCE_PCT 60          // 바닥 반발 계수 (%)
#define PARTICLE_MIN_BOUNCE 24          // 튕긴 뒤 속도(1/16 px/프레임)가 이보다 작으면 소멸

#define AGC_DECAY_RATE      0.95f       // 자동 이득 제어(AGC) 감쇠율 (낮을수록 빠르게 반응)
#define AGC_MIN_AMPLITUDE   512.0f      // AGC 기준값의 하한 = 최대 게인 상한 (클수록 조용할 때 잡음이 덜 증폭됨, 작은 소리는 낮게 보임)
#define AGC_INITIAL_MAX_AMP 2000.0f     // 초기 AGC 기준값 (동적으로 변경됨)

#define NOISE_FLOOR_FACTOR  1.25f       // 측정된 노이즈 최댓값에 곱할 마진 비율 (1.25 = +25%)
#define NOISE_FLOOR_OFFSET  50.0f       // 노이즈 필터링 시 추가로 적용할 상수 오프셋
#define DEFAULT_NOISE_FLOOR 150.0f      // 캘리브레이션 전 기본 노이즈 레벨

#define CAL_ITERATIONS      200         // 캘리브레이션 시 소음을 측정할 횟수 (1회 약 25ms → 약 5초, 밴드별 최댓값 기준)
#define BTN_LONG_PRESS_MS   1000        // 버튼을 길게 누르는 판단 시간 (1초)

#define ADC_DMA_BUF_COUNT   4           // ADC DMA 내부 링버퍼 프레임 깊이
#define BOOT_MESSAGE_DELAY_MS 2000      // 부팅 초기 메시지 노출 시간
#define CAL_START_DELAY_MS    1000      // 캘리브레이션 시작 전 대기 (버튼 소음 방지)
#define CAL_DONE_DELAY_MS     1000      // 캘리브레이션 완료 메시지 노출 시간
#define TEXT_Y_OFFSET         20        // 안내 텍스트가 표시될 Y축 높이

#endif // CONFIG_H
