[English](README.md) | **한국어**

# ⏱️ DeskStream Universal Timer (범용 타이머) - v1.3.0

OLED 4개에 시·분·초와 상태를 하나씩 표시하고, 아래 버튼으로 맞추는 카운트다운 타이머입니다.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ 핵심 기능

- **화면마다 버튼 하나**: 시 · 분 · 초를 각 화면 아래 버튼으로 맞추고, 길게 누르면 빠르게 증가
- 종료 시 화면 점멸과 오르골 알람, 아무 버튼이나 누르면 처음 시작한 시간으로 복원
- **마지막 설정 시간 기억**: START할 때 저장돼 다시 켜도 그 시간으로 시작

## 🎮 버튼

| 버튼 | 짧게 | 누르고 있기 |
| :--- | :--- | :--- |
| BTN 1 | 시 +1 | 시 빠르게 증가 |
| BTN 2 | 분 +1 | 분 빠르게 증가 |
| BTN 3 | 초 +1 | 초 빠르게 증가 |
| BTN 4 | 시작 / 일시정지 / 알람 끄기 | 00:00:00 초기화 (1초) |

## 🚀 설치

[웹 플래셔](https://worynim.github.io/DeskStream/)에서 고르거나 직접 빌드합니다.

```bash
arduino-cli compile --fqbn esp32:esp32:nologo_esp32c3_super_mini .
```

## 📖 자세히

기능 설명, 하드웨어, 파일 구성은 [GUIDE.ko.md](GUIDE.ko.md)에 있습니다.
