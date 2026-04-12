# Hangeul Clock (한글 시계) - v1.9.0

NTP 서버로부터 시간을 동기화하여 4개의 OLED 디스플레이에 고화질 한글 및 숫자로 시, 분, 초를 표시하는 ESP32-C3 기반 프리미엄 데스크 가젯입니다.

## ✨ 핵심 혁신 기능 (Major Features)

### 1. Modern Modular Architecture (모듈형 아키텍처)
- **H/CPP separation**: 펌웨어 로직을 클래스별 모듈(.h/.cpp)로 완전 분리하여 유지보수성과 코드 가독성을 극대화했습니다.
- **WebManager Encapsulation**: 웹 서버 엔진을 독립적인 클래스로 구축하여 메인 로직과의 결합도를 낮췄습니다.
- **Hardware Abstraction Layer (HAL)**: 하드웨어 종속적인 I2C 드라이버 로직을 `I2CPlatform` 클래스로 분리하여 계층형 아키텍처를 실현했습니다.

### 2. High-Performance Memory Optimization (고성능 메모리 최적화)
- **Dynamic Bitmap Cache**: `std::vector` 기반의 동적 할당 캐시 시스템을 통해 폰트 크기에 최적화된 RAM 사용량을 구현했습니다 (기존 대비 최대 50% RAM 절감).
- **Flash-Based Web UI**: 거대한 HTML 데이터를 Flash 메모리(PROGMEM)로 이동하여 시스템 가용 리소스를 확보했습니다.

### 3. 64px Modern Font Engine (고화질 폰트 엔진)
- **광폭 비트맵 지원**: 기존 32px의 한계를 넘어 **64px(512바이트) 고해상도 낱자**를 완벽하게 지원하여 대형 폰트 사용 시에도 글자 잘림 없는 미려한 출력을 보장합니다.
- **지능형 규격 감지**: 32px(Legacy)와 64px(Modern) 데이터를 실시간으로 판별하여 처리하는 하이브리드 인코딩 파이프라인을 구축했습니다.

### 2. Advanced Layered Rendering (투명 겹침 렌더링)
- **Transparent Overlap**: `setBitmapMode(1)` 조작을 통해 비트맵의 검은 배경 간섭을 제거했습니다.
- **예술적 타이포그래피**: 글자와 글자가 서로를 자연스럽게 가로지르며 겹치는 효과를 구현하여, 좁은 OLED 화면 내에서 입체감 있고 정교한 레이아웃을 제공합니다.

### 3. Hardware Self-Healing & Data Integrity (하드웨어 자가 치유 및 무결성)
- **I2C Recovery System**: 실시간 통신 모니터링 중 에러가 감지되면 자동으로 버스를 재초기화하고 디스플레이 장치 주소를 재할당하여 시스템 멈춤을 스스로 극복합니다.
- **Atomic Cache Update**: 비트맵 데이터 로딩 시 임시 버퍼와 `std::move`를 활용한 트랜잭션 방식을 적용하여 파일 시스템 오류 시에도 기존 캐시의 안전을 보장합니다.
- **Concurrency Guard**: FreeRTOS 태스크 간 동기화 타임아웃 처리를 통해 데이터 오염(Race Condition)을 원천 차단했습니다.

### 4. Premium Startup Experience (부팅 UX 혁신)
- **Startup Melody**: 전원 인가 직후 경쾌한 멜로디(피드백)를 재생하여 기기 준비 완료를 알립니다.
- **OLED Boot Log**: 부팅 및 네트워크 연결 과정을 1번 화면에 실시간 로그 형식으로 표시합니다.
- **Progress Animation**: WiFi/NTP 동기화 등 대기 시간이 필요한 작업 시 점(`.`)이 늘어나는 프로그레스 애니메이션을 제공합니다.

### 5. Smart Time & Date System
- **24시간제 정식 지원**: 12H(오전/오후)와 24H(0~23시) 형식을 자유롭게 선택할 수 있습니다.
- **지능형 날짜 자동 표시**: 24H 모드 사용 시, 첫 번째 OLED가 현재 날짜(예: '13일')를 자동으로 판단하여 표시하는 스마트 레이아웃이 적용됩니다.
- **전 구간 Zero-padding**: 숫자 모드에서 시, 분, 초 전 구간에 '05분'과 같이 0을 채워 넣는 `%02d` 포맷팅을 적용하여 시인성을 극대화했습니다.

