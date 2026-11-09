# AI 작업 가이드 — DeskStream 새 프로젝트와 DeskStreamCore

이 문서는 DeskStream 저장소에서 **새 서브 프로젝트를 만들거나 기존 프로젝트를 고치는 AI**가 먼저 읽는 안내서다.
핵심은 하나다: **화면 전송·버튼·부저·WiFi·NTP·설정 저장·웹 설정 API는 직접 만들지 말고 `libraries/DeskStreamCore`를 쓴다.**
예전에는 프로젝트마다 이 코드를 복사해 써서 버전이 어긋났다. 그 상태로 되돌리지 않는 것이 이 문서의 목적이다.

- 라이브러리 원본: [`libraries/DeskStreamCore/`](./libraries/DeskStreamCore/) (README·RELEASE_NOTES 참조)
- 하드웨어: [`hardware.md`](./hardware.md) — ESP32-C3 SuperMini, SSD1306 128x64 × 4, 버튼 4개, 부저
- 사용 예(실제 프로젝트): `Hangeul_Clock`(전 모듈), `Universal_Timer`(이벤트 조회·자동 반복·알람),
  `Game_Of_Life`(버퍼 직접 쓰기·누름 즉시 반응), `Smart_Info_Station`(WiFi·NTP·오프셋 시간대),
  `Sound_Visualizer_4_OLEDs`(512x64 LovyanGFX 캔버스)

---

## 1. 반드시 지킬 규칙

1. **라이브러리 기능을 스케치에 복사하지 않는다.** 필요한 기능이 없거나 부족하면 라이브러리를 확장한다 (§7).
2. 스케치(.ino)는 **`#include <DeskStreamCore.h>`** 를 반드시 넣는다. 하위 헤더(`display/ds_display.h` 등)만
   include 하면 Arduino IDE가 라이브러리를 빌드에 넣지 않는다.
3. 모든 심볼은 `namespace ds` 안에 있다. 핀 번호를 다시 `#define` 하지 않는다 — `ds_board.h` 상수를 쓴다.
4. **화면은 `display.push()`로만 보낸다.** U8g2의 `sendBuffer()`/`updateDisplayArea()`를 직접 부르면 셰도 버퍼와
   실제 화면이 어긋난다.
5. `DisplayCore`, `TimeSync`는 **보드당 하나만** 만든다 (내부에서 정적 포인터로 콜백을 연결한다).
6. 라이브러리가 `sntp_sync_time()`을 대체한다. 스케치에서 같은 함수를 또 정의하지 않는다.
7. 웹 페이지나 설정 키를 바꾸면 기존 기기의 **NVS 키·타입을 그대로** 유지한다 (바꾸면 저장값이 사라진다).
8. 요청 범위 밖 코드는 고치지 않고, 커밋·푸시는 사용자가 지시할 때만 한다 (사용자 전역 규칙).

---

## 2. 빌드 환경

```bash
bash scripts/link-arduino-lib.sh        # PC마다 1회 — 스케치북 libraries/에 심볼릭 링크
```

- 필요한 외부 라이브러리: **U8g2**, **WiFiManager** (라이브러리의 모든 .cpp가 함께 컴파일되므로 WiFi를 안 써도 필요)
- 보드: `esp32:esp32:makergo_c3_supermini` (또는 `nologo_esp32c3_super_mini`), CPU 160MHz
- 앱이 1.25MB를 넘을 수 있으면 스케치 폴더에 **DeskStream 공통 `partitions.csv`** 를 복사하고(예: `ENG_Clock/partitions.csv`)
  IDE에서 `Partition Scheme = Huge APP`를 고른다. app0 2.5MB, spiffs는 `default`와 같은 `0x290000`이라
  다른 프로젝트로 바꿔 올려도 LittleFS 데이터(폰트·설정 파일)가 보존된다.

```bash
arduino-cli compile --fqbn "esp32:esp32:makergo_c3_supermini:CDCOnBoot=default,CPUFreq=160,PartitionScheme=default" <스케치폴더>
```

빌드 산출물로 수정 확인을 할 때는 `--build-path`를 저장소 밖(임시 폴더)으로 준다. 스케치의 `build/`는
웹 플래셔 수집 스크립트가 읽는 **Export 결과**라서 덮어쓰면 안 된다.

---

## 3. 보드 상수 (`ds_board.h`)

