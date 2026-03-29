# Release Notes - Sound Visualizer

## [v3.0.1] - Calibration Data Serial Output (2026-03-30)
### 🚀 주요 개선 사항
- **캘리브레이션 데이터 시리얼 출력 기능 추가**:
    - 캘리브레이션 완료 후 측정된 주파수 밴드별 노이즈 플로어(Noise Floor) 값을 시리얼 모니터로 확인할 수 있도록 개선했습니다.
    - 밴드 번호를 포함하여 줄바꿈된 형식으로 출력되어 가독성을 높였습니다.
- **파일명 정규화**: `Sound_Visualizer_4 _OLEDs.ino` 파일명에서 불필요한 공백을 제거하여 `Sound_Visualizer_4_OLEDs.ino`로 수정했습니다.

---

## [v3.0.0] - ESP-DSP Integer FFT & UI Enhancement (2026-03-23)
### 🚀 주요 개선 및 성능 최적화 (ESP-DSP 통합)
- **고속 정수형 FFT 엔진 교체**:
    - ESP32-C3의 하드웨어 FPU 부재로 인한 성능 저하를 해결하기 위해 `arduinoFFT`를 제거하고, 내장된 **ESP-DSP (16-bit Integer)**를 도입했습니다.
    - FFT 연산 속도가 기존 대비 약 **90% 이상 단축**되어 1ms 내외의 초고속 처리가 가능해졌습니다.
- **입력 신호 증폭 및 Truncation 방지**:
    - 정수 연산 시 발생하는 소수점 버림(Truncation) 손실을 막기 위해 입력 신호를 **16배(<< 4)** 증폭 후 윈도잉을 적용하는 정밀 로직을 탑재했습니다.
- **고속 진폭(Magnitude) 근사 연산**:
    - 연산 부하가 큰 `sqrt()` 함수 대신 `max + 0.5 * min` 방식의 근사 연산을 적용하여 전체 루프 타임을 극단적으로 줄였습니다.
- **UI 및 시각화 고도화**:
    - **Peak Hold (체공형 피크)**: 고프레임 환경에서 피크 점이 일정 시간 머물다 떨어지는 중력 애니메이션을 추가하여 시각적 안정감을 확보했습니다.
    - **Sum Pooling (고음역 가중치)**: 고음역대 가시성을 개선하기 위해 여러 주파수 Bin의 에너지를 합산(Sum)하여 렌더링하는 방식을 적용했습니다.
    - **128 밴드 확장**: 연산 성능 여유를 바탕으로 주파수 해상도를 128개 밴드로 확장했습니다.
- **설정 최적화 (`config.h`)**:
    - `TARGET_FPS`를 **70 FPS**로 상향하여 더욱 부드러운 움직임을 구현했습니다.

---

## [v2.0.1] - Code Review & Performance Refinement (2026-03-22)
### 🚀 주요 개선 및 성능 최적화
- **하이브리드 FPS 제어 엔진 도입**:
    - `delayMicroseconds`의 CPU 블로킹 문제를 해결하기 위해, 2ms 이상의 대기는 `vTaskDelay`로 처리하고 나머지만 정밀 지연시키는 하이브리드 로직을 적용했습니다.
- **FFT 윈도우 연산 최적화**:
    - 매 프레임 실행되던 FFT 윈도우 타입 판별 `switch`문을 제거하고, `begin()` 단계에서 캐싱하여 루프 오버헤드를 최소화했습니다.
- **메모리 안전성 및 안정성 강화**:
    - `AudioAnalyzer` 클래스에 소멸자(`~AudioAnalyzer`)를 추가하여 동적 할당된 힙 메모리의 안전한 해제를 보장합니다.
    - `DEFAULT_NUM_BANDS`를 128개로 상향 조정하여 더욱 조밀한 사운드 시각화를 제공합니다.

## [v2.0.0] - True Parallelism & Mapping Correction (2026-03-21)
### 🚀 주요 개선 및 성능 최적화
- **진정한 병렬 전송(True Parallelism) 엔진 구현**: 
    - `is_transmitting_hw` 상태 플래그를 도입하여 하드웨어 I2C 전송을 '시작지점 대기(Wait-at-Start)' 방식으로 전면 개편했습니다.
    - 이제 전송이 진행되는 동안 CPU는 멈추지 않고 즉시 다음 프레임의 FFT 연산을 수행할 수 있어, 전체 루프 타임이 획기적으로 줄어들었습니다.
