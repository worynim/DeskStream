# DeskStreamCore Release Notes

## [v0.6.1] - 2026-11-10
### 🐛 `ds::utf8CharLen` — 남은 길이가 256 의 배수면 0 반환 (무한 루프)
- 남은 바이트 수 `remaining` 을 `(uint8_t)` 로 잘라 비교해서 **256·512·768… 이 남으면 0 을 반환**했다.
  함수 문서는 0 이 되지 않음을 보장했고 호출부가 `i += len` 으로 전진하므로, 그 지점에서 루프가 영원히 돌았다
  (Agent_Monitor 에서 340바이트 메시지의 256바이트 남은 지점에서 장치가 멈춤). `int` 로 비교하도록 고쳤다.
- 회귀 테스트 6건 추가 (`test_utf8_len` 35건 — 옛 코드에서는 5건 실패). 8종 전부 통과.

## [v0.6.0] - 2026-11-10
### 📶 WiFiManager 제거 — 자체 설정 포털
- `ds::WifiService` 포털을 WiFiManager 대신 직접 구현했다: 열린 AP(`apName`) + 캡티브 DNS(모든 도메인 → 192.168.4.1)
  + 설정 페이지(주변 SSID 목록 — 신호 세기 순·중복 제거, 직접 입력, 비밀번호) → 그 자리에서 연결 시도 →
  실패하면 이유(네트워크 없음 / 비밀번호·거부 / 시간 초과)를 페이지에 표시.
  - API(`WifiConfig`·`begin`·`loop`·`resetSettings`)는 그대로 — 스케치 변경 없음.
  - 자격증명은 `WiFi.begin(ssid, pass)`로 ESP32 NVS에 저장 — WiFiManager와 같은 곳이라 **기존 기기 설정 유지**.
  - `resetSettings()`는 `WiFi.disconnect(true, true)`로 저장된 AP를 지운다.
- **크기**: 웹서버를 쓰는 스케치 기준 WiFiManager 약 90KB → 자체 포털 약 18KB (**약 74KB 감소**).
  `library.properties`의 `depends`에서 WiFiManager를 뺐다 — 설치하지 않아도 빌드된다.
- 보안: 페이지에 찍히는 SSID(누구나 정할 수 있는 값)는 HTML 이스케이프한다. 설정 AP는 예전과 같이 **암호 없는 열린 AP**다.
- 순수 함수 `ds_wifi_portal_util`(SSID·비밀번호 검증, HTML 이스케이프) + 호스트 테스트 `test_wifi_portal` 20건 — 8종 통과.

## [v0.5.0] - 2026-11-09
### 🧩 512x64 캔버스 지원 (3단계)
- `display/ds_canvas.*`: `ds::canvasToScreenPages(canvas, width, column, out)` — 행 우선·MSB 먼저 1bpp 캔버스
  (LovyanGFX 1bit 스프라이트·Game_Of_Life 엔진 버퍼)를 화면별 SSD1306 페이지 버퍼로 전치하는 순수 함수.
- `DisplayCore::pushCanvas(canvas, reverseOrder)`: 4화면 전치 + `push()` (180° 장착 시 화면 순서 뒤집기 옵션).
- 호스트 테스트 `test_canvas`(7건, 무작위 패턴 전 픽셀 왕복 포함) — 7종 통과.
- 사용 중인 스케치: 12개 전부 (Sound_Visualizer_4_OLEDs v3.5.0, Screen_To_OLEDs v2.3.0, OLED_Wide_test v1.3.0 추가).

## [v0.4.0] - 2026-11-09
### 🧩 U8g2 프로젝트 6종 전환에 필요한 확장 (2단계)
- **버튼** (`ds_button_fsm.h` · `ds_buttons.*`):
  - 이벤트를 **비트마스크**로 돌려준다 (`BUTTON_SHORT | BUTTON_LONG | BUTTON_PRESS | BUTTON_REPEAT`).
    한 번의 update에서 LONG과 REPEAT가 함께 날 수 있다 — Universal_Timer에서 REPEAT가 LONG을 덮어쓰던 문제가 사라졌다.
  - `ButtonTiming`에 자동 반복(`repeatDelayMs`·`repeatIntervalMs`)과 안정 디바운스(`stableMs`) 추가. 둘 다 기본 0(끔) — 시계 동작은 그대로.
  - `BUTTON_PRESS`(누르는 순간) 이벤트, `setCallback(idx, ev, fn)`, 조회 API `events(idx)`·`isPressed(idx)`.
  - (호환 깨짐: `ButtonFsm::update()` 반환형 `ButtonEvent` → `uint8_t` 비트마스크)
