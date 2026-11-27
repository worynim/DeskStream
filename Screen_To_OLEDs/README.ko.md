[English](README.md) | **한국어**

# Screen_To_OLEDs (Screen Streamer) - v2.3.1

PC 화면의 일부를 실시간으로 캡처해 WiFi(UDP)로 DeskStream의 OLED 4개(512x64)에 보내는 프로젝트입니다.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ 핵심 기능

- 캡처 영역을 화면 위 박스로 옮기고 크기 조절, 멀티 모니터 지원
- 디더링 6종(Atkinson 기본), Numba JIT 가속
- 한 기기로 보내기(Direct) 또는 네트워크의 모든 기기로 동시에 보내기(Broadcast)
- 화면 변화가 없으면 전송을 쉬어 CPU·네트워크 사용을 줄임

## 🎮 버튼

| 버튼 | 짧게 | 길게 |
| :--- | :--- | :--- |
| BTN1 | — | 화면 180° 회전 |
| BTN2 | 밝기 4단계 순환 | 도움말 화면 |
| BTN3 | — | — |
| BTN4 | 도움말/대기 화면 닫기 | — |

## 🚀 설치

1. **펌웨어** — [웹 플래셔](https://worynim.github.io/DeskStream/)에서 고르거나 직접 빌드합니다. 처음 켜면 `ESP_SCREEN_OLED` AP에 접속해 WiFi를 설정합니다.
2. **PC 스트리머** — 웹 플래셔에서 macOS(Apple Silicon)·Windows(x64) 앱을 받거나, Python으로 실행합니다.
   macOS에서는 **화면 기록**과 **로컬 네트워크** 권한을 허용해야 합니다.

   ```bash
   pip install Pillow mss numpy numba
   python screen_streamer.py
   ```

3. 보드 IP를 입력하거나 `Scan`으로 찾은 뒤 `START STREAMING`을 누릅니다.

## 📖 자세히

구조, 기능 설명, 실행 파일 안내, 설정 파일은 [GUIDE.ko.md](GUIDE.ko.md)에 있습니다.
