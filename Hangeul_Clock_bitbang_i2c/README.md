# Hangeul Clock BB — v1.0.9

4× 1.3" OLED 한글 시계. ESP32-C3에서 **4개의 독립 BitBang I2C 버스**로 화면 4개를
**동시에** 구동합니다. [`Hangeul_Clock`](../Hangeul_Clock/) v2.9.3을 복사해
**전송 계층만 교체**한 파생 프로젝트입니다.

> 실기 검증 완료(2026-10-10). 원본과 동급 속도로 동작합니다.

---

## 1. 왜 BitBang인가

0.96" SSD1306 모듈은 주소 패드를 잘라 0x3C/0x3D를 고를 수 있지만, **1.3" 모듈은
주소를 바꿀 수 없습니다.** 네 개가 모두 0x3C이므로 버스가 4개 필요하고,
ESP32-C3에 하드웨어 I2C는 1개뿐입니다.

→ **SCL 1개를 공유하고 SDA만 4개로 나눕니다.** 5핀으로 4버스를 얻습니다.

```
   GPIO 10 ──┬── OLED1 SCL
             ├── OLED2 SCL          (4개 모듈 공통)
             ├── OLED3 SCL
             └── OLED4 SCL
   GPIO  5 ────── OLED1 SDA
   GPIO  6 ────── OLED2 SDA
   GPIO  7 ────── OLED3 SDA
   GPIO  8 ────── OLED4 SDA
```

**동시 전송이 가능한 이유**: I2C 슬레이브는 SCL 상승 에지에서 **자기 SDA만** 래치합니다.
SCL을 공유한 채 4개 SDA에 서로 다른 비트를 실으면 **4개 화면이 각자 자기 바이트를
동시에** 받습니다. ACK도 한 클럭에 4개를 모두 읽습니다.

**필요한 이유**: ESP32-C3는 단일 코어라 순차 전송이 곧 메인 루프 정지입니다.

| | 4화면 풀 프레임 |
|:--|--:|
| 순차 전송 (화면 1대씩) | 약 41 ms |
| **동시 전송 (기본)** | **약 10 ms** |

---

## 2. 하드웨어

| 신호 | GPIO | 비고 |
|:--|:--|:--|
| **SCL** (공유) | **10** | 4버스 공통 클럭 |
| **SDA** | **5 / 6 / 7 / 8** | 물리 OLED1 / 2 / 3 / 4, 전부 주소 0x3C |
| BTN 1 | 0 | `INPUT_PULLUP` |
| BTN 2 | 1 | 〃 |
| BTN 3 | 3 | 〃 |
| BTN 4 | 4 | 〃 (부팅 시 진단 트리거) |
| 부저 | — | **미연결** (`HAS_BUZZER 0`) |

- **MCU**: ESP32-C3 SuperMini — `esp32:esp32:nologo_esp32c3_super_mini`
- **Display**: 1.3" **SH1106** 128×64 ×4, 4핀(VCC/GND/SCL/SDA), 리셋 없음
- ⚠ **GPIO 8은 ESP32-C3 스트래핑 핀**(부팅 시 High)입니다. 모듈 내장 풀업으로 통상
  문제없지만, 부팅이 불안정하면 GPIO 8에 외부 4.7k 풀업을 추가하세요.

### 화면 배치 (좌→우)

`screens[0]`이 `[오전오후/날짜]`, 이어서 `[시] [분] [초]`입니다.
실기 배치가 반대로 보여 **화면↔버스 배정을 뒤집어** 두었습니다.

```c
#define BB_SCREEN_ORDER_REVERSED 1     // config.h — 0으로 두면 원래 배정
```

> **핀 배열(`BB_SDA_PINS`)은 건드리지 않습니다.** 버스↔핀 대응은 검증된 그대로 두고
> 화면이 어느 버스를 쓸지만 바꿉니다.

---

## 3. 컨트롤러 — SH1106

1.3" 128×64 I2C 모듈은 **겉모습도 핀 배치도 주소(0x3C)도 같고 컨트롤러만 다릅니다.**
I2C에 읽기 경로가 없어 **소프트웨어로 구별할 수 없습니다.** 실기에서는
**SH1106**으로 확인되었습니다(개요.md의 SSD1315 표기는 달랐습니다).

```c
#define OLED_DRIVER OLED_DRIVER_SH1106   // config.h
```

| 값 | 열 오프셋 | 비고 |
|:--|--:|:--|
| **`OLED_DRIVER_SH1106`** (기본) | **+2** | 132열 RAM — 가시 영역이 2열부터 |
| `OLED_DRIVER_SSD1315` | 0 | |
| `OLED_DRIVER_SSD1306` | 0 | |

> 어느 것인지 모르면 **BTN4를 누른 채 부팅**해 컨트롤러 스윕(진단 3)을 돌립니다.
> 네 종류를 차례로 그려 보므로 **테두리가 반듯한 것**을 고르면 됩니다.

---

## 4. 버튼

