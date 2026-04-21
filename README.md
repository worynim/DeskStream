# 🖥️ DeskStream: ESP32-C3 4-OLED Multifunctional Board

DeskStream은 ESP32-C3와 4개의 OLED 디스플레이를 활용한 다기능 데스크톱 정보 스테이션 프로젝트입니다.

-https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## 🛠️ 하드웨어 사양 (Hardware Specification)
- **MCU**: ESP32-C3 (SuperMini)
- **Display**: 0.96" SSD1306 OLED (128x64) x 4개 (가로 배치 시 512x64 광폭 디스플레이 구현 가능)
- **Buttons**: 4개의 사용자 입력 버튼 (GPIO 1, 4, 10, 9)
- **Interface**:
  - **HW I2C**: SDA(GPIO 5), SCL(GPIO 6) - OLED 1, 2 제어
  - **SW I2C**: SDA(GPIO 2), SCL(GPIO 3) - OLED 3, 4 제어
- 상세 정보: [hardware.md](./hardware.md) 참조

## 📁 주요 프로젝트 (Sub-Projects)
본 저장소는 DeskStream 보드용으로 개발된 4가지 주요 프로그램을 포함하고 있습니다.

### 1. [Smart_Info_Station: 스마트 정보 스테이션 (v2.9.0)](./Smart_Info_Station/)
- **기능**: 시계, 날씨, 미세먼지, 유튜브 구독자 수, 증시 지수, 가상화폐 가격, 환율 실시간 모니터링.
- **특징**: **전면 모듈화 아키텍처**, 웹 대시보드 실시간 설정, 비동기 데이터 갱신(FreeRTOS), 네이티브 HAL 기반 고속 SW I2C.
- **버튼 동작**: 버튼 1을 사용하여 HUD 반전 모드 순환, 버튼 4를 사용하여 페이지를 전환합니다.

### 2. [GPS_DASH: 하드웨어 GPS 대시보드](./GPS_DASH/)
- **기능**: 실시간 속도, 나침반, 고도, 위도/경도, 위성 상태, 주행 통계 표시.
- **특징**: NEO-6M GPS 모듈 연동(5Hz), UBX 프로토콜 직접 제어, 8가지 디스플레이 모드 제공.

### 3. [Sound_Visualizer_4_OLEDs: 고속 오디오 비주얼라이저](./Sound_Visualizer_4_OLEDs/)
- **기능**: 아날로그 마이크 입력(G0)을 통한 128밴드 실시간 주파수 시각화.
- **특징**: ESP-DSP 정수형 FFT 가속(Max 70+ FPS), 512x64 초광폭 캔버스 제어, ADC DMA 샘플링.

### 4. [OLED_Wide_test: 초광폭 디스플레이 테스트](./OLED_Wide_test/)
- **기능**: 4개의 OLED를 하나의 512x64 화면으로 제어하는 데모 및 테스트 코드.
- **특징**: Shadow Buffering을 통한 데이터 전송 최적화, LovyanGFX 기반 고속 그래픽 렌더링.

### 5. [Stats_Monitor: 시스템 상태 모니터](./Stats_Monitor/)
- **기능**: PC/Mac의 CPU, GPU, RAM, SSD 상태 및 온도, 네트워크 트래픽 실시간 표시.
- **특징**: 외부 클라이언트 연동, 하드웨어 통계 가젯 모드, 시스템 생체 정보 가시화.

### 6. [DS_DECK: 프로그래머블 매크로 패드 (v1.5.0)](./DS_DECK/)
- **기능**: 4개의 버튼에 텍스트 매크로(한/영 지원), 단축키(Combo), 미디어 컨트롤, **웹 브라우저 런처(Browser)** 할당.
- **특징**: **macOS 프리미엄 런처 지원**(Spotlight 자동화), 초고속 BLE HID 엔진, 웹 기반 실시간 매크로 설정, F1~F12 포함 모든 특수키 지원.

### 7. [Screen_To_OLEDs: 실시간 화면 스트리머 (v1.6.0)](./Screen_To_OLEDs/)
- **기능**: Mac의 화면 일부를 실시간 캡처하여 DeskStream 4-OLED 매트릭스(512x64)에 무선 전송.
- **특징**: 지능형 캡처 엔진(Full 대응), Numba JIT 가속, 6종 디더링 알고리즘, 실시간 설정 변경.
- **버튼 동작**: 버튼 2를 눌러 밝기를 조절하고, 버튼 1을 롱프레스하여 화면을 180도 회전(플립)합니다.

### 8. [Hangeul_Clock: 고화질 폰트 한글 시계 (v2.5.0)](./Hangeul_Clock/)
- **기능**: NTP 동기화 기반의 고화질 한글 시계 및 하이엔드 폰트 렌더링 시스템.
- **특징**: 
    - **제로-스터터(Zero-Stutter) 웹 API**: 폰트 슬롯 이름 메모리 캐싱으로 웹 대시보드 호출 시 시스템 멈춤 현상 근본 해결.
    - **전문가급 안정성 및 보안**: **하드웨어 자가 치유(I2C Recovery)**, **API 입력 범위 검증(Whitelist)** 로직 탑재.
    - **Flat Buffer 메모리 무결성**: 힙 파편화와 메모리 누수를 원천 차단하는 **Flat Buffer** 전략 사용.
    - **비차단(Non-blocking) 엔진**: 상태 머신 기반 애니메이션 루프 및 `refreshNow()` 잔상 방지 로직.
    - **웹 미리보기 동기화**: '색상 반전' 설정이 웹 대시보드 캔버스 미리보기에도 실시간 반영.
    - **64px Modern Font Engine**: 글자 잘림 없는 고해상도 64px(512B) 비트맵 폰트 정식 지원.
    - **지능형 24H 및 날짜 시스템**: 24시간제 지원.
    - **인터랙티브 제어**: 4개의 물리 버튼을 통해 **시보, 화면 반전, 한글/숫자 모드, 시간 형식(12/24H)**을 즉각적으로 변경 가능.
    - **Ecosystem 연동**: 실시간 웹 시뮬레이너(폰트 스튜디오) 및 3초 비동기 동기화 엔진 탑재.

## 🚀 주요 기술적 특징
- **하이브리드 I2C 아키텍처**: 하드웨어 I2C와 레지스터 직접 제어 기반 고속 소프트웨어 I2C를 병렬로 사용하여 4개의 화면을 끊김 없이 제어합니다.
- **성능 최적화**: 부드러운 애니메이션 구현을 위한 Dirty Page 체크 및 그래픽 가속 기법이 적용되었습니다.
- **문서화 표준화**: 전 프로젝트 소스 코드에 Doxygen 스타일 헤더 및 작성자 정보를 도입하여 유지보수성을 극대화했습니다.

---
© 2026 DeskStream Project.