- **주파수 매핑 로직 정밀화 (Bug Fix)**:
    - `MAPPING_MODE 0`(선형)일 때 시작 주파수(`LOG_MIN_FREQ`)가 무시되던 로직 결함을 수정했습니다.
    - 이제 모든 매핑 모드에서 저역대 노이즈 대역을 정확하게 필터링할 수 있습니다.

## [v1.9.5] - FFT Window Enum Fix & Parameter Fine-Tuning (2026-03-20)
### 🛠️ 수정 및 최적화
- **FFTWindow 열거형 명칭 수정**: `arduinoFFT` 라이브러리의 최신 사양에 맞춰 `BlackmanNuttall` → `Blackman_Nuttall`, `BlackmanHarris` → `Blackman_Harris`, `FlatTop` → `Flat_top` 등으로 명칭을 보정했습니다.
- **`config.h` 파라미터 미세 조정**:
    - `FFT_WINDOW_TYPE`: `Precompiled(10)` 모드 적용.
    - `LOG_MIN_FREQ`: 130Hz로 조정하여 저역대 노이즈 필터링 최적화.
    - `PEAK_DOT_HEIGHT`: 2 -> 5 (피크 점 가시성 향상).
    - `CAL_ITERATIONS`: 50 -> 120 (노이즈 캘리브레이션 정밀도 향상).
    - `BTN_LONG_PRESS_MS`: 2000 -> 1000 (사용자 편의를 위한 버튼 반응 속도 개선).
    - `CAL_DONE_DELAY_MS`: 1500 -> 1000 (메시지 대기 시간 단축).

## [v1.9.4] - Configurable FFT Window Functions (2026-03-20)
### 🚀 주요 변경 사항
- **FFT 윈도우 함수 설정 기능 추가**:
    - `config.h`에서 `FFT_WINDOW_TYPE` 설정을 통해 11가지 윈도우 함수를 선택할 수 있도록 구현했습니다.
    - 지원 윈도우: Rectangle, Hamming, Hann, Triangle, Nuttall, Blackman, Blackman_Nuttall, Blackman_Harris, Flat_top, Welch, Precompiled.
    - 사용자 취향과 오디오 특성에 맞춰 주파수 누설(Spectral Leakage) 특성을 조정할 수 있습니다.

## [v1.9.3] - Configuration Tuning for Stability & Response (2026-03-20)
### 🚀 주요 변경 사항
- **FFT 및 샘플링 파라미터 최적화**:
    - `DEFAULT_SAMPLES`: 512 -> 128 (연산량 감소 및 실시간성 강화)
    - `DEFAULT_SAMPLING_FREQ`: 40kHz -> 10kHz (저역대 주파수 해상도 정밀화)
    - `TARGET_FPS`: 30 -> 40 (더 부드러운 애니메이션 구현)
- **노이즈 필터링 고정 임계값 적용**:
    - `LOG_MIN_FREQ`를 `136.71875f` 고정값으로 수정하여 설정 변경 시에도 일관된 저역대 노이즈 차단 성능 확보.

---


## [v1.9.2] - Refactoring & Profiling Enhancements (2026-03-20)
### 🚀 주요 개선 사항
- **`DisplayEngine` 코드 가독성 향상 (DRY 원칙 적용)**:
    - `pushCanvas()` 함수 내부에 중복되어 있던 하드웨어/소프트웨어 I2C용 더티 체킹(Dirty Checking) 로직을 C++ 람다(Lambda) 함수로 통합했습니다.
    - 기존의 병렬 실행 순서와 안정성은 100% 유지하면서 코드 길이를 절반으로 줄여 유지보수성을 크게 향상시켰습니다.
- **정밀 성능 프로파일링 기능 추가**:
    - `DEBUG_MODE == 2`일 때 FFT 분석 시간(`FFT`), 화면 전송 시간(`T`)과 더불어 **렌더링 소요 시간(`R`)**을 마이크로초(us) 단위로 개별 측정하여 출력합니다.
    - `AudioAnalyzer` 클래스 내부에 측정 로직을 캡슐화하여 객체지향적 설계를 강화하고, 성능 병목 지점을 더 세밀하게 분석할 수 있도록 개선했습니다.

---

## [v1.9.1] - RMT I2C 에뮬레이션 실험 및 폐기 (2026-03-20)
### ⚠️ 실험 결과: 구조적 불가능 확인
- **RMT 기반 SW I2C 가속 시도**:
    - ESP32-C3의 RMT(Remote Control) 주변장치를 활용하여 Software I2C를 하드웨어 가속하려는 시도를 실제 구현 및 테스트.
    - 오실로스코프에서 SCL/SDA 파형(714kHz) 출력은 확인되었으나, **OLED(SSD1306)가 데이터를 전혀 인식하지 못함**.
