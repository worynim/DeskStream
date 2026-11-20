# 📖 DeskStream Page Turner (BLE 페이지 터너)

DeskStream 보드를 세로로 손에 쥐고 엄지로 버튼을 눌러 **웹소설·웹툰·유튜브 쇼츠를 넘기는** 블루투스 리모컨입니다.
스마트폰 카메라 셔터(딜레이 셔터 포함)와 PC 발표 슬라이드 넘김에도 씁니다. 별도 앱 없이 폰·PC의 블루투스 설정에서 페어링만 하면 됩니다.

> 🌐 [웹 플래셔](https://worynim.github.io/DeskStream/)에서 바로 설치할 수 있습니다. 설계: [DESIGN.md](./DESIGN.md)

---

## 📌 주요 특징

1. **앱 이름별 모드 7개**: Android 실기 시험으로 앱마다 잘 먹히는 신호(볼륨 키·방향키·마우스 휠·PageUp/Down)와 이동량을 미리 맞춰 두었습니다.
2. **연결 기기 슬롯 3개**: 폰·태블릿·PC를 슬롯 1~3에 하나씩 기억하고, 버튼 두 번으로 바로 바꿉니다. 다시 페어링할 필요가 없습니다.
3. **세로로 쥐는 화면**: 각 OLED가 옆 버튼의 역할을 화살표·아이콘으로 보여줍니다. 어두운 곳에서 눈부시지 않도록 밝기를 최저로 두고, 채운 상자·반전 없이 테두리로만 그립니다 (켜지는 픽셀이 적어 전력도 덜 씀).
4. **버튼만으로 설정**: 휠 칸 수·방향키 횟수·간격·커서 숨기기·딜레이 셔터 시간·버튼음. 설정과 마지막 모드·슬롯은 저장되어 전원을 꺼도 유지됩니다.
5. **배터리 절약**: 1분 무입력 시 화면 끄기, 5분 무입력 시 딥슬립.

---

## 📱 모드 (Android에서 확인)

| # | 모드 | 보내는 신호 | 기본값 | 비고 |
|---|---|---|---|---|
| 1 | YouTube | 마우스 휠 | Steps x3, Gap 30ms, Cursor Hide | 쇼츠 넘기기 |
| 2 | Kakao Webtoon | 방향키 ↑↓ | Keys x4, Gap 30ms | |
| 3 | Kakao Novel | 볼륨 +/− | — | 앱에서 **볼륨 키로 페이지 넘기기**를 켜야 합니다 |
| 4 | Series Webtoon | 마우스 휠 | Steps x10, Gap 80ms, Cursor Hide | |
| 5 | Series Novel | 볼륨 +/− | — | 앱에서 **볼륨 키로 페이지 넘기기**를 켜야 합니다 |
| 6 | Camera | BTN1 볼륨+ / BTN2 딜레이 셔터 | Timer 3s | BTN2는 몇 초 뒤 촬영 (세는 중 다시 누르면 취소) |
| 7 | PPT | PageUp / PageDown | — | PC 발표 |

- **휠 모드**는 휠을 한 번에 크게 보내지 않고 1칸씩 간격을 두고 보내 부드럽게 스크롤합니다.
- **Cursor Hide**: 휠을 보내기 전에 마우스 커서를 화면 오른쪽 끝으로 옮겨 화면을 덜 가립니다.

---

## 🎮 조작 가이드

장치를 세로로 쥐면 위에서부터 OLED1 ~ OLED4, 각 화면 옆에 BTN1 ~ BTN4가 있습니다.
각 화면은 **맨 위에 그 버튼이 하는 일**, 가운데에 아이콘, **맨 아래에 버튼 이름표**(`BTN1`, `MODE 4/7`, `BT1 ON` …)를 보여줍니다.

| 버튼 | 일반 화면 | 설정 메뉴 안 |
|---|---|---|
| **BTN1** | 이전 페이지 / 위로 스크롤 / 셔터 | 값 줄이기 (Off·Show 고르기) |
| **BTN2** | 다음 페이지 / 아래로 스크롤 / 딜레이 셔터 | 값 늘리기 (On·Hide 고르기) |
| **BTN3** | 다음 모드 · **길게 = 기기(슬롯) 선택** | 취소 (바꾼 값 버림) |
| **BTN4** | 설정 메뉴 열기 | 다음 항목, 마지막 항목에서 저장 |

### 설정 항목

| 항목 | 뜻 | 값 | 있는 모드 |
|---|---|---|---|
| Steps | 한 번 누를 때 보내는 휠 칸 수 | x1 ~ x30 | YouTube, Series Webtoon |
| Keys | 한 번 누를 때 보내는 방향키 횟수 | x1 ~ x20 | Kakao Webtoon |
| Gap | 칸·키 사이 간격 | 10 ~ 250ms | 위 세 모드 |
| Cursor | 휠 전에 커서를 화면 끝으로 치우기 | Hide / Show | YouTube, Series Webtoon |
| Timer | BTN2 딜레이 셔터까지 걸리는 시간 | 2 / 3 / 5 / 10s | Camera |
| Beep | 버튼음 | On / Off | 모든 모드 공통 |

---

## 🔗 연결 기기 슬롯 (BTN3 길게)

슬롯 1~3에 기기를 하나씩 기억합니다. 지금 슬롯은 OLED4 이름표에 `BT1 ON`(연결됨) / `BT1 wait`(기다리는 중)로 보입니다.

| 기기 선택 화면에서 | 동작 |
|---|---|
| BTN1 / BTN2 / BTN3 짧게 | 슬롯 1 / 2 / 3 고르기 (테두리 두 겹) → **같은 버튼을 한 번 더** 누르면 그 슬롯 사용 |
| BTN1 / BTN2 / BTN3 길게 | 그 슬롯에 **새 기기 페어링 대기** |
| BTN4 | 취소 |

- 쓰는 중인 슬롯은 번호에 테두리가 둘러집니다 (`In use`).
- 페어링된 슬롯을 쓰면 **그 기기만** 연결됩니다. 다른 기기에는 장치가 보이지 않습니다.
- 빈 슬롯을 고르거나 슬롯을 길게 누르면 **페어링 대기** 화면이 됩니다. 새 기기에서 `DS Page Turner`를 찾아 페어링하세요.
  - 슬롯에 있던 기존 기기는 **새 기기가 페어링되는 순간에** 바뀝니다.
  - **BTN3 짧게** 또는 **60초 경과** → 쓰던 기기로 그대로 돌아갑니다.
  - 슬롯 3개가 모두 찬 상태에서 바꾸다가 페어링이 실패하면, 그 슬롯의 기존 기기는 다시 페어링해야 합니다.
- 처음 켤 때 이미 페어링된 기기는 빈 슬롯에 자동으로 들어갑니다.

### 처음 연결하기

1. 보드를 켭니다. 페어링된 기기가 없으면 바로 슬롯 1 페어링 대기 화면이 나옵니다.
2. 폰의 블루투스 설정에서 **`DS Page Turner`** 를 찾아 페어링합니다. OLED4가 `BT1 ON`으로 바뀝니다.
3. BTN3으로 쓰려는 앱의 모드를 고릅니다.

---

## 🔋 화면 끄기와 딥슬립

| 마지막 버튼 입력 후 | 동작 |
|---|---|
| 1분 | 화면을 끕니다 (블루투스 연결은 유지). 다음 첫 버튼은 **화면만 켜고** 신호를 보내지 않습니다. |
| 5분 | 딥슬립에 들어갑니다 (연결이 끊김). **BTN1 또는 BTN2로 깨웁니다** — BTN3·BTN4로는 깨울 수 없습니다(ESP32-C3는 GPIO 0~5만 딥슬립 깨우기 가능). 깨면 몇 초 안에 쓰던 기기에 다시 연결됩니다. |

딥슬립 중에는 USB 연결도 끊기므로, 펌웨어를 올릴 때는 버튼으로 먼저 깨우세요.

---

## 🛠️ 하드웨어 구성

- **MCU**: ESP32-C3 SuperMini
- **Display**: 0.96" SSD1306 128×64 OLED × 4 (세로 방향으로 돌려 화면마다 64×128로 사용)
- **Buttons**: BTN1 (GPIO 1), BTN2 (GPIO 4), BTN3 (GPIO 10), BTN4 (GPIO 9)
- **Buzzer**: 패시브 부저 (GPIO 7)
- **전원**: 손에 들고 쓰려면 배터리를 따로 연결합니다 (V2 하드웨어에서 내장 예정).

---

## 📁 프로젝트 파일 구성

| 파일 | 내용 |
|---|---|
| `Page_Turner.ino` | 버튼 → 모듈 연결, 화면 끄기·딥슬립 |
| `config.h` | 버전·BLE 이름·페어링 대기 시간·OLED 밝기 |
| `model.h/.cpp` | 모드 표·설정 항목·값 목록 (하드웨어 무관) |
| `menu.h/.cpp` | 설정 메뉴 상태 기계 (하드웨어 무관) |
| `shutter_timer.h/.cpp` | 딜레이 셔터 카운트다운 (하드웨어 무관) |
| `power_policy.h` | 무입력 시간 → 화면 끄기·딥슬립 판단 (하드웨어 무관) |
| `slots.h/.cpp` | 연결 기기 슬롯 표 (하드웨어 무관) |
| `hid_ble.h/.cpp` | BLE HID(키보드·미디어 키·마우스 휠), 신호 전송 태스크, 슬롯 연결·페어링 |
| `view.h/.cpp` | 4화면 그리기 |
| `settings_store.h/.cpp` | 설정·마지막 모드 NVS 저장 (`ds::ConfigStore`) |
| `build_opt.h` | NimBLE 구독(CCCD) 저장 한도 8 → 24 (기기 2대째부터 페어링이 지워지는 문제 방지) |
| `partitions.csv` | DeskStream 공통 파티션 |
| `test/` | 호스트(PC) 단위 테스트 |
| `DESIGN.md` | 설계서 |

화면·버튼·부저·설정 저장은 공통 라이브러리 [DeskStreamCore](../libraries/DeskStreamCore/)를, BLE는 **NimBLE-Arduino 2.5.0**을 씁니다.
빌드 전 저장소 루트에서 `bash scripts/link-arduino-lib.sh`를 PC마다 1회 실행합니다.

---

## 🚀 빌드 · 업로드 · 테스트

```bash
arduino-cli compile --fqbn "esp32:esp32:makergo_c3_supermini:CDCOnBoot=default,CPUFreq=160,PartitionScheme=huge_app" Page_Turner
```

```bash
arduino-cli upload --fqbn "esp32:esp32:makergo_c3_supermini:CDCOnBoot=default,CPUFreq=160,PartitionScheme=huge_app" -p /dev/cu.usbmodem* Page_Turner
```

```bash
bash Page_Turner/test/run_all.sh
```

Arduino IDE에서는 `Partition Scheme = Huge APP`을 고릅니다 (스케치 폴더의 `partitions.csv`가 쓰입니다).

---

## 🗺️ 다음 버전

- iOS 실기 시험 (키보드·볼륨 키는 동작 예상, 휠은 AssistiveTouch 필요할 수 있음)
- V2 하드웨어: 배터리 내장, 잔량 표시
