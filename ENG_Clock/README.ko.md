[English](README.md) | **한국어**

# ENG Clock (English Language Clock) - v1.1.1

NTP로 시간을 맞춰 4개의 OLED에 시·분·초를 **영어 단어**로 표시하는 시계입니다. `Hangeul_Clock`의 구조를 그대로 쓰고 텍스트만 영어로 바꿨습니다.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ 핵심 기능

- **영어 단어 / 숫자 표시**: 긴 단어(`SEVENTEEN`)는 어절 단위 2줄로 표시
- **12H(AM/PM) · 24H 형식**, 영문 요일(`MONDAY`)
- **시간대·DST 자동 처리**: 웹에서 12개 지역 프리셋 또는 직접 입력
- **Font Studio**: 브라우저에서 TTF를 골라 64px 비트맵으로 업로드, 폰트 슬롯 5개
- 애니메이션 5종, 시보

## 🎮 버튼

| 버튼 | 짧게 | 길게 |
| :--- | :--- | :--- |
| BTN 1 | 시보 ON/OFF | 화면 반전(180°) |
| BTN 2 | 단어 ↔ 숫자 표시 | 12H ↔ 24H |
| BTN 3 | 애니메이션 순환 | 폰트 슬롯 순환 (0~4) |
| BTN 4 | 시계 → IP 확인 → 도움말 | 색상 반전 |

## 🚀 설치

1. **펌웨어 굽기** — [웹 플래셔](https://worynim.github.io/DeskStream/)에서 고르거나, 직접 빌드합니다.
   Arduino IDE에서는 `Partition Scheme`을 **Huge APP**로 고릅니다.

   ```bash
   arduino-cli compile --fqbn esp32:esp32:makergo_c3_supermini:PartitionScheme=huge_app .
   ```

2. **WiFi 설정** — 처음 켜면 생기는 `ENG_Clock_Setup` AP에 접속해 WiFi 정보를 입력합니다.
3. **폰트 올리기 (필수)** — 폰트가 없으면 숫자 모드로만 표시됩니다(`Font Required!`). BTN 4로 IP를 확인해 접속하고 Font Studio에서 업로드합니다.

## 📖 자세히

기능 설명, 디렉터리 구조, 빌드·테스트 방법은 [GUIDE.ko.md](GUIDE.ko.md)에 있습니다.

---
© 2026 DeskStream Project. — worynim@gmail.com
