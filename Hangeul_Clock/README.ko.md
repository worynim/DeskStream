[English](README.md) | **한국어**

<img width="778" height="211" alt="image" src="https://github.com/user-attachments/assets/0eabecfa-ce8a-4527-8e50-6d8101b9150c" />

<img width="879" height="1067" alt="image" src="https://github.com/user-attachments/assets/325bc0a3-bf28-45f6-92ca-6e8ce69a8893" />

# Hangeul Clock (한글 시계) - v2.11.1

NTP로 시간을 맞춰 4개의 OLED에 시·분·초를 고화질 한글 또는 숫자로 표시하는 시계입니다.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ 핵심 기능

- **한글 / 숫자 표시**, 12H(오전/오후) · 24H(날짜 표시) 형식
- **64px 고화질 폰트**: 웹 Font Studio에서 TTF를 골라 바로 업로드, 폰트 슬롯 5개
- **숫자 전환 애니메이션** 여러 종, 시보 멜로디, 밝기 조절
- **웹 대시보드**: 설정 변경과 실시간 미리보기, 버튼으로 바꾼 설정도 3초 안에 반영

## 🎮 버튼

| 버튼 | 짧게 | 길게 (1초) |
| :--- | :--- | :--- |
| BTN 1 | 시보 ON/OFF | 화면 반전(180°) |
| BTN 2 | 한글 ↔ 숫자 표시 | 12H ↔ 24H |
| BTN 3 | 애니메이션 순환 | 폰트 슬롯 순환 (0~4) |
| BTN 4 | 시계 → IP 확인 → 도움말 | 색상 반전 |

부팅할 때 **BTN 1**을 누르고 있으면 WiFi 설정이 지워지고 설정 모드로 들어갑니다.

## 🚀 설치

1. **펌웨어 굽기** — [웹 플래셔](https://worynim.github.io/DeskStream/)에서 고르거나, 직접 빌드합니다.
   Arduino IDE에서는 `Partition Scheme`을 **Huge APP**로 고릅니다.
2. **WiFi 설정** — 처음 켜면 생기는 `Hangeul_Clock_Setup` AP에 접속해 WiFi 정보를 입력합니다.
3. **폰트 올리기** — BTN 4로 IP를 확인해 브라우저로 접속하고 Font Studio에서 폰트를 업로드합니다.

## 📖 자세히

기능 설명, 파일 구성, 하드웨어, 빌드 설정은 [GUIDE.ko.md](GUIDE.ko.md)에 있습니다.

---
© 2026 DeskStream Project.
