# 🖥️ DeskStream: ESP32-C3 4-OLED Multifunctional Board

DeskStream은 ESP32-C3와 4개의 OLED 디스플레이를 활용한 다기능 데스크톱 정보 스테이션 프로젝트입니다.

## 🛠️ 하드웨어 사양 (Hardware Specification)
- **MCU**: ESP32-C3 (SuperMini 등)
- **Display**: 0.96" SSD1306 OLED (128x64) x 4개 (가로 배치 시 512x64 광폭 디스플레이 구현 가능)
- **Buttons**: 4개의 사용자 입력 버튼 (GPIO 1, 4, 10, 9)
- **Interface**:
  - **HW I2C**: SDA(GPIO 5), SCL(GPIO 6) - OLED 1, 2 제어
  - **SW I2C**: SDA(GPIO 2), SCL(GPIO 3) - OLED 3, 4 제어
- 상세 정보: [hardware.md](./hardware.md) 참조

## 📁 주요 프로젝트 (Sub-Projects)
본 저장소는 DeskStream 보드용으로 개발된 4가지 주요 프로그램을 포함하고 있습니다.

### 1. [Smart_Info_Station: 스마트 정보 스테이션](./Smart_Info_Station/)
- **기능**: 시계, 날씨, 미세먼지, 유튜브 구독자 수, 증시 지수, 가상화폐 가격, 환율 실시간 모니터링.
- **특징**: 웹 대시보드를 통한 실시간 설정, 비동기 데이터 갱신(FreeRTOS), 네이티브 HAL 기반 고속 SW I2C.

### 2. [GPS_DASH: 하드웨어 GPS 대시보드](./GPS_DASH/)
- **기능**: 실시간 속도, 나침반, 고도, 위도/경도, 위성 상태, 주행 통계 표시.
- **특징**: NEO-6M GPS 모듈 연동(5Hz), UBX 프로토콜 직접 제어, 8가지 디스플레이 모드 제공.

### 3. [Sound_Visualizer_4_OLEDs: 고속 오디오 비주얼라이저](./Sound_Visualizer_4_OLEDs/)
- **기능**: 아날로그 마이크 입력을 통한 128밴드 실시간 주파수 시각화.
- **특징**: ESP-DSP 정수형 FFT 가속(Max 70+ FPS), 512x64 초광폭 캔버스 제어, ADC DMA 샘플링.

### 4. [OLED_Wide_test: 초광폭 디스플레이 테스트](./OLED_Wide_test/)
- **기능**: 4개의 OLED를 하나의 512x64 화면으로 제어하는 데모 및 테스트 코드.
- **특징**: Shadow Buffering을 통한 데이터 전송 최적화, LovyanGFX 기반 고속 그래픽 렌더링.

### 5. [Stats_Monitor: 시스템 상태 모니터](./Stats_Monitor/)
- **기능**: PC/Mac의 CPU, GPU, RAM, SSD 상태 및 온도, 네트워크 트래픽 실시간 표시.
- **특징**: 외부 클라이언트 연동, 하드웨어 통계 가젯 모드, 시스템 생체 정보 가시화.

## 🚀 주요 기술적 특징
- **하이브리드 I2C 아키텍처**: 하드웨어 I2C와 레지스터 직접 제어 기반 고속 소프트웨어 I2C를 병렬로 사용하여 4개의 화면을 끊김 없이 제어합니다.
- **성능 최적화**: 30~70 FPS의 부드러운 애니메이션 구현을 위한 Dirty Page 체크 및 그래픽 가속 기법이 적용되었습니다.
- **비동기 처리**: 네트워크 요청이나 복잡한 연산 중에도 화면 갱신이 멈추지 않도록 FreeRTOS를 적극 활용합니다.

---
© 2026 DeskStream Project. 만들어진 보드에 생동감을 불어넣는 코드를 작성합니다.
