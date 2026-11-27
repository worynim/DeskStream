[English](GUIDE.md) | **한국어**

# 📘 DeskStream 개발·배포 가이드

[README](README.ko.md)에서 다 담지 못한 자세한 내용입니다: 공통 라이브러리 설정, 웹 플래셔·런처, 펌웨어 배포 절차.

## 🏗️ 공통 설계

- **하이브리드 I2C 아키텍처**: 하드웨어 I2C와 레지스터 직접 제어 기반 고속 소프트웨어 I2C를 병렬로 사용하여 4개의 화면을 끊김 없이 제어합니다.
- **성능 최적화**: 부드러운 애니메이션 구현을 위한 Dirty Page 체크 및 그래픽 가속 기법이 적용되었습니다.
- **문서화 표준화**: 전 프로젝트 소스 코드에 Doxygen 스타일 헤더 및 작성자 정보를 도입하여 유지보수성을 극대화했습니다.

## 🧩 공통 라이브러리 (DeskStreamCore)

여러 프로젝트가 함께 쓰는 기능(화면 엔진, 버튼, 부저, WiFi, NTP, 설정 저장, 웹 설정 엔진)을
Arduino 라이브러리 [`libraries/DeskStreamCore`](./libraries/DeskStreamCore/README.ko.md)로 모으고 있습니다.
**16개 프로젝트 전부**가 이 라이브러리를 씁니다 (화면 전송 HW 1.1MHz + 고속 SW I2C, 바뀐 타일만 병렬 전송 — 한글 시계 실측 4화면 45.7fps).

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

**https://worynim.github.io/DeskStream/** — Chrome/Edge 데스크톱 또는 Android Chrome에서 USB로 연결한 ESP32-C3 보드에 16개 프로젝트를 브라우저에서 바로 플래시할 수 있습니다. 별도 프로그램 설치가 필요 없습니다.

