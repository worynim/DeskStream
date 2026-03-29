# ESP32-C3 Ultra-Wide OLED (512x64) Matrix (v3.0.1)

이 프로젝트는 4개의 128x64 OLED(SSD1306)를 가로로 배치하여 하나의 **512x64 초광폭(Ultra-Wide) 디스플레이**로 제어하는 Arduino 프로젝트입니다. ESP32-C3의 하드웨어 FPU 부재를 극복하기 위해 **ESP-DSP의 정수형 FFT**를 도입하여 극한의 성능(Max 70+ FPS)을 구현했습니다.

## 🚀 주요 특징 및 성능 최적화

### 1. ESP-DSP 정수형 FFT 가속 (High-Speed Engine)
*   **Integer-only Math**: ESP32-C3의 FPU 부재로 인한 `float` 연산 병목을 제거하기 위해 16비트 정수형(`int16_t`) FFT를 사용합니다.
*   **Zero-Loss Truncation Guard**: 정수 연산 중 발생하는 데이터 손실을 막기 위해 입력 신호를 **16배(<< 4)** 증폭하여 연산 정밀도를 확보했습니다.
*   **Fast Magnitude Approximation**: 무거운 `sqrt()` 대신 `max(a, b) + 0.5 * min(a, b)` 근사 공식을 사용하여 연산 시간을 기존 대비 **90% 이상 단축**했습니다.
*   **Manual Window Generation**: 라이브러리 버전 파편화 문제를 해결하기 위해 수학적 공식을 이용한 윈도우(Hann, Hamming 등) 생성기 내장.

### 2. 고해상도 시각화 및 UI 애니메이션
*   **128 Bands Support**: 연산 성능 향상으로 128개의 세밀한 주파수 밴드를 실시간으로 처리할 수 있습니다.
*   **Sum Pooling (High-Freq Weighting)**: 고음역대 가시성을 개선하기 위해 단순히 최대값을 찾는 대신 주파수 에너지를 **합산(Sum)**하여 렌더링하는 가중치 방식을 적용했습니다.
*   **Peak Hold Animation**: 고속 프레임 환경에서 시각적 안정감을 주기 위해 피크 점이 일정 시간 체공 후 중력에 의해 떨어지는 애니메이션 로직을 탑재했습니다.

### 3. Zero-Conflict 병렬 전송 엔진 (Smart Flush 2.0)
*   **Dual-Core 분산 처리**: ESP32-C3의 단일 코어 환경에서도 FreeRTOS 태스크를 활용하여 I2C 전송과 CPU 연산을 효율적으로 분할합니다.
*   **Direct Protocol (HW OLED 1, 2)**: U8g2 라이브러리의 통신 레이어를 가로채어(Hijacking), ESP-IDF 드라이버를 통해 SSD1306 명령을 직접 쏩니다. 이로 인해 라이브러리 내부의 자원 충돌 없이 백그라운드에서 하드웨어 전송이 가능합니다.
*   **True Parallelism**: 하드웨어가 데이터를 쏘는 동안 CPU는 즉시 OLED 3, 4의 비트뱅잉을 개시합니다. 두 개의 I2C 버스가 동시에 데이터를 전송하여 전체 지연 시간을 최소화합니다.
*   **No-Mixed Data**: 코드 경로의 완전 분리를 통해 화면 간 데이터 혼선과 그래픽 깨짐 문제를 근본적으로 해결했습니다.
*   **Tile-based Dirty Checking**: **8픽셀(Tile) 단위**로 변화를 감지하여 실제 데이터가 변경된 영역만 선택적으로 전송함으로써 I2C 대역폭 소모를 최소화합니다.

### 4. 실시간 사운드 분석 기술 (ADC DMA & Optimized FFT)
*   **ADC DMA 연속 샘플링**: 하드웨어 타이머 대신 **ADC Continuous Driver(DMA)**를 사용하여 오디오 데이터를 배경에서 자동으로 수집합니다. CPU 간섭 없이 40kHz 정밀 샘플링을 보장합니다.
*   **Auto Gain Control (AGC)**: 입력 음량에 따라 감도를 자동 조절하며, 노이즈 억제 하한선(`AGC_MIN_AMP`)을 갖추고 있습니다.
*   **성능 모니터링 (Debug Mode)**: FFT/Render 시간과 I2C 전송 시간을 Us 단위로 분리 측정하여 병목 구간을 고도로 프로파일링합니다.

## 🛠 하드웨어 구성
- **MCU**: ESP32-C3 (Single Core RISC-V with FPU)
- **Display**: SSD1306 128x64 OLED x 4 (Total 512x64)
- **Microphone**: Analog Mic Module (GPIO 0 / ADC1_CH0)
- **핀 맵**:
  - HW I2C (OLED 1, 2): SDA(GPIO 5), SCL(GPIO 6)
  - SW I2C (OLED 3, 4): SDA(GPIO 2), SCL(GPIO 3)

## 📦 설치가 필요한 라이브러리
이 소스 코드를 컴파일하려면 아두이노 라이브러리 매니저에서 다음 라이브러리들을 설치해야 합니다:
*   **U8g2**: OLED 디스플레이 드라이버 및 폰트 제어
*   **LovyanGFX**: 고속 그래픽 그리기 및 가상 캔버스 라이브러리
*   **arduinoFFT (v2.0+)**: 고속 푸리에 연산 (Float 지원 버전 권장)

## 📺 포함된 기능 (데모 및 비주얼라이저)
- 4대의 OLED를 하나의 512x64 광폭 캔버스로 제어
- **실시간 사운드 비주얼라이저 (Sound Visualizer)**
  - 128개 밴드 스펙트럼 및 2픽셀 두께의 Peak Dot
  - 부드러운 막대 하강 이펙트 및 하드웨어 가속 FFT 연산
  - 하이브리드 지연(vTaskDelay + delayMicroseconds) 기반 정밀 FPS 제어
  - 성능 데이터 실시간 출력 모드 (DEBUG_MODE)

## 📈 성능 결과
- **병렬 전송 효율**: 직렬 전송 대비 약 40% 이상의 성능 향상
- **FFT+렌더링 시간**: 약 **14~15ms** (float 최적화 적용)
- **구동 속도**: 약 **35~40 FPS** 수준의 부드러운 애니메이션 (v1.6 기준)

---

## 📅 로드맵 및 향후 계획

### [v1.9.1] - ~~RMT 전송 가속~~ → 폐기 (2026-03-20)
> ⚠️ **RMT I2C 에뮬레이션은 구조적으로 불가능**함을 실험으로 확인.
> SCL/SDA 채널 간 동기화 불가, ACK 양방향 통신 불가, 메모리 제약 등의 이유로 폐기.
> 상세 분석은 `RESEARCH.md` 참조.

### [v2.0] - 향후 최적화 방향 (검토 중)
- [ ] 현행 SW 비트뱅잉 + HW I2C 병렬 태스크 구조 내 미세 최적화.
- [ ] SPI OLED 또는 ESP32-S3 듀얼 코어 마이그레이션 타당성 검토.
- [ ] 더티 체킹 알고리즘 추가 최적화 (전송량 최소화).

> **상태**: 현재 v3.0.1 (캘리브레이션 데이터 출력 및 70 FPS 최적화 완료)