| 상수 | 값 | 비고 |
|:--|:--|:--|
| `ds::kButtonPins[4]` | `{1, 4, 10, 9}` | BTN1~BTN4, INPUT_PULLUP (LOW = 눌림) |
| `ds::kBuzzerPin` | `7` | 패시브 부저 |
| `ds::kHwSdaPin / kHwSclPin` | `5 / 6` | 화면 0·1 (HW I2C) |
| `ds::kSwSdaPin / kSwSclPin` | `2 / 3` | 화면 2·3 (고속 SW I2C) |
| `ds::kOledAddr[2]` | `{0x3C, 0x3D}` | 버스마다 같은 주소 쌍 |
| `ds::kNumScreens`, `kScreenWidth`, `kScreenHeight` | `4`, `128`, `64` | `kScreenBufBytes` = 1024 |

---

## 4. 최소 스케치 템플릿

```cpp
#include <Arduino.h>
#include <DeskStreamCore.h>

static ds::DisplayCore display;
static ds::Buttons buttons;
static ds::Buzzer buzzer;

static const ds::Note STARTUP[] = {{2093, 100, 20}, {2637, 100, 20}, {3136, 100, 20}, {4186, 100, 20}};
static int counter = 0;

static void onShort0() { buzzer.beep(50, 3000); counter++; }
static void onLong0()  { buzzer.beep(150, 2000); counter = 0; }

void setup() {
    Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(0);          // 시리얼 모니터를 안 열어도 loop()가 멈추지 않게
#endif
    display.begin();                   // 기본값 = HW 1.1MHz·병합 전송·SW 100ns·U8G2_R2 (정방향)
    buttons.begin();                   // 짧게 > 50ms, 길게 > 1000ms
    buttons.setCallbacks(0, onShort0, onLong0);
    buzzer.begin();
    buzzer.playMelody(STARTUP, 4);     // 비차단
}

void loop() {
    buttons.update();                  // 매 루프 호출 (폴링)
    static int shown = -1;
    if (counter != shown) {            // 바뀌었을 때만 그린다
        shown = counter;
        for (int i = 0; i < ds::kNumScreens; i++) {
            U8G2& u = display.screen(i);
            u.clearBuffer();
            u.setFont(u8g2_font_logisoso32_tr);
            u.setCursor(10, 50);
            u.print(counter + i);
        }
        display.push();                // 바뀐 타일만 4화면 병렬 전송
    }
}
```

---

## 5. 모듈별 사용법과 주의점

### 5.1 화면 — `ds::DisplayCore`

| API | 설명 |
|:--|:--|
| `begin(cfg = DisplayConfig())` | I2C 버스·HW 전송 태스크·U8G2 4개 초기화 후 화면을 지운다 |
| `screen(i)` / `screens()` | `U8G2&` / `U8G2**` — 화면마다 **독립 1KB 버퍼**(U8g2 공유 버퍼 문제 해결됨) |
| `push()` | 셰도와 비교해 바뀐 타일만 전송. 화면 0·1은 HW 태스크, 2·3은 호출 태스크에서 **병렬** |
| `clearAll()` | 4화면을 지우고 셰도를 무효화해 **전 화면 강제 재전송** (잔상 제거용) |
| `invalidate(i)` | 화면 i를 다음 `push()`에서 통째로 다시 보낸다 |
| `setBrightness(0~255)`, `setInversion(bool)`, `setFlip(bool)` | 4화면 동시 적용 |
| `runFpsBenchmark(frames, showMs)` | 풀 프레임 FPS 측정 (부팅 중에만, 안 부르면 링커가 제거) |

- **방향**: `U8G2_R2` + `setFlip(false)`가 이 보드의 정방향이다 (기본값). U8g2 초기화가 패널을 미러로 두고 R2가
  180° 돌려 상쇄된다. `setFlip(true)`는 180° 뒤집는다.
- **버퍼에 직접 쓰는 경우**(`getBufferPtr()`에 비트맵을 memcpy): 소프트웨어 회전이 적용되지 않으므로
  `cfg.rotation = U8G2_R0`으로 두고 필요하면 `setFlip(true)`로 뒤집는다 (Game_Of_Life 방식).
- 좌우/상하 미러 같은 4가지 반전이 필요하면 `display.screen(i).sendF("c", seg)` / `sendF("c", com)`으로 패널 명령을
  직접 보낸다 (GPS_DASH·Smart_Info_Station `updateDisplayFlip()` 참조).
