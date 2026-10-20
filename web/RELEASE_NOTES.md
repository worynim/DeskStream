# Release Notes - DeskStream Web Flasher

ESP32-C3 DeskStream 보드용 웹 플래셔 **https://worynim.github.io/DeskStream/** 의 변경 이력입니다.

- 웹 앱: `web/` (Vite + TypeScript + esptool-js)
- 펌웨어 소스: `firmware/manifest.json`
- 배포: `main` push 시 `.github/workflows/deploy-web.yml`이 자동 빌드·배포
- 검증: `node tools/collect-firmware.mjs --check`

---

## [v1.6.0] - 2026-10-21

### ✨ Android WebUSB(CDC-ACM) 우회 플래시 모드

- Android Chrome에서 **Web Serial 포트 목록에 USB 장치(ESP32-C3)가 보이지 않는 경우**를 위한 WebUSB 우회 경로를 추가했습니다.
- 플래시 옵션에 **`USB(WebUSB) 모드`** 체크박스를 추가했습니다.
  - `web-serial-polyfill`로 WebUSB 장치를 SerialPort처럼 사용합니다.
  - 선택 시 `navigator.usb.requestDevice()`로 ESP32-C3(CDC-ACM)에 직접 연결합니다.
  - WebUSB를 사용할 수 있는 브라우저에서만 옵션이 노출됩니다.
  - Web Serial API 자체가 없는 브라우저에서는 해당 모드가 기본 활성화됩니다.
- WebUSB 모드 안내 문구를 한국어/영어로 추가하고 사용 안내(guide)를 갱신했습니다.
- README에 Android WebUSB 우회 모드 안내를 추가했습니다.
- 커밋: `cd690d3`

### 참고

- WebUSB 모드는 **실험적 기능**입니다. Android 커널/드라이버가 CDC 인터페이스를 선점한 기기에서는 `Unable to claim interface` 오류가 날 수 있습니다.
- 데스크톱에서는 기존 Web Serial 방식이 기본이며, WebUSB 모드를 체크하지 않으면 기존과 동일하게 동작합니다.

## [v1.5.0] - 2026-10-21

### 📱 스마트폰(Android) 지원 범위 반영

- Android Chrome 148+에서 Web Serial(USB·Bluetooth)이 지원됨에 따라 안내 문구를 실제 지원 범위로 수정했습니다.
  - 지원: Chrome/Edge 데스크톱, Android Chrome 148+ (USB OTG 어댑터 필요)
  - 미지원: iPhone/iPad(Safari), Firefox Android, 인앱 브라우저(카카오톡·네이버 등)
- `isSerialSupported()`를 `navigator.serial.requestPort` 함수 존재까지 확인하도록 보강하여, API가 없는 브라우저에서 잘못 활성화되지 않도록 했습니다.
- README에 **“스마트폰에서 사용하기 (Android)”** 섹션을 추가했습니다.
- 커밋: `93845a7`

## [v1.4.0] - 2026-10-21

### 🔗 프로젝트 카드 README 링크

- 각 프로젝트 카드 우측 하단에 **`README ↗`** 링크를 추가했습니다. (GitHub README, 새 탭)
- 카드 선택(button)과 README 링크(a)를 분리해 클릭 충돌을 제거했습니다.
  - 카드는 `aria-pressed` 토글 버튼으로 정리
  - README 링크에 `target="_blank"` / `rel="noopener noreferrer"` 적용
- 언어 전환 시 링크 title/aria-label도 한국어/영어로 변경됩니다.
- `web/src/config.ts`에서 저장소 URL과 README URL을 관리합니다.
- 커밋: `f8bfbd7`

## [v1.3.0] - 2026-10-21

### 🌐 영어 UI 및 한국어/English 언어 전환

- 페이지 우측 상단에 **한국어 / English** 언어 스위치를 추가했습니다.
  - 선택값은 `localStorage`에 저장되어 새로고침·재방문 시 유지
  - 저장값이 없으면 브라우저 언어 자동 감지 (`ko-*` → 한국어, 그 외 → English)
- 전체 UI 문자열을 `web/src/i18n.ts`로 분리했습니다.
  - 헤더·안내·가이드·옵션·버튼·상태·에러·진행률·로그 메시지
  - 날짜·프로젝트 수 포맷
- 프로젝트 이름·설명·안내문의 영어 번역을 `firmware/projects.json`의 `nameEn` / `descriptionEn` / `noteEn`으로 추가하고 manifest에 포함했습니다.
  - 영어 필드가 없으면 한국어로 자동 폴백
- 플래시 진행/로그 메시지도 현재 언어로 표시됩니다.
- 커밋: `8cbfefa`

## [v1.2.1] - 2026-10-21

### ℹ️ 시스템 상태 모니터 호스트 앱 안내

- 시스템 상태 모니터 선택 시 화면 스트리머와 동일한 형태로 **PC 호스트 앱 안내**를 표시합니다.
  - “PC에서 `client_py/main.py`(호스트 앱)와 함께 사용합니다. BLE로 연결되며, 화면 회전·밝기 설정은 NVS에 저장됩니다.”