- **폐기 사유 (3가지 근본 원인)**:
    1. **SCL/SDA 동기화 불가**: 두 독립 RMT 채널의 시작 시점 오차(수 μs)로 I2C 프로토콜 위반.
    2. **ACK 처리 불가**: RMT는 일방향 출력 전용 → 양방향 I2C ACK 통신 불가능.
    3. **메모리 제약**: 채널당 48 symbols 하드웨어 메모리 → 인터럽트 오버헤드로 FPS 급락(30→7).
- **부작용**: Stack Overflow, Load Access Fault 등 무한 리셋 빈발.
- **결론**: 코드를 직전 커밋으로 복구하고, RMT I2C 접근법을 **영구 폐기** 처리함.
- **향후 방향**: 현행 SW 비트뱅잉 + HW I2C 병렬 태스크 안정화, SPI OLED/듀얼 코어 MCU 대안 검토.

## [v1.9.0] - Cognitive Power-Law Mapping
### 🚀 주요 추가 및 개선 사항
- **다중 주파수 매핑 엔진 구현**: 
    - `MAPPING_MODE` 설정을 통해 선형(Linear), 로그(Log), 거듭제곱(Power-Law) 방식 선택 가능.
    - `MAPPING_POWER` 계수 조절을 통한 주파수 대역의 시각적 곡률(Curvature) 미세 조정 기능 하드웨어 최적화.
- **고해상도 시각화 환경 구축**:
    - FFT 샘플 수 상향 (256 -> 512) 및 오디오 샘플링 주파수 상향 (10kHz -> 20kHz).
    - 시각화 밴드 수 2배 증설 (64 -> 128), 디스플레이 해상도(512px) 완전 대응.
- **저역대 정교화 및 보간(Interpolation)**:
    - **부동 소수점 빈 참조(Linear Interpolation)** 기술 적용으로 물리적 하드웨어 한계를 넘는 부드러운 움직임 구현.
    - 저역대 노이즈 구간 자동 배제 수식 적용 (156.25Hz 미만 4개 Bin 스킵).
- **매핑 동기형 캘리브레이션**:
    - `getBandAmplitude()` 공통 함수 추출을 통해 매핑 모드와 캘리브레이션 노이즈 컷 레벨의 완벽한 일치 달성.

### 🛠️ 수정 및 최적화
- **메모리 구조 개선**: `AudioAnalyzer` 클래스 내 밴드 매핑 테이블(Lookup Table) 동적 할당 및 관리 효율화.
- **캘리브레이션 파라미터 튜닝**: `CAL_ITERATIONS` 최적화 (120 -> 50)를 통한 부팅 속도 개선.

## [v1.8.0] - Magic Number Modularization & Code Clean-up (최신)
### 🚀 주요 변경 사항
- **중앙 집중식 상수 관리 시스템 도입 (`config.h`)**:
    - 코드 전반에 산재해 있던 '매직 넘버'들을 `config.h`파일로 통합하여 유지보수성 극대화.
    - **디스플레이 설정**: OLED 해상도, I2C 통신 속도(1MHz), 타임아웃, 핀 번호(HW/SW SDA, SCL) 모듈화.
    - **오디오/FFT 설정**: 샘플 수(256), 샘플링 주파수(10kHz), 밴드 수(64), 프레임 속도(30FPS) 관리.
    - **애니메이션 튜닝**: 막대 및 피크 하강 속도, 피크 도트 두께(`PEAK_DOT_HEIGHT`) 등 시각 요소 정의.
    - **동작 정책**: 캘리브레이션 횟수, 버튼 롱 프레스 시간(2초), 각종 지연 시간(부팅, 완료 메시지 등) 통합.

- **코드 가독성 및 무결성 향상**:
    - `AudioAnalyzer` 클래스: 하드코딩된 수치 대신 `config.h`의 `DEFAULT_...` 상수를 참조하도록 생성자 및 렌더링 로직 수정.
    - `OLED_Wide_FFT.ino`: 전역 상수를 제거하고 `CANVAS_WIDTH`, `TEXT_Y_OFFSET` 등을 사용하여 코드 흐름 가독성 개선.
    - `DisplayEngine`: I2C 버스 설정 및 태스크 우선순위 등을 상수를 통해 유연하게 조정 가능하도록 변경.