- `push()`는 직전 HW 전송이 끝날 때까지 먼저 기다리므로, 반환 직후 버퍼를 다시 그려도 안전하다.
- 실측 (Hangeul_Clock): 4화면 풀 프레임 21.9ms / **45.7fps**. 설정값 근거는 `ds_display_config.h` 주석.
- **512x64 한 장 캔버스**(LovyanGFX 1bit 스프라이트 등): `display.pushCanvas((const uint8_t*)sprite.getBuffer(), reverseOrder)`.
  캔버스를 화면별로 전치(`ds::canvasToScreenPages`)해 U8g2 버퍼에 직접 쓰므로 소프트웨어 회전은 적용되지 않는다 —
  방향은 `setFlip()`, 180° 장착 시 화면 순서는 `reverseOrder`로 맞춘다 (Sound_Visualizer_4_OLEDs·OLED_Wide_test 참조).

### 5.2 버튼 — `ds::Buttons` (+ `ds::ButtonTiming`)

```cpp
ds::ButtonTiming t;            // debounceMs=50, longPressMs=1000, repeatDelayMs=0(끔), repeatIntervalMs=60, stableMs=0(끔)
t.repeatDelayMs = 300;         // 누르고 있으면 300ms 후 60ms 간격으로 REPEAT
buttons.begin(t);
```

- 이벤트는 **비트마스크**: `ds::BUTTON_SHORT`(놓을 때) · `BUTTON_LONG`(누른 채 1초 초과 즉시) ·
  `BUTTON_PRESS`(누르는 순간) · `BUTTON_REPEAT`. LONG과 REPEAT가 같은 update에서 함께 날 수 있다.
- 콜백 방식: `setCallbacks(idx, short, long)` 또는 `setCallback(idx, ds::BUTTON_PRESS, fn)`.
  콜백은 **`update()`를 부른 태스크**에서 실행된다.
- 조회 방식: `update()` 뒤 `if (buttons.events(0) & (ds::BUTTON_SHORT | ds::BUTTON_REPEAT)) …` (Universal_Timer 방식).
- 길게가 난 누름, 자동 반복이 시작된 누름은 놓아도 SHORT가 나지 않는다 (증가 중복 방지).
- 즉각 반응이 필요하면 `stableMs = 20` + `BUTTON_PRESS` (Game_Of_Life). 렌더링이 길어 입력을 놓칠 수 있으면
  `update()`를 5ms 주기 별도 FreeRTOS 태스크에서 부르고 콜백에서는 플래그만 세운다.
- 판정 로직은 순수 함수 `ds::ButtonFsm` — 호스트 테스트 `test/test_button_fsm.cpp`.

### 5.3 부저 — `ds::Buzzer`

- `beep(ms, hz)`, `playMelody(notes, count, loop=false)`, `stop()`, `isPlaying()`, `isLooping()`.
- `ds::Note = {주파수Hz(0=쉼표), 소리 ms, 다음 음까지 쉼 ms}`. **배열은 재생이 끝날 때까지 살아 있어야 한다** —
  `static const`로 선언한다.
- 모두 **비차단**(FreeRTOS 원샷 타이머). `delay()`로 멜로디를 만들지 않는다.
- 알람: `playMelody(ALARM, n, true)` → 끌 때 `stop()`. 알람 중 버튼 비프가 알람을 끊지 않게 하려면
  `if (!buzzer.isLooping()) buzzer.beep(...)`.
- 새 `beep()`/`playMelody()`는 진행 중인 소리를 끊는다.

### 5.4 WiFi — `ds::WifiService` (+ `ds::WifiConfig`)

```cpp
ds::WifiConfig w;
w.apName = "MyProject_Setup";   // 설정 포털 SSID
w.connectTimeoutMs = 10000;     // 저장된 AP 비차단 접속 대기 (넘으면 포털)
w.portalTimeoutS = 120;         // 0 = 무제한
w.restartOnFail = true;         // false면 실패해도 오프라인으로 계속 (DS_DECK)
w.onConnecting = [](uint32_t tick) { /* 500ms마다 진행 표시 */ };
w.onPortal = [](const char* ap) { /* 화면에 AP 이름·192.168.4.1 안내 후 display.push() */ };
bool ok = wifi.begin(w);        // 접속(+포털)이 끝날 때까지 반환하지 않는다
// loop(): wifi.loop();         // 끊기면 60초마다 재접속 시도
```