- 커밋: `025c4c2`

## [v1.2.0] - 2026-10-21

### 🧹 프로젝트 정리 및 README 보강

- **한글 시계 BB(`Hangeul_Clock_bitbang_i2c`) 웹 플래셔 제외**
  - 1.3" SH1106 · 4-bus 회로 구성이 달라 플래셔 대상에서 제외
  - `firmware/projects.json`, 펌웨어 바이너리, `firmware/manifest.json`에서 제거
- 시스템 상태 모니터의 `EraseFlash=all` 안내 문구를 삭제하고 최신 빌드(`EraseFlash=none`)를 반영했습니다.
- 플래셔 대상 프로젝트를 **11개**로 정리했습니다.
- 루트 README에 웹 플래셔 상단 링크, 관련 파일 링크 표를 보강했습니다.
- 커밋: `c01dc62`

## [v1.1.0] - 2026-10-21

### 🔁 플래시 후 자동 재부팅 수정

- esptool-js 0.7.0의 `after("hard_reset")`이 ESP32-C3의 USB-Serial/JTAG에서 동작하지 않아, 플래시 후 칩이 stub에 머물던 문제를 수정했습니다.
- esptool(Python)의 `--after watchdog-reset`과 동일한 **RTC WDT 리셋**을 적용했습니다.
  - ESP32-C3/C2/S2/S3: RTC WDT 리셋
  - 그 외 Espressif 내장 USB(VID `0x303A`): USB-JTAG 리셋 시퀀스
  - UART 브리지: 클래식 EN 펄스
- 성공 안내에 “자동으로 재부팅되지 않으면 RESET 버튼” 안내를 추가했습니다.
- 커밋: `25bd4ce`

## [v1.0.1] - 2026-10-21

### 🛠️ GitHub Actions 빌드 수정

- 클린 환경에서 `vite.config.ts`의 `process` 타입을 찾지 못해 `tsc`가 실패하던 문제를 수정했습니다.
  - `@types/node` devDependency 추가
  - 앱용 `tsconfig.json`과 `vite.config.ts`용 `tsconfig.node.json` 분리
- 커밋: `cb16cd1`

## [v1.0.0] - 2026-10-21

### 🚀 최초 공개

- Vite + TypeScript + esptool-js 0.7 기반 웹 플래셔와 **GitHub Pages 자동 배포**를 추가했습니다.
- 12개 프로젝트 펌웨어 선택·플래시
  - 표준 4파일 업로드 (bootloader `0x0` / partitions `0x8000` / boot_app0 `0xe000` / app `0x10000`)
  - 기본은 NVS(WiFi·설정) 보존, **전체 지우기** 옵션 제공
- 펌웨어 **SHA-256**(다운로드) + **MD5**(쓰기 후) 무결성 검증
- 진행률·로그·에러 안내, BOOT+RESET 가이드, 지원 브라우저 감지
- `tools/collect-firmware.mjs`: Arduino IDE export(`flash_args`)에서 필요한 `.bin`만 수집하고 `firmware/manifest.json` 생성·검증
- `.gitignore`: 업로드용 `firmware/**/*.bin`만 추적, `build/`의 `.elf`/`.map` 등 대용량 산출물은 계속 무시
- 커밋: `1adbc98`, `41c6a5d`

---

## v1.6.0 배포 시점 포함 펌웨어

| 프로젝트 | 버전 |
|---|---|
| 스마트 정보 스테이션 | 2.9.1 |
| GPS 대시보드 | 2.6.1 |
| 사운드 비주얼라이저 | 3.4.1 |
| 초광폭 OLED 테스트 | 1.2.1 |
| 시스템 상태 모니터 | 1.5.1 |
| DS DECK 매크로 패드 | 1.5.1 |
| 화면 스트리머 (수신 펌웨어) | 2.2.1 |
| 게임 오브 라이프 | 1.0.2 |
| 한글 시계 | 2.9.5 |
| 영어 시계 | 1.0.4 |
| 중국어 시계 | 1.0.2 |

> 펌웨어 목록과 버전은 `firmware/manifest.json`을 기준으로 하며, Arduino IDE에서 다시 export한 뒤 `node tools/collect-firmware.mjs`를 실행하면 갱신됩니다.

## 알려진 제한사항

- **Web Serial**: Chrome/Edge 데스크톱, Android Chrome 148+ (USB OTG 어댑터 필요). iPhone/iPad(Safari)·Firefox Android·인앱 브라우저는 지원하지 않습니다.
- **Android USB 장치가 Web Serial 목록에 안 보이는 경우**: `USB(WebUSB) 모드`를 사용해 보세요. (v1.6.0, 실험적)
- **GitHub Pages 캐시**: 페이지가 최대 10분간 캐시될 수 있습니다. 변경 직후에는 강력 새로고침(Cmd/Ctrl+Shift+R)을 사용하세요.
- `Smart_Info_Station`·`DS_DECK`은 `huge_app` 파티션(OTA 없음) 빌드입니다.
