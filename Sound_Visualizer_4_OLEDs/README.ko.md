[English](README.md) | **한국어**

# ESP32-C3 Ultra-Wide OLED (512x64) Matrix (v3.6.0)

마이크 소리를 OLED 4개를 이은 **512x64 초광폭 화면**에 실시간으로 보여 주는 오디오 비주얼라이저입니다.
ESP-DSP 정수형 FFT로 최대 70+ FPS를 냅니다.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ 핵심 기능

- **6가지 모드**: 스펙트럼(128밴드) · 파형 · 미러 · 스펙트로그램 · 도트 매트릭스 · 파티클
- 입력 음량에 맞춰 감도 자동 조절(AGC), 노이즈 캘리브레이션
- 아날로그 마이크(GPIO 0)를 DMA로 40kHz 샘플링

## 🎮 버튼

| 버튼 | 짧게 | 길게 |
| :--- | :--- | :--- |
| BTN 1 | 화면 180° 회전 | — |
| BTN 2 | — | 버튼 도움말 |
| BTN 4 | 시각화 모드 순환 | 노이즈 캘리브레이션 |

## 🚀 설치

1. 아날로그 마이크 모듈을 GPIO 0에 연결합니다.
2. [웹 플래셔](https://worynim.github.io/DeskStream/)에서 고르거나 직접 빌드합니다 (U8g2 · LovyanGFX · DeskStreamCore 필요).

## 📖 자세히

성능 최적화, 하드웨어, 라이브러리, 모드별 설명은 [GUIDE.ko.md](GUIDE.ko.md)에 있습니다.