- **기능 최적화**:
    - `PEAK_DOT_HEIGHT`를 설정을 통해 사용자가 쉽게 변경할 수 있도록 구조 기반 마련.

## [v1.7.1] - Sensitivity & Speed Fine-tuning (최신)
### 🚀 주요 변경 사항
- **오디오 분석 민감도 극대화**:
    - `AGC_MIN_AMP`를 1000.0f -> 32.0f로 대폭 하향 조정.
    - 정밀 캘리브레이션 덕분에 노이즈 걱정 없이 극도로 작은 소리에도 역동적인 반응 가능.
- **I2C 전송 속도 최적화**:
    - 하드웨어 I2C 클럭을 **1MHz**로 상향하여 데이터 전송 지연 최소화 및 FPS 안정성 확보.
- **데이터 샘플링 조정**:
    - `SAMPLING_FREQ`를 20kHz -> 10kHz로 조정하여 저음역대 해상도 및 FFT 분석 정밀도 개선.
- **캘리브레이션 알고리즘 튜닝**:
    - 노이즈 플로어 결정 시의 여유 마진(Margin)을 15% -> 10%로 미세 조정하여 감도와 안정성 사이의 최적점 확보.
- **주파수 매핑 심화 연구 완료**: 인간의 옥타브 인지 구조를 반영한 '초정밀 인지적 로그 스케일링(Mel-Scale 기반)' 연구 및 보간법 전략 수립.

## [v1.7.0] - Dynamic Noise Calibration
### 🚀 주요 추가 사항
- **정밀 노이즈 캘리브레이션 기능**:
    - 버튼(9번 핀) 롱 프레스(2초)를 통해 현재 환경의 소음을 측정하여 자동 보정하는 기능 추가.
    - **피크 탐지(Peak Detection)** 방식: 단순 평균이 아닌 측정 기간 중 최댓값을 추적하여 순간적인 노이즈까지 완벽 차단.
    - **안전 마진 적용**: 산출된 피크값에 15% 가중치와 절대 여유치를 더해 신뢰성 있는 임계값 설정.
- **사용자 경험(UX) 개선**:
    - 버튼을 뗀 후 **1초 뒤에 측정을 시작**하도록 지연 로직을 추가하여 버튼 조작 시 발생하는 기계적 노이즈 배제.
    - 화면 메시지(CALIBRATING, RELEASE, DONE)를 통한 실시간 상태 안내.
- **대역별 독립 임계값**: 모든 FFT 대역(64개)에 대해 개별적인 노이즈 플로어를 적용하여 정밀 시각화 구현.

## [v1.6.1] - Stability & Signal Quality
### 🚀 주요 개선 사항
- **FPS 상한 제어 (30 FPS Limit)**: 
    - 화면 변화가 적을 때 막대가 너무 빠르게 떨어지는 문제를 해결하기 위해 프레임 속도를 최대 30 FPS로 고정. 
    - 프레임 대기 시간을 동적으로 계산하여 애니메이션 일관성 확보.
- **I2C 신호 안정성 강화 (Internal Pull-up)**:
    - 고속(800kHz~1MHz) 통신 시 신호 왜곡(톱니파 현상)을 완화하기 위해 HW/SW 모든 I2C 핀의 내부 풀업(Internal Pull-up) 활성화.
- **디버깅 가독성 개선**: 시리얼 모니터의 루프 시간 출력 형식 최적화.

## [v1.6.0] - Zero-Conflict Parallel Engine (최종 안정화 버전)
### 🚀 주요 변경 사항 (The Final Fix)
- **Zero-Conflict 병렬 아키텍처 도입**: 
    - 하드웨어 전송 경로에서 U8g2 라이브러리를 완전히 제거하고 **SSD1306 직접 전송 프로토콜(Direct Protocol)**을 구현.
    - 라이브러리 내부의 정적 변수 공유로 인한 **화면 데이터 섞임(OLED 3 데이터가 2에 표시되는 문제)을 근본적으로 해결**.
- **진정한 병렬 처리(True Parallelism)**:
    - 하드웨어 I2C(OLED 1, 2)는 ESP-IDF 드라이버가 백그라운드 태스크에서 전용으로 처리.
    - 소프트웨어 I2C(OLED 3, 4)는 메인 태스크에서 CPU 비트뱅잉으로 처리.
    - 두 작업의 코드 경로를 완전히 분리하여 동시 실행 시에도 간섭 없는 100% 병렬 전송 달성.