| 버튼 | 짧게 | 길게 (1초) |
|:--|:--|:--|
| **BTN 1** (GPIO 0) | 시보 ON/OFF | 화면 뒤집기 (FLIP) |
| **BTN 2** (GPIO 1) | 한글 ↔ 숫자 모드 | 12H ↔ 24H |
| **BTN 3** (GPIO 3) | 애니메이션 8종 순환 | 폰트 슬롯 변경 (0~4) |
| **BTN 4** (GPIO 4) | 다음 페이지 (시계 → IP → 도움말) | 색상 반전 (INVERT) |

> **부저가 없어 버튼은 무음입니다.** 시보는 화면의 종 아이콘으로만 표시되고,
> 웹 설정의 시보 토글은 그대로 살아 있습니다.

**애니메이션 8종**: Scroll Up · Scroll Down · Vertical Flip · Dithered Fade · Zoom ·
Snow Assemble · Split Flap · 없음

---

## 5. 빌드

```bash
arduino-cli compile --fqbn esp32:esp32:nologo_esp32c3_super_mini --build-path ./build .
arduino-cli upload  --fqbn esp32:esp32:nologo_esp32c3_super_mini -p /dev/tty.usbmodemXXXX .
```

Arduino IDE라면 `Hangeul_Clock_bitbang_i2c.ino`를 열고 보드를
**"Nologo ESP32C3 SuperMini"** 로 고릅니다.

| 라이브러리 | 설치 | 용도 |
|:--|:--|:--|
| **U8g2** 2.36.19+ | 라이브러리 매니저 | 렌더링 · SH1106 드라이버 · 명령 바이트 생성 |
| **WiFiManager** | 라이브러리 매니저 | AP 설정 포털 |
| **Multi_BitBang** | **불필요** — 스케치에 내장 | 부팅 시 버스 진단 |

Multi_BitBang은 GPLv3이며 원본 그대로 들어 있습니다 — 출처·이유·주의는
[`VENDORED.md`](./VENDORED.md)를 보세요. **프레임 전송에는 쓰지 않습니다.**

**현재 사용량**: Flash 1,288,953 B (98%, 원본과 동일) / RAM 59,956 B (18%)

---

## 6. 시리얼로 상태 확인

부팅 시 115200으로 네 줄이 찍힙니다. **화면이 이상할 때 여기부터 봅니다.**

```
[I2C] BitBang bus scan (Multi_BitBang)
[I2C] 화면0 (SDA 5): 0x3C OK          ← 하나라도 NOT FOUND면 배선/전원
[I2C] 화면1 (SDA 6): 0x3C OK
[I2C] 화면2 (SDA 7): 0x3C OK
[I2C] 화면3 (SDA 8): 0x3C OK
[I2C] 반주기 지연: 없음 (최대 속도)
[SYSTEM] Hangeul_Clock_bitbang_i2c v1.0.9 OLED=SH1106 frame=CONCURRENT screenRev=1
```

**마지막 줄이 보이면 정상 시계 펌웨어가 도는 중입니다.**

동시 전송이 실패하면 **처음 한 번** 알려 줍니다 — 이 줄이 **안 나와야** 정상입니다.

```
[I2C] 동시 재생 실패 -> 순차 폴백 (bus=0x08 ok=0x00) err=1
```

---

## 7. 진단 모드 — BTN4를 누른 채 부팅

**컴파일 스위치가 아니라 런타임 트리거입니다.** 스위치가 없으니 "시계 대신 진단이
도는" 상황이 생기지 않습니다. BTN4를 누르지 않으면 항상 정상 시계로 부팅합니다.

| 단계 | 시험 | 판정 |
|:--|:--|:--|
| 진단 1 | 내장 Multi_BitBang으로 4버스 스캔 | `NOT FOUND` → 배선/전원 (SDA·SCL·VCC·GND·풀업) |
| 진단 2 | 이 펌웨어 드라이버로 버스별 probe | `NACK` → ACK 판독 또는 배선 |
| 진단 3 | **컨트롤러 스윕** — SH1106/WINSTAR/SSD1315/SSD1306을 6초씩 | 제대로 나오는 것을 `OLED_DRIVER`에 |
| 진단 4 | **A/B/C 비교** — U8g2 / 캡처·재생 / 순차를 같은 내용으로 | 셋이 같아야 정상 |

진단 3은 U8g2의 드라이버를 쓰고, 진단 1은 **서드파티 Multi_BitBang**을 씁니다 —
그래서 "하드웨어 vs 코드"가 갈립니다. 진단 4는 어느 전송 경로가 깨지는지 특정합니다.

---

## 8. 조정 지점 (`config.h`)

| 매크로 | 기본 | 언제 바꾸나 |
|:--|:--|:--|
| `OLED_DRIVER` | `SH1106` | 컨트롤러가 다를 때 (진단 3으로 확인) |
| `BB_SCREEN_ORDER_REVERSED` | `1` | 좌→우 배치가 반대일 때 |
| `BB_HALF_BIT_NS` | `0` | 화면이 깨질 때 **키운다** (1250 = 약 400kHz) |
| `BB_FRAME_PATH` | `CONCURRENT` | 동시 전송이 문제일 때 `U8G2`(순차, 안전) |
| `HAS_BUZZER` | `0` | 부저를 달 때 (**GPIO 7은 OLED3 SDA라 재지정 필요**) |

