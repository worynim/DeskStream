[English](README.md) | **한국어**

# ESP32-C3 4-OLED GPS 대시보드 (GPS_DASH) - v2.7.1

NEO-6M GPS 모듈을 연결해 OLED 4개에 GPS 정보를 보여 주는 대시보드입니다. WiFi·웹 기능 없이 GPS 표시에만 집중합니다.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ 핵심 기능

- **8가지 화면**: 시계, 속도계, 나침반, 위성 상태(DOP), 위도/경도, 고도, 달력, 주행 통계
- 화면마다 버튼으로 표시 정보를 따로 고름, 설정은 다시 켜도 유지
- GPS 5Hz 수신, 위치를 잡기 전에도 시간 먼저 표시, 위성 잡힘/놓침 알림음

## 🎮 버튼

| 버튼 | 짧게 | 길게 |
| :--- | :--- | :--- |
| BTN 1 | OLED 1 화면 순환 | 화면 전체 방향 순환 |
| BTN 2 | OLED 2 화면 순환 | 버튼 도움말 켜기/끄기 |
| BTN 3 | OLED 3 화면 순환 | 주행 통계 초기화 |
| BTN 4 | OLED 4 화면 순환 | 데모 모드 켜기/끄기 |

## 🚀 설치

1. GPS 모듈을 연결합니다: RX(GPIO 20), TX(GPIO 21).
2. [웹 플래셔](https://worynim.github.io/DeskStream/)에서 고르거나 직접 빌드합니다 (`U8g2`, `Adafruit GPS Library`, DeskStreamCore 필요).

## 📖 자세히

기능 설명, 하드웨어 배선, 소프트웨어 설정, 파일 구조는 [GUIDE.ko.md](GUIDE.ko.md)에 있습니다.