- `WiFiManager::autoConnect()`를 직접 쓰지 않는다 — 내부에서 최대 60초 블로킹한다.
- 공장 초기화: `ds::WifiService::resetSettings()`.
- 스케치에서 `WiFi.localIP()` 등을 쓰면 `#include <WiFi.h>`를 직접 넣는다.

### 5.5 NTP 시간 — `ds::TimeSync` (+ `ds::TimeSyncConfig`)

```cpp
static const char* currentTz() { return settings.timezone; }   // POSIX TZ, 예: "KST-9"
ds::TimeSyncConfig t;            // 서버 3개·SNTP 1시간·감독 6h/12h/30s/1분 기본값
t.tzProvider = currentTz;
timeSync.begin(t);
timeSync.applyTimezone();        // TZ 적용 + SNTP (재)시작 — TZ를 바꿀 때도 이 함수
// loop(): timeSync.loop();
```

- `tzProvider`는 **검증된** POSIX TZ를 돌려줘야 한다 (`ds::isValidTimezone()`).
  시간 오프셋만 저장하는 프로젝트는 `"UTC-9"`(= UTC+9, 부호 반대) 형태로 만들어 넘긴다 (Smart_Info_Station).
- `configTime(오프셋, …)`은 쓰지 않는다 — 코어가 TZ를 덮어써 UTC가 된다.
- 1초 갱신은 `millis()` 주기가 아니라 초 경계로: `ds::localTimeIsValid(time(nullptr))`,
  `ds::secondTickDue(last, now)`, `ds::clockRenderDue(...)`. 메인 루프에서 `getLocalTime()`(최대 5초 블로킹)을 쓰지 않는다.

### 5.6 설정 저장 — `ds::ConfigStore` (필드 테이블)

```cpp
struct Settings {
    uint8_t brightness = 1;
    bool    flipped = false;
    String  name = "Default";
    char    timezone[ds::kTimezoneMaxLen] = "KST-9";
};
static Settings g;
static ds::ConfigStore store;

static void onBright(const void* v) { g.brightness = *(const uint8_t*)v; display.setBrightness(g.brightness); store.setDirty(); }

// 순서 = 웹 POST 적용 순서 (먼저 적용돼야 하는 필드를 위에)
static const ds::ConfigField kFields[] = {
    DS_CFG_U8(Settings, brightness, "brightness", "bright", 1, 255, onBright),
    DS_CFG_BOOL(Settings, flipped, "is_flipped", "flip", nullptr),
    DS_CFG_STR(Settings, name, "name", "name", 64, nullptr, nullptr, 0),
    DS_CFG_CSTR(Settings, timezone, "timezone", "tz", ds::isValidTimezone, nullptr),
};
store.begin("myproj", &g, kFields, sizeof(kFields) / sizeof(kFields[0]));   // NVS 로드
```

- 타입: `U8`(min~max 검사) · `BOOL` · `STR`(Arduino String, 최대 길이) · `CSTR`(char 배열). **NVS 키는 15자 이하**.
- 로드 때 범위·검증 실패 값은 **구조체 기본값을 유지**한다. 저장(`setDirty()` → 5초 지연)은 필드가 **하나라도**
  무효면 전체를 거부한다.
- `onChange`가 `nullptr`이면 웹 변경 시 저장소가 값을 쓰고 `setDirty()`한다. 지정하면 **값 쓰기까지 그 함수가 한다**
  (하드웨어 반영 등 부수효과가 있을 때 — 버튼 경로와 같은 setter를 쓰게 한다).
- 플래그: `ds::FIELD_APPLY_ALWAYS`(값이 같아도 onChange 호출), `ds::FIELD_NO_WEB`(NVS 전용, 웹 비노출) —
  플래그를 주는 U8은 `DS_CFG_U8_F`.
- 마이그레이션: `store.begin(ns, &g, fields, n, 5000, afterLoad, beforeSave)` — `Preferences&`를 받는 훅
  (Hangeul_Clock `config_manager.cpp`의 `flip_mig` 참조). 첫 부팅(네임스페이스 없음)에도 afterLoad가 불린다.
- 지원하지 않는 타입(int 등)으로 이미 저장된 프로젝트를 옮길 때는 **기존 Preferences 코드를 유지**한다 (Stats_Monitor).

### 5.7 웹 설정 API — `ds::WebConfigApi`

