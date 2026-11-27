[English](README.md) | **한국어**

# 🖥️ Stats_Monitor - v1.6.1

PC/Mac의 시스템 지표(CPU, GPU, RAM, SSD, 온도, 소비전력)를 BLE로 받아 OLED 4개에 실시간으로 보여 주는 데스크 가젯입니다.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ 핵심 기능

- CPU/GPU 사용량·온도, RAM/SSD 사용 현황, 네트워크·전력 표시
- PC 클라이언트(트레이 앱)가 켜지면 보드와 자동으로 BLE 연결
- 화면 180° 회전, 밝기 4단계 — 다시 켜도 유지

## 🎮 버튼

| 버튼 | 짧게 |
| :--- | :--- |
| BTN 1 | 화면 180° 회전 (출력 순서도 4-3-2-1로 반전) |
| BTN 2 | 밝기 4단계 순환 |
| BTN 3 | (미할당) |
| BTN 4 | 도움말 켜기/끄기 |

## 🚀 설치

1. **펌웨어** — [웹 플래셔](https://worynim.github.io/DeskStream/)에서 고르거나 직접 빌드합니다.
2. **PC 클라이언트** — 웹 플래셔에서 macOS(Apple Silicon)·Windows(x64) 실행 파일을 받거나, Python으로 실행합니다.

   ```bash
   pip install -r client_py/requirements.txt
   python client_py/main.py
   ```

## 📖 자세히

클라이언트 패키지, 실행 파일 빌드, Release 배포 절차는 [GUIDE.ko.md](GUIDE.ko.md)에 있습니다.