- **부저**: `playMelody(notes, count, loop)` — `loop=true`면 `stop()`까지 반복(알람). `stop()`·`isLooping()` 공개.
- 호스트 테스트: `test_button_fsm` 30건(자동 반복·LONG|REPEAT 동시·안정 디바운스 추가).
- 사용 중인 스케치: 시계 3종 + Game_Of_Life v1.1.0, Universal_Timer v1.1.0, Stats_Monitor v1.6.0, GPS_DASH v2.7.0,
  DS_DECK v1.6.0, Smart_Info_Station v2.10.0.

## [v0.3.0] - 2026-11-09
### 🧩 ENG_Clock · Chinese_Clock 전환에 필요한 확장 (1단계 ②)
- `ConfigField` 플래그 `FIELD_NO_WEB`: NVS에만 저장하고 웹 API(GET/POST)에서 건너뛴다 — 웹이 여러 필드를
  묶은 계산 키로 다루는 경우(중국어 시계 `presentation` = display_mode + script_type).
- `DS_CFG_U8_F` 매크로 (플래그 지정 U8 필드).
- `WebConfigApi::AfterPost` 훅이 POST 본문(`const char*`)을 받는다 — 테이블 밖 키를 `jsonGetInt` 등으로 처리.
  (호환 깨짐: 훅 시그니처 `void()` → `void(const char*)`. Hangeul_Clock 반영)
- 사용 중인 스케치: Hangeul_Clock v2.11.0, ENG_Clock v1.1.0, Chinese_Clock v1.1.0.

## [v0.2.0] - 2026-11-09
### 🧩 서비스 모듈 추가 (1단계: Hangeul_Clock v2.10.0에서 추출)
- **화면 엔진** `ds::DisplayCore` (+ `ds::I2CPlatform`, 고속 SW I2C): U8G2 4개 개별 버퍼, 셰도 diff,
  HW 태스크/SW 병렬 전송, 재시도 마스크·버스 복구, 밝기/반전/플립, FPS 벤치마크.
  `config.h` 매크로 → `ds::DisplayConfig` 런타임 설정(기본값 = 1.1MHz·병합 전송·SW 100ns).
  - 셰도 비교를 함수 호출 대신 포인터로 직접 한다 (같은 결과, 루프 오버헤드만 줄임).
  - SW gpio 콜백이 하드코딩 핀 대신 `u8x8->pins`를 쓴다.
  - 수정: HW 명령 패킷 콜백의 넘침 플래그가 매 호출 초기화되던 문제 (넘친 패킷이 잘린 채 전송될 수 있었다).
- **입력** `ds::Buttons` + 순수 상태 머신 `ds::ButtonFsm` (짧게 > 50ms, 길게 > 1000ms — 기존과 동일).
- **부저** `ds::Buzzer`: 비프와 멜로디를 FreeRTOS 타이머로 **비차단** 재생.
- **네트워크** `ds::WifiService`(비차단 접속·포털·재접속), `ds::TimeSync`(NTP 감독, `tzProvider` 콜백).
- **설정** `ds::ConfigStore`(필드 테이블·NVS·지연 저장·마이그레이션 훅) + `ds::WebConfigApi`(GET/POST `/api/config`).
  - 로드 때 모든 필드를 검증한다 (범위 밖·잘못된 문자열은 기본값 유지). 저장은 하나라도 무효면 전체 거부.
  - 마이그레이션 훅은 NVS 네임스페이스가 없는 첫 부팅에도 불린다.
- **유틸** `ds::Logger`(시리얼 + OLED 4줄), 순수 JSON 파서 `ds_json_lite`.
- `library.properties`: `depends=U8g2, WiFiManager`.
- 호스트 테스트 6종(151건) — `test_json_lite`, `test_button_fsm` 신설.

## [v0.1.0] - 2026-11-09
### 🏗️ 라이브러리 기반 신설 (0단계)
- Arduino 라이브러리 골격 추가: `library.properties`, 묶음 헤더 `DeskStreamCore.h`(`DS_CORE_VERSION`), `namespace ds`.
- `ds_board.h`: 공통 핀 맵(BTN 1·4·10·9, 부저 7, HW I2C 5/6, SW I2C 2/3), OLED 주소(0x3C/0x3D), 화면 치수.
- Hangeul_Clock v2.10.0 에서 순수 모듈을 옮김(로직 변경 없음, 네임스페이스만 적용):
  - `display/ds_i2c_retry_policy.h`, `net/ds_time_sync_policy.h`, `net/ds_tz_util.*`, `util/ds_utf8.h`
  - `TIMEZONE_MAX_LEN` 매크로 → `ds::kTimezoneMaxLen` 상수 (값 48 동일)
- 호스트 테스트 4종과 `test/run_all.sh` 이전 — 109건 통과.
- 이 버전을 include 하는 스케치는 없었습니다 (v0.2.0에서 Hangeul_Clock 전환).