---

## 9. 파일 구성

| 파일 | 역할 |
|:--|:--|
| `Hangeul_Clock_bitbang_i2c.ino` | 메인 (setup/loop, 버튼 콜백, 진단 트리거) |
| `config.h` | 핀맵 · 컨트롤러 · 전송 경로 · 클럭 |
| `bitbang_i2c_core.h` | **순수 판단 로직** — 비트 순서, ACK 극성, 열 구간 합집합 (호스트 테스트 대상) |
| `bitbang_i2c.{h,cpp}` | ESP32-C3 BitBang 드라이버 — 4버스 동시 전송 |
| `i2c_platform.{h,cpp}` | **캡처 후 재생** 전송 계층 + 셰도 버퍼 |
| `oled_panel.h` | `OLED_DRIVER`에 따라 U8g2 클래스를 고르는 typedef |
| `bb_diagnostic.{h,cpp}` | 하드웨어 진단 (BTN4 트리거, 항상 컴파일) |
| `display_manager.{h,cpp}` | 4화면 렌더링 · UI 스테이지 · 애니메이션 |
| `renderer.*` `hangeul_time.*` | 한글 글리프 렌더러 · 시간→한글 변환 |
| `web_manager.*` `web_pages.h` | 웹 대시보드 + Font Studio |
| `Multi_BitBang.*` | 벤더링한 외부 라이브러리 |

**원본 v2.9.3과 동일한 파일**: `renderer*` `hangeul_time*` `tz_util` `utf8_len`
`config_manager*` `input_manager*` `logger*` `web_manager*` `web_pages.h`

설계 배경과 함정은 [`PLAN.md`](./PLAN.md)에 정리해 두었습니다.

---

## 10. 웹 대시보드

첫 부팅 시 AP **`Hangeul_Clock_BB`** 에 접속해 WiFi를 설정하면, 이후 브라우저에서:

- 시간대(POSIX TZ) · 밝기 · 12/24H · 표시 모드 · 애니메이션 · 색상 반전
- **Font Studio** — 브라우저에서 TTF를 64px 비트맵으로 래스터화해 기기에 업로드
- 폰트 슬롯 5개(`/f0`~`/f4`) 관리, 슬롯별 이름표

> 폰트를 올리지 않으면 내장 폰트로 대체 표시됩니다.

---

## 11. 테스트

```bash
cd test
g++ -std=c++11 -I.. test_bitbang_core.cpp -o /tmp/t && /tmp/t   # 비트뱅 인코딩/정렬
g++ -std=c++11 -I.. test_utf8_len.cpp    -o /tmp/t && /tmp/t
g++ -std=c++11 -I.. test_tz_util.cpp ../tz_util.cpp             -o /tmp/t && /tmp/t
g++ -std=c++11 -I.. test_geometry.cpp ../renderer_geometry.cpp  -o /tmp/t && /tmp/t
g++ -std=c++11 -I.. test_layout.cpp ../renderer_layout.cpp      -o /tmp/t && /tmp/t
bash js/run_all.sh                                              # 웹↔펌웨어 배선
```

**136건(호스트) + 45건(JS) = 181건.** 자세한 내용은 [`test/README.md`](./test/README.md).

---

## 12. 문제가 생기면

| 증상 | 먼저 볼 것 |
|:--|:--|
| 화면이 **아무것도** 안 나옴 | 시리얼의 `진단1` 스캔 → `NOT FOUND`면 배선/전원 |
| 화면이 **깨져** 나옴 | ① `OLED_DRIVER`가 맞나(진단 3) ② `BB_HALF_BIT_NS`를 크게 ③ `BB_FRAME_PATH`를 `U8G2`로 |
| **느림** | 시리얼에 `동시 재생 실패`가 있나 → 있으면 동시 전송이 죽은 것 |
| 부팅이 **불안정** | GPIO 8(스트래핑)에 외부 4.7k 풀업 |
| 여러 화면만 **안 바뀜** | `동시 재생 실패` 로그 확인 (폴백이 화면은 살리지만 느려짐) |

---

## 13. 알려진 제약

- **부저 미연결** — 완전 무음
- **Multi_BitBang이 GPLv3** — 배포 시 라이선스 전파 ([`VENDORED.md`](./VENDORED.md) §3)
- **Flash 98%** — 원본과 동일. 남는 23 KB 중 약 8 KB가 진단 코드
- **SCL stuck-low**는 비트뱅에서 ACK 성공으로 오독될 수 있어, 상승 대기 타임아웃과
  오류 누적 시 `recoverBus()`(SCL 펄스)로 대응합니다

변경 이력은 [`RELEASE_NOTES.md`](./RELEASE_NOTES.md).
