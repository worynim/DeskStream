# DeskStreamCore

DeskStream 보드(ESP32-C3 + SSD1306 128x64 × 4) 서브 프로젝트들이 함께 쓰는 공통 모듈 Arduino 라이브러리입니다.
프로젝트마다 복사해 쓰던 코드(`[SYNC]` 주석으로 관리)를 이 라이브러리 한 곳으로 모읍니다.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## 설치 (PC마다 1회)

```bash
bash scripts/link-arduino-lib.sh
```

저장소의 `libraries/DeskStreamCore`를 Arduino 스케치북의 `libraries/` 폴더(기본 `~/Documents/Arduino/libraries`)에
심볼릭 링크로 연결합니다. Arduino IDE / arduino-cli 모두 이 링크를 통해 라이브러리를 찾습니다.
Windows(`mklink /J` 정션)·외부 라이브러리 설치·확인 방법은 루트 [README의 "최초 설정"](../../README.md#최초-설정-pc마다-1회)을 참고하세요.

## 사용

스케치(.ino)에서 반드시 묶음 헤더를 include 합니다. Arduino IDE는 `src/` 루트의 헤더 이름으로
라이브러리를 찾으므로 하위 폴더 헤더만 include 하면 라이브러리가 빌드에 포함되지 않습니다.

```cpp
#include <DeskStreamCore.h>

ds::DisplayCore disp;
ds::Buttons buttons;

void setup() {
    disp.begin();                         // 기본값 = HW 1.1MHz · 병합 전송 · SW 100ns · U8G2_R2
    buttons.begin();
    buttons.setCallbacks(0, onShort, onLong);
}
void loop() {
    buttons.update();
    disp.screen(0).drawStr(0, 10, "Hello");
    disp.push();                          // 바뀐 페이지만, HW·SW 버스 병렬 전송
}
```

모든 심볼은 `namespace ds` 안에 있습니다. 프로젝트별 값은 `ds::DisplayConfig`·`ds::WifiConfig`·
`ds::TimeSyncConfig`·`ds::ButtonTiming` 구조체로 넘깁니다 (기본값 = Hangeul_Clock 채택값).
쓰지 않는 모듈의 코드는 링커가 제거합니다.

> 필요한 라이브러리: `U8g2` (Arduino 라이브러리 매니저). WiFi 설정 포털은 이 라이브러리에 내장돼 있어
> WiFiManager는 필요 없습니다 (v0.6.0~).

## 모듈

| 파일 | 내용 | 출처 |
|:--|:--|:--|
| `src/ds_board.h` | 공통 핀 맵(버튼·부저·I2C)·OLED 주소·화면 치수 | `hardware.md` |
| `src/display/ds_display.*` | **DisplayCore** — U8G2 4개(개별 버퍼)·셰도 diff·HW/SW 병렬 전송·밝기/반전/플립·FPS 벤치마크 | Hangeul_Clock v2.10.0 |
| `src/display/ds_display_config.h` | 화면 엔진 설정(I2C 속도·병합 전송·SW 반주기·타임아웃·태스크) + 실측 근거 | Hangeul_Clock v2.10.0 |
| `src/display/ds_i2c_platform.*` | HW I2C 비동기 전송 태스크·셰도 버퍼·버스 자가 복구 | Hangeul_Clock v2.10.0 |
| `src/display/ds_sw_i2c_fast.*` | SW I2C 고속 byte 콜백 (레지스터 직접·보정 NOP, 실효 ~1.19MHz) | Hangeul_Clock v2.10.0 |
| `src/display/ds_canvas.*` | 512x64 1bpp 캔버스 → 화면별 페이지 버퍼 전치 (순수) — `DisplayCore::pushCanvas()` | Sound_Visualizer·Game_Of_Life |
| `src/display/ds_i2c_retry_policy.h` | HW I2C 전송 후 dirty 마스크 갱신 정책 (순수) | Hangeul_Clock v2.10.0 |
| `src/input/ds_buttons.*` | 4버튼 폴링 — 콜백(짧게/길게/누름/반복) 또는 이벤트 조회 | Hangeul_Clock + Universal_Timer |
| `src/input/ds_button_fsm.h` | 버튼 판정 상태 머신 — 자동 반복·안정 디바운스 옵션 (순수) | Hangeul·Universal_Timer·Game_Of_Life |
| `src/sound/ds_buzzer.*` | 비차단 비프·멜로디·반복 알람 (FreeRTOS 타이머) | Hangeul_Clock + Universal_Timer |
| `src/net/ds_wifi.*` | 저장 AP 비차단 접속 → **자체 설정 포털**(열린 AP·캡티브 DNS·SSID 목록) → 끊김 재접속 | Hangeul_Clock v2.10.0 + 자체 포털 |
| `src/net/ds_wifi_portal_util.*` | 포털 입력 검증(SSID·비밀번호 길이)·HTML 이스케이프 (순수) | 신규 |
| `src/net/ds_time_sync.*` | NTP 재동기화 감독·드리프트 기록 (`sntp_sync_time` 대체) | Hangeul_Clock v2.10.0 |
| `src/net/ds_time_sync_policy.h` | NTP 재동기화 시점·1초 렌더 트리거 규칙 (순수) | Hangeul_Clock v2.10.0 |
| `src/net/ds_tz_util.*` | POSIX TZ 문자열 화이트리스트 검증 (순수) | Hangeul_Clock v2.10.0 |
| `src/net/ds_web_config.*` | 필드 테이블 기반 `GET/POST /api/config` + `jsonEscape` | Hangeul·Chinese_Clock web_manager |
| `src/store/ds_config.*` | 필드 테이블 기반 NVS 로드·검증·지연 저장·마이그레이션 훅 | Hangeul_Clock config_manager |
| `src/util/ds_json_lite.*` | 평평한 JSON 키 값 파서 (순수) | 신규 (parseVal 대체) |
| `src/util/ds_logger.*` | 부팅 로그 (시리얼 + OLED 한 화면 4줄) | Hangeul_Clock v2.10.0 |
| `src/util/ds_utf8.h` | UTF-8 리딩 바이트 길이 판정 (순수) | Hangeul_Clock v2.10.0 |

사용 중인 스케치: Hangeul_Clock (v2.11.0~), ENG_Clock (v1.1.0~), Chinese_Clock (v1.1.0~), Game_Of_Life (v1.1.0~),
Universal_Timer (v1.1.0~), Stats_Monitor (v1.6.0~), GPS_DASH (v2.7.0~), DS_DECK (v1.6.0~), Smart_Info_Station (v2.10.0~),
Sound_Visualizer_4_OLEDs (v3.5.0~), Screen_To_OLEDs (v2.3.0~), OLED_Wide_test (v1.3.0~), Agent_Monitor (v0.1.0~),
Wireless_Analyzer (v0.1.0~) — 저장소의 14개 프로젝트 전부.

## 테스트

Arduino 의존이 없는 순수 모듈은 호스트(g++)에서 검증합니다.

```bash
bash libraries/DeskStreamCore/test/run_all.sh
```

| 파일 | 내용 |
|:--|:--|
| `test/test_tz_util.cpp` | POSIX TZ 화이트리스트 검증 |
| `test/test_utf8_len.cpp` | UTF-8 리딩 바이트 판정 (4바이트 오판정 회귀, 한글·한자 포함) |
| `test/test_i2c_retry_policy.cpp` | 전송 실패 페이지 재시도 마스크 회귀 |
| `test/test_time_sync_policy.cpp` | NTP 재동기화 정책·1초 렌더 트리거·millis 랩 구간 |
| `test/test_json_lite.cpp` | JSON 키 값 파서 (키 오프셋 버그 회귀, 문자열 속 키 오인, 이스케이프·잘림 거부) |
| `test/test_wifi_portal.cpp` | 포털 SSID·비밀번호 검증, SSID HTML 이스케이프(스크립트 삽입 방지) |
| `test/test_canvas.cpp` | 캔버스 전치 좌표·화면 경계·무작위 패턴 전 픽셀 왕복 |
| `test/test_button_fsm.cpp` | 버튼 짧게/길게 경계·바운스·millis 랩·자동 반복·LONG\|REPEAT 동시·안정 디바운스 |

## 규칙

- 라이브러리는 스케치의 `config.h`를 볼 수 없으므로, 프로젝트별 값은 매크로가 아닌 설정 구조체로 넘깁니다.
- 라이브러리를 고치면 이미 전환한 스케치를 모두 다시 빌드합니다. `tools/collect-firmware.mjs`는 이 헤더를 include 하는 스케치에 대해 라이브러리 변경도 "소스가 바이너리보다 최신" 판정에 반영합니다.
- 버전은 `library.properties`의 `version`과 `DeskStreamCore.h`의 `DS_CORE_VERSION`을 함께 올립니다. 변경 내역은 [RELEASE_NOTES.md](./RELEASE_NOTES.md)에 기록합니다.