```cpp
WebServer server(80);                     // 서버는 스케치가 소유한다
static ds::WebConfigApi api;
api.begin(server, store,
          [](String& json) { json += ",\"extra\":1"; },   // GET 응답 끝에 덧붙이기 (선택)
          [](const char* body) { /* 테이블 밖 키 처리, 화면 갱신 */ });
server.on("/", HTTP_GET, [] { server.send_P(200, PSTR("text/html"), PAGE_HTML); });  // 프로젝트 전용 경로
server.begin();
// loop(): server.handleClient();
```

- `GET /api/config` → 필드 테이블 JSON, `POST /api/config` → 본문의 키만 **테이블 순서대로** 파싱·검증·적용, 본문이 없으면 400.
- 큰 PROGMEM 페이지는 `send_P`로 보낸다 (`send()`는 힙에 통째로 복사해 백지 페이지가 된다).
- 테이블 밖 키는 `ds::jsonGetInt/jsonGetBool/jsonGetString(body, "key", …)`로 읽는다 (중국어 시계 `presentation`).
- 응답 JSON에 사용자 문자열을 넣을 때는 `ds::jsonEscape()`.

### 5.8 기타

- `ds::Logger`: `attachDisplay(&display, 0, u8g2_font_6x10_tf)` 후 `addLog()`/`updateLastLog()` — 시리얼 + 화면 0에 4줄 부팅 로그.
- `ds::utf8CharLen(p, remaining)`: UTF-8 글자 길이 (0을 돌려주지 않음 — `i += len` 루프 안전).
- `ds::isValidTimezone(tz)`: `setenv("TZ")`에 넣기 전 화이트리스트 검증.

---

## 6. 새 프로젝트 체크리스트

1. 폴더 `<ProjectName>/<ProjectName>.ino`, 파일 첫 줄 `// worynim@gmail.com` + Doxygen `@file/@brief/@details` 헤더.
2. `#include <DeskStreamCore.h>` — 화면·버튼·부저·WiFi·NTP·설정은 §5 모듈로.
3. 프로젝트 전용 상수는 `config.h`에, 보드 공통 값은 `ds_board.h`에서 (복제 금지).
4. 앱 크기가 1.25MB에 가까우면 공통 `partitions.csv` + `Huge APP` (§2).
5. Arduino 의존 없는 순수 로직은 별도 `.h/.cpp`로 분리해 `test/`에 호스트(g++) 테스트를 둔다.
6. `README.md`(기능·버튼·파일 구성·빌드 설정)와 `RELEASE_NOTES.md` 작성.
7. 루트 `README.md` 프로젝트 목록, 루트 `RELEASE_NOTES.md`에 항목 추가.
8. 웹 플래셔에 올리려면 `firmware/projects.json`에 항목 추가 → Arduino IDE `Export Compiled Binary`
   → `node tools/collect-firmware.mjs` (전체 실행 — `--project`는 manifest를 덮어쓴다) → `--check` 통과 확인.

---

## 7. 라이브러리를 고쳐야 할 때

- 새 기능이 둘 이상의 프로젝트에 쓰일 만하면 **라이브러리에 넣는다.** 한 프로젝트 전용이면 스케치에 둔다.
- 설정값은 매크로가 아니라 **설정 구조체 필드**로 받는다 (라이브러리는 스케치의 `config.h`를 볼 수 없다).
  기존 동작이 바뀌지 않도록 새 옵션의 기본값은 "끔"으로 둔다.
- 순수 로직이면 `src/`에 Arduino 의존 없이 두고 `test/`에 테스트를 추가한 뒤 `test/run_all.sh` 표에 등록한다.
- `library.properties`의 `version`과 `DeskStreamCore.h`의 `DS_CORE_VERSION`을 함께 올리고,
  `libraries/DeskStreamCore/RELEASE_NOTES.md`에 기록한다 (호환이 깨지면 명시).
- **이 라이브러리를 쓰는 모든 스케치를 다시 빌드한다** (목록은 라이브러리 README "사용 중인 스케치").
  특히 앱 크기를 확인한다.

```bash
bash libraries/DeskStreamCore/test/run_all.sh     # 라이브러리 호스트 테스트
node tools/collect-firmware.mjs --check           # 배포본 manifest 검증
```

- 시계 3종은 웹 페이지 JS와 펌웨어를 대조하는 하네스가 있다(`Hangeul_Clock/test/js/run_all.sh`,
  `ENG_Clock/test/js/run_all.sh`, `Chinese_Clock/test/run_all.sh`). 라이브러리 소스 경로를 읽는 검사가 있으므로
  웹 설정·NTP 쪽을 고치면 함께 돌린다.