- **Web Serial + [esptool-js](https://github.com/espressif/esptool-js)** 기반. Chrome/Edge 데스크톱, **Android Chrome 148+ (USB OTG)** 지원. iPhone/iPad(Safari)·Firefox Android·인앱 브라우저는 불가
- **한국어 / English UI** 지원 — 페이지 우측 상단에서 전환 (브라우저 언어 자동 감지, 선택값 저장)
- 각 프로젝트 카드의 **README ↗** 링크에서 해당 프로젝트 문서를 새 탭으로 열 수 있습니다.
- 기본 플래시는 **NVS(WiFi·표시 설정) 보존**, 필요 시 "전체 지우기" 옵션 제공
- **런처(esp-launcher)** 항목: 보드 화면에서 펌웨어를 골라 Wi-Fi로 설치하는 런처를 설치합니다 ([아래](#-런처-esp-launcher))
- 펌웨어 **SHA-256**(다운로드) 및 **MD5**(쓰기 후) 무결성 검증, 플래시 후 자동 재부팅
- 16개 프로젝트(웹 플래셔 순서): 한글/영어/중국어 시계, 세계 시계, 범용 타이머, 스마트 정보 스테이션, GPS 대시보드, 무선 환경 분석기, 시스템 상태 모니터, 화면 스트리머, AI 에이전트 모니터, DS DECK, 페이지 터너, 사운드 비주얼라이저, 게임 오브 라이프, 초광폭 OLED 테스트
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

### 🚀 런처 (esp-launcher)

PC 없이 보드에서 프로젝트를 바꿀 수 있게 해 주는 런처입니다. 런처는 factory 영역에, 고른 프로젝트는 앱 영역(ota_0)에 들어갑니다.

1. 웹 플래셔에서 **런처 (esp-launcher)** 를 골라 한 번 굽습니다. 지금 앱은 지워지고 Wi-Fi 설정·글꼴(LittleFS)은 남습니다.
2. 런처 화면의 **Wi-Fi** 에서 폰으로 QR을 찍어 설정 포털에 접속하고 AP를 저장합니다 (앱과 같은 저장소라 앱에서도 그대로 씁니다).
3. **Firmware** 에서 목록([`firmware/manifest.launcher.json`](./firmware/manifest.launcher.json))을 받아 프로젝트를 골라 설치하면 그 앱으로 재부팅합니다.
4. 런처로 돌아가려면 USB를 뺀 뒤 **BTN1을 누른 채 USB를 연결**하고 BTN1을 **2초** 더 누르고 있습니다. (케이스에 넣으면 RESET 버튼을 누를 수 없으므로 USB 연결로 전원을 넣습니다.)
5. 런처가 새로 나오면 런처의 **Info → Update** 로 기기에서 바로 업데이트합니다 (설치된 앱은 지워지므로 다시 설치). 런처 0.1.x는 Firmware 목록의 "Launcher"를 설치하면 됩니다.

- 런처가 설치된 보드에 웹 플래셔로 다른 프로젝트를 구우면, 플래셔가 보드의 파티션 테이블을 읽어 **런처는 남기고 앱만** ota_0에 설치합니다.
- 런처를 없애려면 **전체 지우기**를 켜고 아무 프로젝트나 구우면 됩니다 (예전 방식으로 4개 이미지를 모두 씁니다).
- 앱 코드에서 런처로 돌아가려면 DeskStreamCore 0.7.0의 `ds::returnToLauncher()`를 부릅니다.
- 런처 소스는 별도 저장소 `esp-launcher`에 있습니다. 배포본은 `tools/mkbundle.py deskstream_c3 <DeskStream>/firmware/Launcher` 로 내보낸 번들입니다.

### 관련 파일 및 링크

| 항목 | 링크 |
|---|---|
| 웹 플래셔 앱 (Vite + TypeScript + esptool-js) | [`web/`](./web/) |
| **웹 플래셔 릴리즈 노트** | [`web/RELEASE_NOTES.md`](./web/RELEASE_NOTES.md) |
| 펌웨어 수집·manifest 생성 스크립트 | [`tools/collect-firmware.mjs`](./tools/collect-firmware.mjs) |
| 프로젝트 표시 메타데이터 (이름·버전·아이콘·설명) | [`firmware/projects.json`](./firmware/projects.json) |
| 배포된 펌웨어 목록·오프셋·SHA-256 | [`firmware/manifest.json`](./firmware/manifest.json) |
| 런처가 기기에서 읽는 펌웨어 목록 (자동 생성) | [`firmware/manifest.launcher.json`](./firmware/manifest.launcher.json) |
| 웹 플래셔에 표시할 PC 클라이언트 목록 | [`firmware/clients.json`](./firmware/clients.json) |
| PC 클라이언트 빌드·Release 워크플로 | [`.github/workflows/build-clients.yml`](./.github/workflows/build-clients.yml) |
| GitHub Pages 배포 워크플로 | [`.github/workflows/deploy-web.yml`](./.github/workflows/deploy-web.yml) |

### 펌웨어 업데이트 방법

1. Arduino IDE에서 보드 선택: **ESP32C3 Super Mini** (`makergo_c3_supermini` 보드 패키지)
2. `Sketch → Export Compiled Binary` 실행
3. 저장소 루트에서 수집 스크립트 실행 → `firmware/<project>/`와 `firmware/manifest.json`·`manifest.launcher.json` 갱신

   ```bash
   node tools/collect-firmware.mjs
   ```

4. `firmware/` 변경분을 커밋해 `main`에 push → GitHub Actions가 웹 플래셔를 자동 빌드·배포

> 웹 플래셔에 프로젝트를 추가/제외하려면 [`firmware/projects.json`](./firmware/projects.json)을 수정한 뒤 `node tools/collect-firmware.mjs`를 다시 실행하세요.
> 로컬 미리보기: `cd web && npm install && npm run dev` → http://localhost:5173/DeskStream/

### 최초 1회 설정

저장소 **Settings → Pages → Build and deployment → Source**를 **GitHub Actions**로 지정합니다. 이후 `.github/workflows/deploy-web.yml`이 `main` push 시 자동 배포합니다.