- **호환성 최적화**:
    - U8g2 라이브러리 버전에 따라 `user_ptr` 멤버가 없는 경우를 대비해 **개별 독립 콜백(Independent Callbacks)** 구조로 재설계하여 컴파일 호환성 확보.
- **UI 안정성 향상**:
    - 초기 부팅 시 "Sound Visualizer" 텍스트가 깨지거나 섞여 나오는 현상 해결.

## [v1.5.0] - Parallel I2C Power Engine (최신)
### 🚀 주요 변경 사항 (Architecture Redesign)
- **OLED 병렬 전송 엔진 도입**: 
    - 하드웨어 I2C(OLED 1, 2)와 소프트웨어 I2C(OLED 3, 4)를 **병렬(Parallel)**로 전송하도록 아키텍처 전면 재설계.
    - ESP-IDF v5 `i2c_master` 드라이버 기반의 비차단(Non-blocking) 전송 콜백 구현.
    - FreeRTOS 전용 태스크(Core 0)를 배정하여 하드웨어 전송을 분산, 시스템 응답성 극대화.
- **성능 비약적 향상**: 
    - 전체 화면 전송 시간을 직렬 방식 대비 **~40% 단축** (순차 40ms -> 병렬 24ms).
    - 전체 통합 시스템 FPS를 **~35~40 FPS** 수준으로 안정화 (기존 v1.4 대비 약 40% 향상).
- **코드 무결성 확보**: 
    - 분할 렌더링 루프를 제거하고 **전체 화면 동시 업데이트** 루프로 회귀함과 동시에 고성능 유지.

## [v1.4.0] - ADC DMA & Float Optimization
### Changed
- **Float-point 최적화**: ESP32-C3의 FPU 하드웨어를 활용하기 위해 모든 FFT 및 오디오 연산을 `double`에서 `float`로 변경. FFT 연산 시간이 약 5ms 단축됨.
- **I2C 성능 튜닝**: SW I2C 콜백에서 불필요한 딜레이를 완전히 제거하여 전송 속도 극대화.

## [v1.3.2] - Pin Configuration Refactoring
### Changed
- **핀 설정 중앙 집중화**: 하드웨어 및 소프트웨어 I2C 핀 정의를 `DisplayEngine.cpp`에서 메인 `.ino` 파일로 이동하여 하드웨어 변경 시 관리가 용이하도록 개선.
- **Extern Linkage 적용**: 전역 상수에 `extern` 키워드를 적용하여 파일 간 설정값을 안전하게 공유.

## [v1.3.1] - Multi-level Debug Mode & UI Tuning
### Added
- **다단계 디버그 모드**: `DEBUG_MODE`를 0(Off), 1(FPS), 2(Full) 단계로 세분화하여 상황에 맞는 성능 모니터링 가능.

### Changed
- **애니메이션 튜닝**: 막대 하강 속도 및 Peak Dot 하강 속도를 미세 조정하여 시각적 부드러움 최적화.
- **I2C 설정 최적화**: 하드웨어 I2C 클럭을 1MHz로 조정하여 전송 효율과 안정성 균형 확보.

## [v1.3.0] - Modularization & Tile-based Optimization
### Added
- **모듈화 아키텍처**: `DisplayEngine` (OLED 제어) 및 `AudioAnalyzer` (FFT/렌더링) 클래스로 코드 분리하여 가독성 및 재사용성 향상.
- **Tile-based Dirty Checking**: 8픽셀 단위 미세 변화 감지 로직을 도입하여 I2C 전송 효율 극대화.
- **I2C 오버클럭**: 하드웨어 I2C 클럭을 1.2MHz로 상향 조정하여 전송 지연 시간 단축.

### Changed
- **성능 향상**: 기존 13 FPS에서 **약 20 FPS**로 실구동 성능 개선.
- **버퍼 관리**: 512x64 캔버스 상수의 헤더 정의 및 좌표계 오프셋 버그 수정.
- **디버그 모드**: 기본 설정값을 `0 (false)`으로 변경하여 상용 모드 진입.

## [v1.2.0] - AGC & Performance Debugging
- Auto Gain Control 알고리즘 및 초기 디버깅 도구 구축.

## [v1.1.0] - Sound Visualizer Update
### Added
- 실시간 사운드 비주얼라이저 기능 추가 및 `arduinoFFT` 연산 도입.
- 하드웨어 타이머 기반 비동기 ADC 샘플링 구현.
- 128개 밴드 스펙트럼 뷰 및 초기 Peak Dot 로직 반영.
