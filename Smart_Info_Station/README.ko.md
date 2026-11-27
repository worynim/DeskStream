[English](README.md) | **한국어**

# ESP32-C3 4-OLED 스마트 정보 스테이션 (Smart_Info_Station) - v2.14.0

OLED 4개에 원하는 정보 위젯을 골라 놓고 실시간으로 보는 데스크톱 정보 표시기입니다.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ 핵심 기능

- **위젯**: 한국·세계 시각, 동네·해외 날씨, 미세먼지, 유튜브 구독자 수, 증시(코스피·코스닥·S&P 500), 비트코인·이더리움, 환율, D-day, 일출·일몰·달 모양, 주간 날씨, 기온 그래프, 관심 종목
- **웹 설정**: 화면 배치, 채널·지역·도시·종목을 브라우저에서 이름으로 검색해 설정 (한국어/영어)
- 데이터는 백그라운드에서 받아 화면이 멈추지 않고, 증시는 장이 열린 시간에만 갱신
- 설정은 NVS에 저장돼 다시 켜도 유지

## 🎮 버튼

| 버튼 | 짧게 | 길게 |
| :--- | :--- | :--- |
| BTN 1 | 화면 방향 순환 (Normal → Mirror H → 180° → Mirror V) | — |
| BTN 2 | 밝기 4단계 | — |
| BTN 3 | — | 페이지 자동 순환 켜기/끄기 |
| BTN 4 | 페이지 전환 | 모든 위젯 즉시 갱신 |

전원을 켤 때 BTN 1을 누르고 있으면 WiFi 설정이 초기화됩니다. 5번 페이지에서 버튼 도움말을 볼 수 있습니다.

## 🚀 설치

1. [웹 플래셔](https://worynim.github.io/DeskStream/)에서 고르거나 직접 빌드합니다. 직접 빌드할 때는 `Partition Scheme`을 **Huge APP**로 고릅니다.
2. 처음 켜면 `Smart_Info_Station` 핫스팟에 접속해 `192.168.4.1`에서 WiFi를 설정합니다.
3. 1번 화면에 나온 IP로 브라우저에서 접속해 위젯과 지역 등을 설정합니다.

## 📖 자세히

기능 설명, 하드웨어, 빌드 설정, 파일 구조는 [GUIDE.ko.md](GUIDE.ko.md)에, 버전별 변경 사항은 [RELEASE_NOTES.md](./RELEASE_NOTES.md)에 있습니다.
