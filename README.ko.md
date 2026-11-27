[English](README.md) | **한국어**

# 🖥️ DeskStream: ESP32-C3 4-OLED Multifunctional Board

DeskStream은 ESP32-C3와 4개의 OLED 디스플레이를 활용한 다기능 데스크톱 정보 스테이션 프로젝트입니다.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

> 🌐 **웹 플래셔**: [https://worynim.github.io/DeskStream/](https://worynim.github.io/DeskStream/) — USB로 연결한 ESP32-C3 보드에 원하는 프로젝트를 브라우저에서 바로 업로드할 수 있습니다. ([사용법](GUIDE.ko.md#-웹-플래셔-web-flasher) · [소스](./web/))

## 🛠️ 하드웨어 사양 (Hardware Specification)
- **MCU**: ESP32-C3 (SuperMini)
- **Display**: 0.96" SSD1306 OLED (128x64) x 4개 (가로 배치 시 512x64 광폭 디스플레이 구현 가능)
- **Buttons**: 4개의 사용자 입력 버튼 (GPIO 1, 4, 10, 9)
- **Interface**:
  - **HW I2C**: SDA(GPIO 5), SCL(GPIO 6) - OLED 1, 2 제어
  - **SW I2C**: SDA(GPIO 2), SCL(GPIO 3) - OLED 3, 4 제어
- 상세 정보: [hardware.md](./hardware.md) 참조

## 📁 주요 프로젝트 (Sub-Projects)
DeskStream 보드용 16가지 프로그램입니다. 번호는 웹 플래셔·런처 목록 번호와 같습니다(0번은 런처).
이름을 누르면 프로젝트 README로 이동합니다.

| # | 프로젝트 | 설명 |
|---|---|---|
| | **시계·시간** | |
| 1 | [Hangeul_Clock](./Hangeul_Clock/README.ko.md) (v2.11.1) | 고화질 폰트 한글 단어 시계 |
| 2 | [ENG_Clock](./ENG_Clock/README.ko.md) (v1.1.1) | 고화질 폰트 영어 단어 시계 |
| 3 | [Chinese_Clock](./Chinese_Clock/README.ko.md) (v1.1.1) | 고화질 폰트 중국어 한자 시계 (간체·번체) |
| 4 | [World_Clock](./World_Clock/README.ko.md) (v0.1.0) | 화면마다 도시 하나씩, 세계 4곳 시계 |
| 5 | [Universal_Timer](./Universal_Timer/README.ko.md) (v1.3.0) | 시·분·초를 화면별로 맞추는 카운트다운 타이머 |
| | **정보·측정** | |
| 6 | [Smart_Info_Station](./Smart_Info_Station/README.ko.md) (v2.14.1) | 날씨·미세먼지·증시·환율·유튜브 등 정보 표시 |
| 7 | [GPS_DASH](./GPS_DASH/README.ko.md) (v2.7.1) | 속도·나침반·고도를 보여 주는 GPS 대시보드 |
| 8 | [Wireless_Analyzer](./Wireless_Analyzer/README.ko.md) (v0.5.1) | 2.4GHz 채널 혼잡도·AP·BLE 기기 분석기 |
| | **PC 연동** | |
| 9 | [Stats_Monitor](./Stats_Monitor/README.ko.md) (v1.6.1) | PC/Mac CPU·GPU·RAM·온도 모니터 |
| 10 | [Screen_To_OLEDs](./Screen_To_OLEDs/README.ko.md) (v2.3.1) | Mac 화면 일부를 512x64로 실시간 전송 |
| 11 | [Agent_Monitor](./Agent_Monitor/README.ko.md) (v0.8.2) | Claude Code 세션 상태 표시·권한 승인 |
| | **블루투스 입력** | |
| 12 | [DS_DECK](./DS_DECK/README.ko.md) (v1.6.1) | 버튼 4개 BLE 매크로 패드 |
| 13 | [Page_Turner](./Page_Turner/README.ko.md) (v1.1.0) | 웹소설·웹툰·쇼츠·발표용 BLE 페이지 터너 |
| | **그래픽·데모** | |
| 14 | [Sound_Visualizer_4_OLEDs](./Sound_Visualizer_4_OLEDs/README.ko.md) (v3.6.0) | 마이크 입력 128밴드 오디오 비주얼라이저 |
| 15 | [Game_Of_Life](./Game_Of_Life/README.ko.md) (v1.1.1) | 512x64 생명 게임 |
| 16 | [OLED_Wide_test](./OLED_Wide_test/README.ko.md) (v1.3.1) | 4화면을 512x64 한 화면으로 쓰는 데모 |

## 🚀 시작하기

- **바로 쓰기**: [웹 플래셔](https://worynim.github.io/DeskStream/)에서 프로젝트를 골라 굽습니다 (Chrome/Edge 데스크톱, Android Chrome 148+).
- **직접 빌드**: 모든 프로젝트가 공통 라이브러리 [DeskStreamCore](./libraries/DeskStreamCore/README.ko.md)를 씁니다. PC마다 한 번 저장소 루트에서 아래를 실행하고, Arduino IDE 라이브러리 매니저에서 **U8g2**를 설치합니다.

  ```bash
  bash scripts/link-arduino-lib.sh
  ```

Windows 설정, 런처, 펌웨어 배포 절차 등 자세한 내용은 [GUIDE.ko.md](GUIDE.ko.md)를 보세요.
AI로 새 프로젝트를 만들 때는 [`AI_GUIDE.md`](./AI_GUIDE.md)를 먼저 읽게 하세요.

---

## 📜 라이선스

| 대상 | 라이선스 | 파일 |
|---|---|---|
| 하드웨어 설계 (회로도 [`DeskStream_Schematic.pdf`](./DeskStream_Schematic.pdf), [`hardware.md`](./hardware.md)) | CERN-OHL-S-2.0 | [`LICENSE-HARDWARE`](./LICENSE-HARDWARE) |
| 소프트웨어 (펌웨어, DeskStreamCore 라이브러리, PC 클라이언트, 웹 플래셔, 스크립트 등 그 밖의 전부) | MIT | [`LICENSE`](./LICENSE) |

- 저장소에 포함된 외부 글꼴 데이터(갈무리7, Noto Sans KR/SC, Source Han Sans, Roboto Condensed, 나눔 등을 변환한 것)는 각 글꼴의 원래 라이선스를 따릅니다. 갈무리7은 SIL OFL 1.1입니다.
- 이 설계로 보드를 만들어 배포할 때는 CERN-OHL-S-2.0에 따라 설계 소스 위치(이 저장소 주소)를 함께 표기해야 합니다.

---

© 2026 DeskStream Project.
