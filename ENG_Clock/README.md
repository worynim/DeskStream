# ENG Clock (English Language Clock) - v1.0.0

NTP 서버로부터 시간을 동기화하여 4개의 OLED 디스플레이에 **영어 단어**로 시, 분, 초를 표시하는 ESP32-C3 기반 프리미엄 데스크 가젯입니다. `Hangeul_Clock` v2.6.0의 기능·품질을 100% 유지하면서 텍스트만 영어로 교체한 포트입니다.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ 핵심 혁신 기능 (Major Features)

### 1. 2-Line Word Layout (어절 단위 2줄 레이아웃)
- **한 화면 4자 제한 극복**: 영어 최장 단어 `SEVENTEEN`(9자)을 수용하기 위해 어절 단위 줄바꿈을 도입했습니다. 어절이 2개 이상이면 무조건 2줄로 분리하고, 각 줄은 가로 중앙 정렬됩니다.
- **강제 줄바꿈 기각**: 글자 수 기준 강제 줄바꿈은 `WEDNESDAY`를 깨뜨리므로 배제했습니다.
- **1줄 세로 정렬 사전 계산**: `LayoutChar::y`로 줄별 세로 위치를 미리 계산해 전달하여, 호출부에 같은 산술이 복제되지 않습니다.

### 2. POSIX TZ Timezone System (시간대 시스템)
- **DST 자동 처리**: 고정 오프셋 대신 POSIX TZ 문자열(`KST-9`, `EST5EDT,...` 등)을 사용해 서머타임이 자동으로 적용됩니다.
- **웹 UI 시간대 선택**: 12개 지역 프리셋 + Custom 직접 입력. 설정값은 NVS(Preferences)에 영속 저장됩니다.
- **입력 검증**: TZ 문자열 화이트리스트 검증(`tz_util`)으로 잘못된 설정이 시스템 시간을 망가뜨리지 않습니다.

### 3. Modular Architecture (모듈형 아키텍처) — 한글판과 동일
- **H/CPP separation**: 펌웨어 로직을 클래스별 모듈(.h/.cpp)로 완전 분리.
- **순수 로직 분리 (Native-Testable)**: 시간 변환(`english_time_core`), TZ 검증(`tz_util`), 레이아웃(`layout_engine` + `renderer_geometry`)을 Arduino 의존 없는 순수 C++로 분리해 **호스트에서 단위 테스트**가 가능합니다.
- **Hardware Abstraction Layer (HAL)**: 하드웨어 종속 I2C 드라이버 로직을 `I2CPlatform` 클래스로 분리.
- **Flat Buffer & Static Allocation**: 힙 파편화와 메모리 누수를 원천 차단하는 Flat Buffer 전략.

### 4. Font Studio (브라우저 폰트 스튜디오)
- **TTF → 비트맵 원스톱 변환**: 브라우저에서 TTF를 업로드하면 38개 영문 낱자(A–Z, 0–9, `,` `'` `.` 등)를 64px 고해상도 비트맵으로 래스터화해 기기에 직접 업로드합니다.
- **잉크 중앙 정렬 래스터라이저**: em 박스 중앙이 아닌 **실측 잉크 박스 합집합**을 래스터 세로 중앙에 놓아(대문자 A–Z 기준) 크기를 키워도 위·아래 여백이 균등하게 유지됩니다.
- **글자별 가로 중앙 정렬**: 넓은 글자(`W`)와 좁은 글자(`I`) 모두 각자의 잉크 중심이 셀 중앙에 옵니다.
- **폰트 슬롯 5개**: LittleFS `/f0`~`/f4` 슬롯에 폰트 세트를 저장하고 웹에서 선택합니다.
- **실시간 미리보기·시뮬레이너**: 업로드 전 캔버스 미리보기와 기기 설정 실시간 시뮬레이션.

### 5. Interactive Control & Animation — 한글판과 동일
- **4버튼 입력** (GPIO 1/4/10/9): 시보, 표시모드, 시간형식, 화면반전 등 즉시 변경.
- **5가지 비차단 애니메이션**: Scroll Up/Down, Vertical Flip, Dithered Fade, Zoom. 2줄 레이아웃에서 이동량이 `LINE_HEIGHT`로 축소되고 줄 밴드 클립으로 격리됩니다.
- **12/24시간제 + 영문 요일**: 12H(AM/PM), 24H 모두 지원하며 요일은 모드 무관하게 영문(`MONDAY`)으로 표기합니다.
- **숫자 모드 단위 표기**: `02 H 15 M 30 S`처럼 단위 접미사가 인라인으로 붙습니다.

### 6. Hardware Self-Healing & Test Infrastructure
- **I2C Recovery System**: 버스 에러 감지 시 자동 재초기화 및 주소 재할당.
- **호스트 테스트 인프라**: 네이티브 C++ 단위 테스트 4종 + 웹↔펌웨어 교차 검증 6종 (`test/` 참고 — `bash test/js/run_all.sh` 한 번에 전체 검증).

## 📁 디렉터리 구조

```
ENG_Clock/
├── ENG_Clock.ino        메인 엔트리 (Setup / 루프 제어)
├── config.h / config_manager.*   전역 상수 / 설정 영속화 (NVS)
├── english_time.*       시간 → 영어 텍스트 변환 (Arduino String 인터페이스)
├── english_time_core.*  순수 시간 변환 로직 (네이티브 테스트 대상)
├── tz_util.*            POSIX TZ 문자열 검증
├── layout_engine.*      어절 단위 2줄 레이아웃 엔진
├── renderer_geometry.*  글자 기하 판별 (192B/384B)
├── renderer.*           U8g2 렌더러 (밴드 클립, 겹침 렌더링)
├── display_manager.*    4-OLED 병렬 출력, 시간대 적용, 애니메이션
├── input_manager.*      4버튼 비차단 입력
├── i2c_platform.*       HW/SW I2C HAL
├── web_manager.*        웹 서버 엔진
├── web_pages.h          Font Studio 웹 UI (HTML/JS 내장)
├── logger.*             로깅
├── PLAN.md              설계 문서
├── 수정할 사항.md        버그/개선 추적
└── test/                호스트 단위·교차 검증 (test/README.md)
```

## 🚀 빌드

```bash
arduino-cli compile --fqbn esp32:esp32:nologo_esp32c3_super_mini .
```

## 🧪 테스트

```bash
bash test/js/run_all.sh   # JS ↔ 펌웨어 교차 검증 전체
cd test && g++ -std=c++11 -I.. test_english_time.cpp ../english_time_core.cpp -o /tmp/t && /tmp/t   # 네이티브 단위 테스트
```

---
© 2026 DeskStream Project. — worynim@gmail.com
