# DeskStream Game of Life 🧬

ESP32-C3 기반 DeskStream 하드웨어를 위한 단독 구동형(Standalone) **Conway's Game of Life** 펌웨어입니다. 4개의 OLED 디스플레이를 하나로 통합하여 512x64 해상도의 거대한 캔버스에서 생태계 시뮬레이션을 즐길 수 있습니다.

## 🚀 주요 특징

- **대화면 시뮬레이션**: 128x64 OLED 4개를 병렬 제어하여 512x64 해상도 구현.
- **메모리 최적화**: 1-bit 단위 비트마스킹 연산으로 ESP32-C3의 메모리 사용량 최소화 (총 8KB 사용).
- **고속 렌더링**: 변경된 타일만 골라 전송하는 Dirty Tile Tracking 및 비동기 병렬 I2C 전송 기술 적용.
- **단독 구동 (Standalone)**: Wi-Fi나 PC 연결 없이 기기 전원만으로 동작.
- **물리 제어**: 4개의 버튼을 이용한 실시간 시뮬레이션 제어.
- **시각/청각 피드백**: 부팅 멜로디, 스플래시 화면 및 버튼 비프음 지원.

## 🛠 하드웨어 구성

- **MCU**: ESP32-C3 (RISC-V)
- **Display**: 128x64 SSD1306 OLED x 4 (Total 512x64)
- **Input**: Tactile Button x 4 (GPIO 1, 4, 10, 9)
- **Audio**: Active/Passive Buzzer (GPIO 7)

## 🎮 조작 가이드

| 버튼 | 기능 | 설명 |
| :--- | :--- | :--- |
| **BTN 1** | **재생 / 일시정지** | 시뮬레이션을 멈추거나 다시 시작합니다. |
| **BTN 2** | **수동 단계 진행** | 일시정지 상태에서만 동작하며, 누를 때마다 1세대씩 진행합니다. |
| **BTN 3** | **속도 조절** | 시뮬레이션 속도를 전환합니다 (5fps ➔ 10fps ➔ 20fps). |
| **BTN 4** | **패턴 리셋** | 현재 상태를 지우고 새로운 무작위 세포 패턴을 생성합니다. |

## 📦 설치 및 빌드 방법

1.  **필수 라이브러리**: Arduino IDE 라이브러리 매니저에서 `U8g2`를 설치하세요.
2.  **보드 설정**: `ESP32C3 Dev Module`을 선택하세요.
3.  **컴파일 및 업로드**: `Game_Of_Life.ino` 파일을 열고 업로드하세요.

---
Developed by Antigravity AI for DeskStream Project.