### 6. Interactive Web Dashboard & Real-time Sync
- **Font Studio v2+**: 웹 대시보드에서 TTF 폰트 업로드, 낱자 비트맵 변환, 실시간 미리보기를 원스톱으로 제공합니다.
- **실시간 웹 시뮬레이너**: 기기의 현재 설정값(표시 모드, 시간 형식 등)을 웹에서 실시간으로 시뮬레이션하고 제어할 수 있습니다.
- **3초 비동기 폴링 (Polling)**: 물리 버튼 조작 시 별도의 새로고침 없이 **3초 내에 웹 화면에 자동 반영**되는 강력한 동기화 엔진을 탑재했습니다.
- **폰트 즉시 반영**: 웹에서 폰트 업로드 즉시 별도의 재부팅 없이 기기 화면에 새 폰트가 즉각 적용됩니다.

## 📂 프로젝트 파일 구성 및 기능 (Project Structure)

펌웨어는 기능별로 철저히 모듈화되어 있으며, 각 파일의 역할은 다음과 같습니다.

| 파일명 | 구분 | 주요 기능 및 역할 |
| :--- | :--- | :--- |
| **Hangeul_Clock.ino** | **Entry** | 프로그램 진입점. 시스템 초기화(Setup) 및 메인 서비스 루프 관리 |
| **config.h** | **Config** | 전역 상수, 핀 맵, 하드웨어 타이밍 및 시스템 설정값 정의 |
| **config_manager.h/cpp** | **Service** | 설정값(JSON)의 Flash 메모리 저장(NVS/LittleFS) 및 지능형 지연 저장(Lazy Save) 관리 |
| **display_manager.h/cpp** | **Manager** | 고수준 디스플레이 제어, IP 확인/도움말 등 UI 스테이지 및 애니메이션 트리거 관리 |
| **hangeul_time.h/cpp** | **Logic** | 시간을 한글 텍스트 및 숫자 문자열로 변환하는 비즈니스 로직 (24H/정각 처리 포함) |
| **i2c_platform.h/cpp** | **HAL** | 하드웨어 I2C 인터페이스 추상화 및 4개 OLED에 대한 고속 병렬 전송 로직 구현 |
| **input_manager.h/cpp** | **Service** | 버튼 입력 디바운싱, 짧은/긴 누름 감지 및 인터럽트 안전 콜백 관리 |
| **logger.h/cpp** | **Service** | 통합 로깅 시스템. OLED 상태 표시와 시erial 모니터(도트 애니메이션 포함) 출력 관리 |
| **renderer.h/cpp** | **Engine** | 비트맵 캐시 관리 및 고수준 그래픽 렌더링 엔진 (Dither, Zoom, Flip 등) |
| **web_manager.h/cpp** | **Web** | 비동기 웹 서버, JSON API 처리 및 기기-웹 대시보드 간 설정 동기화 관리 |
| **web_pages.h** | **Resource** | 폰트 스튜디오 및 대시보드용 임베디드 HTML/Javascript/CSS 리소스 |

## 🛠 하드웨어 구성
- **MCU**: ESP32-C3 (SuperMini)
- **Display**: 4x SSD1306 OLED (128x64) - I2C 고속 연결
- **Input**: 1x Reset/Function Button (GPIO 1)
- **Output**: Passive Buzzer (GPIO 7) - 고정밀 비차단 부저 로직

## 🎮 버튼 동작 (Button Controls)

기기 전면에 배치된 4개의 버튼을 통해 다양한 설정을 즉각적으로 변경할 수 있습니다.

| 버튼 | 짧게 누름 (Short Press) | 길게 누름 (Long Press - 1초) |
| :--- | :--- | :--- |
| **BTN 1** | **시보 ON/OFF**: 정시 소리 알림 설정 | **화면 반전**: 디스플레이 상하 180도 회전 |
| **BTN 2** | **표시 모드 switching**: 한글 ↔ 숫자 표시 | **시간 형식 switching**: 12H ↔ 24H 형식 |
| **BTN 3** | **애니메이션 Toggle**: 숫자 변환 효과 사용/해제 | - |
| **BTN 4** | **화면 전환**: 시계 ↔ IP 확인 ↔ 도움말 순환 | - |

> [!TIP]
> **WiFi 초기화**: 기기 부팅 시 **BTN 1**을 누르고 있으면 저장된 WiFi 설정이 삭제되며 설정 모드로 진입합니다.

## 📖 사용 방법
1. **WiFi 설정**: 처음 부팅 시 생성되는 `Hangeul_Clock_Setup` AP에 접속하여 WiFi 정보를 입력합니다.
2. **도움말 확인**: 기기 동작 중 **BTN 4**를 눌러 도움말 페이지에 진입하면 현재 설정 상태(CHIME, FLIP 등)를 확인할 수 있습니다.
3. **설정 초기화**: 부팅 시 버튼(BTN 1)을 길게 누르고 있으면 WiFi 정보 및 시스템 설정이 초기화됩니다.

## 📚 라이브러리 의존성
- `U8g2` (고속 그래픽 제어)
- `WiFiManager` (네트워크 설정 관리)
- `LittleFS` (비트맵 및 설정 저장)

---
© 2026 DeskStream Project.
