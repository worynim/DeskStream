[English](README.md) | **한국어**

# Chinese Clock (중국어 단어 시계) - v1.1.1

NTP로 시간을 맞춰 4개의 OLED에 **중국어 한자**로 시·분·초를 표시하는 시계입니다. `Hangeul_Clock` / `ENG_Clock`의 구조를 그대로 쓰고 텍스트만 중국어로 바꿨습니다.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ 핵심 기능

- **4화면 표시**: 오전/오후(또는 요일) · 시 · 분 · 초를 화면마다 한 칸씩, 최대 4자라 줄바꿈 없음
- **표준형 한자 표현**: `两点`, `三点零五分`, `三点半`, `九点整`, `上午`/`下午`, `星期日`~`星期六`
- **간체 / 번체 / 숫자 전환**: `简体 → 繁體 → 數字`
- **Font Studio**: 브라우저에서 TTF를 골라 기기에 업로드, 폰트 슬롯 5개
- **시간대·DST 자동 처리**(기본 `CST-8`), 애니메이션 6종, 웹 설정 페이지

## 🎮 버튼

| 버튼 | 짧게 | 길게 |
|:--|:--|:--|
| BTN1 | 시보 ON/OFF | 화면 반전(180°) |
| BTN2 | 표시 방식 简体 → 繁體 → 數字 | 12H ⇄ 24H |
| BTN3 | 애니메이션 순환 (6종) | 폰트 슬롯 순환 (0~4) |
| BTN4 | 페이지 전환 (시계 → IP → 도움말) | 색상 반전 |

## 🚀 설치

1. **펌웨어 굽기** — [웹 플래셔](https://worynim.github.io/DeskStream/)에서 고르거나, 직접 빌드합니다.
   Arduino IDE에서는 `Partition Scheme`을 **Huge APP**로 고릅니다.

   ```bash
   arduino-cli compile --fqbn esp32:esp32:makergo_c3_supermini:PartitionScheme=huge_app .
   ```

2. **폰트 올리기 (필수)** — 폰트가 없으면 한자가 표시되지 않습니다. 기기 IP(BTN4 짧게)로 접속해
   간체·번체를 모두 담은 CJK 폰트(Noto Sans SC, Source Han Sans 등)를 업로드합니다.

## 📖 자세히

화면 구성, 표현 규칙, 웹 API, 테스트, 메모리, 알려진 제약은 [GUIDE.ko.md](GUIDE.ko.md)에 있습니다.
폰트 업로드 단계별 방법은 [폰트 업로드 절차](GUIDE.ko.md#-폰트-업로드-절차)를 보세요.

---
© 2026 DeskStream Project. — worynim@gmail.com
