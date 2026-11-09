# 🖥️ DeskStream: ESP32-C3 4-OLED Multifunctional Board

DeskStream은 ESP32-C3와 4개의 OLED 디스플레이를 활용한 다기능 데스크톱 정보 스테이션 프로젝트입니다.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

> 🌐 **웹 플래셔**: [https://worynim.github.io/DeskStream/](https://worynim.github.io/DeskStream/) — USB로 연결한 ESP32-C3 보드에 원하는 프로젝트를 브라우저에서 바로 업로드할 수 있습니다. ([사용법](#-웹-플래셔-web-flasher) · [소스](./web/))

## 🛠️ 하드웨어 사양 (Hardware Specification)
- **MCU**: ESP32-C3 (SuperMini)
- **Display**: 0.96" SSD1306 OLED (128x64) x 4개 (가로 배치 시 512x64 광폭 디스플레이 구현 가능)
- **Buttons**: 4개의 사용자 입력 버튼 (GPIO 1, 4, 10, 9)
- **Interface**:
  - **HW I2C**: SDA(GPIO 5), SCL(GPIO 6) - OLED 1, 2 제어
  - **SW I2C**: SDA(GPIO 2), SCL(GPIO 3) - OLED 3, 4 제어
- 상세 정보: [hardware.md](./hardware.md) 참조

## 📁 주요 프로젝트 (Sub-Projects)
본 저장소는 DeskStream 보드용으로 개발된 13가지 주요 프로그램을 포함하고 있습니다.

### 1. [Smart_Info_Station: 스마트 정보 스테이션 (v2.10.1)](./Smart_Info_Station/)
- **기능**: 시계, 날씨, 미세먼지, 유튜브 구독자 수, 증시 지수, 가상화폐 가격, 환율 실시간 모니터링.
- **특징**: **전면 모듈화 아키텍처**, 웹 대시보드 실시간 설정, 비동기 데이터 갱신(FreeRTOS), 네이티브 HAL 기반 고속 SW I2C.
- **버튼 동작**: 버튼 1을 사용하여 HUD 반전 모드 순환, 버튼 4를 사용하여 페이지를 전환합니다.

### 2. [GPS_DASH: 하드웨어 GPS 대시보드](./GPS_DASH/)
- **기능**: 실시간 속도, 나침반, 고도, 위도/경도, 위성 상태, 주행 통계 표시.
- **특징**: NEO-6M GPS 모듈 연동(5Hz), UBX 프로토콜 직접 제어, 8가지 디스플레이 모드 제공.

### 3. [Sound_Visualizer_4_OLEDs: 고속 오디오 비주얼라이저](./Sound_Visualizer_4_OLEDs/)
- **기능**: 아날로그 마이크 입력(G0)을 통한 128밴드 실시간 주파수 시각화.
- **특징**: ESP-DSP 정수형 FFT 가속(Max 70+ FPS), 512x64 초광폭 캔버스 제어, ADC DMA 샘플링.

### 4. [OLED_Wide_test: 초광폭 디스플레이 테스트](./OLED_Wide_test/)
- **기능**: 4개의 OLED를 하나의 512x64 화면으로 제어하는 데모 및 테스트 코드.
- **특징**: Shadow Buffering을 통한 데이터 전송 최적화, LovyanGFX 기반 고속 그래픽 렌더링.

### 5. [Stats_Monitor: 시스템 상태 모니터](./Stats_Monitor/)
- **기능**: PC/Mac의 CPU, GPU, RAM, SSD 상태 및 온도, 네트워크 트래픽 실시간 표시.
- **특징**: 외부 클라이언트 연동, 하드웨어 통계 가젯 모드, 시스템 생체 정보 가시화.

### 6. [DS_DECK: 프로그래머블 매크로 패드 (v1.6.1)](./DS_DECK/)
- **기능**: 4개의 버튼에 텍스트 매크로(한/영 지원), 단축키(Combo), 미디어 컨트롤, **웹 브라우저 런처(Browser)** 할당.
- **특징**: **macOS 프리미엄 런처 지원**(Spotlight 자동화), 초고속 BLE HID 엔진, 웹 기반 실시간 매크로 설정, F1~F12 포함 모든 특수키 지원.

### 7. [Screen_To_OLEDs: 실시간 화면 스트리머 (v2.3.1)](./Screen_To_OLEDs/)
- **기능**: Mac의 화면 일부를 실시간 캡처하여 DeskStream 4-OLED 매트릭스(512x64)에 무선 전송.
- **특징**: 지능형 캡처 엔진(Full 대응), Numba JIT 가속, 6종 디더링 알고리즘, 실시간 설정 변경.
- **버튼 동작**: 버튼 2를 눌러 밝기를 조절하고, 버튼 1을 롱프레스하여 화면을 180도 회전(플립)합니다.

### 8. [Hangeul_Clock: 고화질 폰트 한글 시계 (v2.11.1)](./Hangeul_Clock/)
- **기능**: NTP 동기화 기반의 고화질 한글 시계 및 하이엔드 폰트 렌더링 시스템.
- **특징**: 
    - **제로-스터터(Zero-Stutter) 웹 API**: 폰트 슬롯 이름 메모리 캐싱으로 웹 대시보드 호출 시 시스템 멈춤 현상 근본 해결.
    - **전문가급 안정성 및 보안**: **하드웨어 자가 치유(I2C Recovery)**, **API 입력 범위 검증(Whitelist)** 로직 탑재.
    - **Flat Buffer 메모리 무결성**: 힙 파편화와 메모리 누수를 원천 차단하는 **Flat Buffer** 전략 사용.
    - **비차단(Non-blocking) 엔진**: 상태 머신 기반 애니메이션 루프 및 `refreshNow()` 잔상 방지 로직.
    - **웹 미리보기 동기화**: '색상 반전' 설정이 웹 대시보드 캔버스 미리보기에도 실시간 반영.
    - **64px Modern Font Engine**: 글자 잘림 없는 고해상도 64px(512B) 비트맵 폰트 정식 지원.
    - **지능형 24H 및 날짜 시스템**: 24시간제 지원.
    - **인터랙티브 제어**: 4개의 물리 버튼을 통해 **시보, 화면 반전, 한글/숫자 모드, 시간 형식(12/24H)**을 즉각적으로 변경 가능.
    - **Ecosystem 연동**: 실시간 웹 시뮬레이너(폰트 스튜디오) 및 3초 비동기 동기화 엔진 탑재.
 
### 9. [Game_Of_Life: 단독 구동형 생명 게임](./Game_Of_Life/)
- **기능**: Conway의 생명 게임(Game of Life) 시뮬레이션을 512x64 대화면에서 무한 루프로 실행.
- **특징**: 1-bit 메모리 최적화(총 8KB 사용), 4개 버튼을 통한 **재생/일시정지, 수동 단계 진행, 속도 조절(5/10/20 FPS), 패턴 리셋** 지원.

### 10. [ENG_Clock: 고화질 폰트 영어 시계 (v1.1.1)](./ENG_Clock/)
- **기능**: NTP 동기화 기반의 고화질 영어 단어 시계. `Hangeul_Clock`의 아키텍처와 품질을 유지하며 텍스트만 영어로 포트.
- **특징**: 
    - **2-Line Word Layout**: 영어 최장 단어(`SEVENTEEN`, 9자)를 수용하는 어절 단위 2줄 레이아웃.
    - **POS TZ 시간대 시스템**: DST 자동 처리, 웹 UI에서 12개 지역 프리셋 + Custom 입력.
    - **Font Studio**: 브라우저에서 TTF를 잉크 중앙 정렬 64px 비트맵으로 래스터화해 직접 업로드.
    - **호스트 테스트 인프라**: 네이티브 C++ 단위 테스트 + 웹↔펌웨어 교차 검증(`bash test/js/run_all.sh`).

### 11. [Chinese_Clock: 고화질 폰트 중국어 시계 (v1.1.1)](./Chinese_Clock/)
- **기능**: NTP 동기화 기반의 중국어 한자 단어 시계. `Hangeul_Clock` / `ENG_Clock`의 하드웨어·UI·애니메이션·웹 파이프라인을 승계하고 텍스트 생성부만 중국어로 포트.
- **특징**: 
    - **4화면 32×32 셀 레이아웃**: 최장 표현이 4자(`四十五分`)라 줄바꿈 없이 화면당 정사각형 셀 4개에 표시.
    - **표준형 한자 표현**: `两点`, `三点零五分`, `三点半`, `九点整`, `上午`/`下午`, `星期日`~`星期六` 등 표준 규칙 적용.
    - **간체 / 번체 / 숫자 표시 순환**: `简体 → 繁體 → 數字` (차이 3글자를 치환표 하나로 처리).
    - **POSIX TZ 시간대 시스템**: DST 자동 처리, 기본값 `CST-8`, 웹 UI 프리셋 + Custom 입력.
    - **Font Studio**: 브라우저에서 TTF를 37자(간체 ∪ 번체) 48×48 비트맵으로 구워 5개 폰트 슬롯에 업로드.

### 12. [Universal_Timer: 4-OLED 범용 타이머 (v1.1.1)](./Universal_Timer/)
- **기능**: 4개의 OLED에 시·분·초를 각각 표시 및 조절하고 카운트다운을 수행하는 하드웨어 타이머.
- **특징**: 
    - **4분할 인터랙티브 레이아웃**: OLED 1(시 / 00H), OLED 2(분 / 00M), OLED 3(초 / 00S), OLED 4(START / PAUSE / RESET).
    - **자동 가속 반복 (Auto-Repeat)**: 버튼을 길게 누르면 60ms 간격으로 고속 연속 증가.
    - **타임아웃 자동 복원 메모리**: 카운트다운 종료 알람 시 아무 버튼이나 누르면 직전에 설정했던 시간으로 자동 복원.
    - **이벤트 기반 더티 체킹**: 변경된 화면만 선별 전송하여 지연 없는 즉각적 반응성 및 정확한 30ms 비프음 제공.

### 13. [Agent_Monitor: AI 개발 에이전트 모니터 (v0.4.0)](./Agent_Monitor/)
- **기능**: Claude Code 세션 상태(생각 중 / 실행 중 / 승인 대기 / 입력 대기 / 완료 / 오류)를 OLED 한 장에 세션 하나씩 실시간 표시하고 부저로 알림.
- **특징**: Claude Code 훅(CLI·데스크톱 앱) → PC 데몬(Python, 트레이 아이콘) → USB 시리얼 또는 BLE(USB 로 등록한 키로 서명). 데몬이 꺼져 있어도 Claude Code 동작에 영향 없음. 권한 요청을 장치 버튼으로 승인/거절(터미널 대화상자와 동시 대기, 자동 허용 없음). 세션 상세 화면, 음소거·무음 시간대, 버튼 길게 누르기로 PC 명령(앱 앞으로·셸 명령·키 입력) 실행.

## 🚀 주요 기술적 특징
- **하이브리드 I2C 아키텍처**: 하드웨어 I2C와 레지스터 직접 제어 기반 고속 소프트웨어 I2C를 병렬로 사용하여 4개의 화면을 끊김 없이 제어합니다.
- **성능 최적화**: 부드러운 애니메이션 구현을 위한 Dirty Page 체크 및 그래픽 가속 기법이 적용되었습니다.
- **문서화 표준화**: 전 프로젝트 소스 코드에 Doxygen 스타일 헤더 및 작성자 정보를 도입하여 유지보수성을 극대화했습니다.

## 🧩 공통 라이브러리 (DeskStreamCore)

여러 프로젝트가 함께 쓰는 기능(화면 엔진, 버튼, 부저, WiFi, NTP, 설정 저장, 웹 설정 엔진)을
Arduino 라이브러리 [`libraries/DeskStreamCore`](./libraries/DeskStreamCore/)로 모으고 있습니다.
**13개 프로젝트 전부**가 이 라이브러리를 씁니다 (화면 전송 HW 1.1MHz + 고속 SW I2C, 바뀐 타일만 병렬 전송 — 한글 시계 실측 4화면 45.7fps).

### 최초 설정 (PC마다 1회)

Arduino IDE는 `#include <DeskStreamCore.h>`를 **스케치 폴더**와 **스케치북의 `libraries/` 폴더**에서만 찾습니다.
라이브러리 원본은 저장소의 `libraries/DeskStreamCore/`에 있으므로, 스케치북 `libraries/` 안에 이 폴더를 가리키는
**링크(바로가기)**를 한 번 만들어 둡니다. 복사가 아니라 링크라서 저장소에서 라이브러리를 고치거나 `git pull`하면
모든 스케치에 바로 반영됩니다.

```
<스케치북>/libraries/DeskStreamCore   ──(링크)──▶   DeskStream/libraries/DeskStreamCore
```

**1. 스케치북 위치 확인** — Arduino IDE **Settings(환경설정) → Sketchbook location**.
기본값은 macOS `~/Documents/Arduino`, Windows `C:\Users\<사용자>\Documents\Arduino` 입니다.

**2-A. macOS / Linux** — 저장소 루트(DeskStream 폴더)에서 실행합니다.

```bash
bash scripts/link-arduino-lib.sh
```

- 스케치북 경로는 인자 → `arduino-cli config get directories.user` → `~/Documents/Arduino` 순으로 찾습니다.
  기본 위치가 아니면 인자로 넘깁니다: `bash scripts/link-arduino-lib.sh "/경로/스케치북"`
- 성공하면 `✅ 연결 완료`, 이미 연결돼 있으면 `✅ 이미 연결됨`이 나옵니다 (여러 번 실행해도 안전).
- 저장소를 옮겨 링크가 다른 곳을 가리키면 새 위치로 교체합니다. 링크가 아닌 **진짜 폴더**가 이미 있으면
  덮어쓰지 않고 멈추므로, 내용을 확인해 옮기거나 지운 뒤 다시 실행합니다.

**2-B. Windows** — `scripts/link-arduino-lib.sh`는 bash용이므로 **명령 프롬프트(cmd)**에서 디렉터리 정션을 만듭니다
(관리자 권한 불필요). 아래 두 경로를 자기 PC에 맞게 바꿉니다.

```bat
mkdir "%USERPROFILE%\Documents\Arduino\libraries"
mklink /J "%USERPROFILE%\Documents\Arduino\libraries\DeskStreamCore" "C:\경로\DeskStream\libraries\DeskStreamCore"
```

- 첫 줄은 `libraries` 폴더가 없을 때만 필요합니다 (이미 있으면 "이미 있습니다" 메시지가 나와도 괜찮습니다).
- 성공하면 `...에 대한 교차점을 만들었습니다` (영문: `Junction created for ...`)가 나옵니다.
- PowerShell을 쓴다면: `New-Item -ItemType Junction -Path "$env:USERPROFILE\Documents\Arduino\libraries\DeskStreamCore" -Target "C:\경로\DeskStream\libraries\DeskStreamCore"`
- Git Bash의 `ln -s`는 설정에 따라 링크 대신 **복사**를 만들 수 있으므로 쓰지 않습니다.
- 저장소를 옮긴 경우: `rmdir "%USERPROFILE%\Documents\Arduino\libraries\DeskStreamCore"`로 정션만 지우고
  (`rmdir`은 정션만 지우고 원본 폴더는 건드리지 않습니다) 다시 만듭니다.

**3. 외부 라이브러리 설치** — Arduino IDE 라이브러리 매니저에서 **U8g2**를 설치합니다 (모든 프로젝트).
WiFi 설정 포털은 DeskStreamCore에 들어 있어 WiFiManager는 필요 없습니다 (v0.6.0~).
캔버스 프로젝트(Sound_Visualizer·Screen_To_OLEDs·OLED_Wide_test)는 **LovyanGFX**도 필요합니다.

**4. 확인** — Arduino IDE를 **완전히 껐다 켠 뒤**(시작할 때 라이브러리 목록을 읽습니다) 아무 프로젝트(예: `Game_Of_Life`)를
열어 **Verify(✓)**합니다. `DeskStreamCore.h: No such file or directory`가 나오면 링크가 1번의 스케치북 위치에 있는지 확인합니다.

- 터미널 확인: macOS `ls -l ~/Documents/Arduino/libraries | grep DeskStreamCore` (화살표 `->`가 보이면 정상),
  Windows `dir "%USERPROFILE%\Documents\Arduino\libraries"` (`<JUNCTION>`으로 표시되면 정상)
- 링크는 PC의 실제 경로를 가리키므로 **PC마다** 한 번씩 만듭니다 (저장소가 Google Drive 등으로 동기화돼도 링크는 따라가지 않습니다).
- 시계 3종을 빌드·Export할 때는 `Partition Scheme = Huge APP`도 골라야 합니다 (각 프로젝트 README 참조).
- 라이브러리 호스트 테스트: `bash libraries/DeskStreamCore/test/run_all.sh`
- **AI로 새 프로젝트를 만들 때**: [`AI_GUIDE.md`](./AI_GUIDE.md)를 먼저 읽게 하세요 — 라이브러리 사용 규칙·모듈별 API·템플릿·체크리스트.

## 🌐 웹 플래셔 (Web Flasher)

**https://worynim.github.io/DeskStream/** — Chrome/Edge 데스크톱 또는 Android Chrome에서 USB로 연결한 ESP32-C3 보드에 13개 프로젝트를 브라우저에서 바로 플래시할 수 있습니다. 별도 프로그램 설치가 필요 없습니다.

- **Web Serial + [esptool-js](https://github.com/espressif/esptool-js)** 기반. Chrome/Edge 데스크톱, **Android Chrome 148+ (USB OTG)** 지원. iPhone/iPad(Safari)·Firefox Android·인앱 브라우저는 불가
- **한국어 / English UI** 지원 — 페이지 우측 상단에서 전환 (브라우저 언어 자동 감지, 선택값 저장)
- 각 프로젝트 카드의 **README ↗** 링크에서 해당 프로젝트 문서를 새 탭으로 열 수 있습니다.
- 기본 플래시는 **NVS(WiFi·표시 설정) 보존**, 필요 시 "전체 지우기" 옵션 제공
- 펌웨어 **SHA-256**(다운로드) 및 **MD5**(쓰기 후) 무결성 검증, 플래시 후 자동 재부팅
- 13개 프로젝트: 스마트 정보 스테이션, GPS 대시보드, 사운드 비주얼라이저, 초광폭 OLED 테스트, 시스템 상태 모니터, DS DECK, 화면 스트리머, 게임 오브 라이프, 한글/영어/중국어 시계, 범용 타이머, AI 에이전트 모니터
- **한글 시계 BB(Hangeul_Clock_bitbang_i2c)는 1.3" SH1106 · 4-bus 회로 구성이 달라 웹 플래셔 대상에서 제외**됩니다.
  하드웨어가 달라 2026-11-08부터 별도 폴더 **DeskStream_V2**에서 관리합니다.

### 스마트폰에서 사용하기 (Android)

Android Chrome 148 이상 + **USB OTG 어댑터**(USB 데이터 케이블)가 있으면 스마트폰에서도 플래시할 수 있습니다.

1. Chrome을 최신 버전(148+)으로 업데이트합니다.
2. USB-C OTG 어댑터에 USB 데이터 케이블을 연결하고 ESP32-C3 보드를 연결합니다.
3. 카카오톡·네이버 등 **인앱 브라우저가 아닌 Chrome**에서 주소를 직접 입력해 엽니다.
4. 프로젝트를 선택하고 **플래시 시작** → 포트 선택 창에서 보드를 고릅니다.

> USB 장치가 Web Serial 포트 목록에 보이지 않으면, 플래시 옵션의 **USB(WebUSB) 모드**를 체크하고 다시 시도해 보세요. WebUSB(CDC-ACM)로 직접 접근하는 실험적 우회 모드입니다.
>
> iPhone/iPad는 Safari(WebKit)가 Web Serial API를 지원하지 않아 브라우저 플래시가 불가능합니다. PC의 Chrome/Edge를 사용해 주세요.

### 관련 파일 및 링크

| 항목 | 링크 |
|---|---|
| 웹 플래셔 앱 (Vite + TypeScript + esptool-js) | [`web/`](./web/) |
| **웹 플래셔 릴리즈 노트** | [`web/RELEASE_NOTES.md`](./web/RELEASE_NOTES.md) |
| 펌웨어 수집·manifest 생성 스크립트 | [`tools/collect-firmware.mjs`](./tools/collect-firmware.mjs) |
| 프로젝트 표시 메타데이터 (이름·버전·아이콘·설명) | [`firmware/projects.json`](./firmware/projects.json) |
| 배포된 펌웨어 목록·오프셋·SHA-256 | [`firmware/manifest.json`](./firmware/manifest.json) |
| GitHub Pages 배포 워크플로 | [`.github/workflows/deploy-web.yml`](./.github/workflows/deploy-web.yml) |

### 펌웨어 업데이트 방법

1. Arduino IDE에서 보드 선택: **ESP32C3 Super Mini** (`makergo_c3_supermini` 보드 패키지)
2. `Sketch → Export Compiled Binary` 실행
3. 저장소 루트에서 수집 스크립트 실행 → `firmware/<project>/`와 `firmware/manifest.json` 갱신

   ```bash
   node tools/collect-firmware.mjs
   ```

4. `firmware/` 변경분을 커밋해 `main`에 push → GitHub Actions가 웹 플래셔를 자동 빌드·배포

> 웹 플래셔에 프로젝트를 추가/제외하려면 [`firmware/projects.json`](./firmware/projects.json)을 수정한 뒤 `node tools/collect-firmware.mjs`를 다시 실행하세요.
> 로컬 미리보기: `cd web && npm install && npm run dev` → http://localhost:5173/DeskStream/

### 최초 1회 설정

저장소 **Settings → Pages → Build and deployment → Source**를 **GitHub Actions**로 지정합니다. 이후 `.github/workflows/deploy-web.yml`이 `main` push 시 자동 배포합니다.

---

© 2026 DeskStream Project.
