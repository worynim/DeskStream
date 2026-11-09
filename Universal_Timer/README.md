# ⏱️ DeskStream Universal Timer (범용 타이머)

DeskStream 보드(ESP32-C3)와 가로로 배치된 4개의 SSD1306 OLED 디스플레이, 그리고 하단 버튼을 활용한 직관적인 4채널 하드웨어 범용 카운트다운 타이머입니다.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

---

## 📌 주요 특징

1. **4분할 인터랙티브 UI**:
   - **OLED 1**: 시 (00 ~ 99) 표시 및 BTN 1을 누르면 +1 증가
   - **OLED 2**: 분 (00 ~ 59) 표시 및 BTN 2를 누르면 +1 증가
   - **OLED 3**: 초 (00 ~ 59) 표시 및 BTN 3를 누르면 +1 증가
   - **OLED 4**: START / PAUSE / RESET 상태 표시 및 BTN 4를 통해 타이머 제어
2. **자동 가속 반복 입력 (Auto Repeat)**:
   - 버튼(BTN 1, 2, 3)을 길게 누르고 있으면 숫자가 빠르게 연속 증가합니다.
3. **직관적인 시작 / 일시정지 / 초기화 (BTN 4)**:
   - **Short Press**: 카운트다운 시작(START) 또는 일시정지(PAUSE)
   - **Long Press (1초)**: 00시 00분 00초로 전체 초기화(RESET)
4. **시각 및 청각 피드백**:
   - 버튼 클릭 시마다 경쾌하고 짧은 비프음 출력
   - 카운트다운 종료 시 4번 OLED 화면 반전 점멸 및 경고 알람음 연속 출력
   - 알람 발생 중 아무 버튼이나 누르면 즉시 알람 해제
5. **고속 듀얼 I2C 아키텍처**:
   - `Sound_Visualizer_4_OLEDs`의 검증된 고속 I2C 방식을 채택
   - **OLED 1, 2**: ESP-IDF Native 드라이버 기반 Hardware I2C (GPIO 5, 6)
   - **OLED 3, 4**: W1TS/W1TC 레지스터 직접 제어 고속 Software I2C (GPIO 2, 3)

---

## 🛠️ 하드웨어 구성

- **MCU**: ESP32-C3 (RISC-V Single-core, 3.3V)
- **Display**: 0.96 inch SSD1306 128×64 OLED × 4개 (I2C)
  - **HW I2C (OLED 1, 2)**: SDA (GPIO 5), SCL (GPIO 6)
  - **SW I2C (OLED 3, 4)**: SDA (GPIO 2), SCL (GPIO 3)
- **Buttons**:
  - **BTN 1 (GPIO 1)**: 시 (Hour) 조절
  - **BTN 2 (GPIO 4)**: 분 (Minute) 조절
  - **BTN 3 (GPIO 10)**: 초 (Second) 조절
  - **BTN 4 (GPIO 9)**: 시작 / 일시정지 / 리셋
- **Buzzer**: 패시브 부저 (GPIO 7)

---

## 🎮 조작 가이드

| 입력 위치 | 짧게 누름 (Short Press) | 누르고 있기 (Hold) |
| :--- | :--- | :--- |
| **BTN 1 (OLED 1)** | 시(Hour) +1 | 시(Hour) 고속 연속 증가 |
| **BTN 2 (OLED 2)** | 분(Minute) +1 | 분(Minute) 고속 연속 증가 |
| **BTN 3 (OLED 3)** | 초(Second) +1 | 초(Second) 고속 연속 증가 |
| **BTN 4 (OLED 4)** | 타이머 시작(START) / 일시정지(PAUSE) / 알람 끄기 | **00:00:00 전체 초기화 (RESET)** |

---

## 📁 프로젝트 파일 구성

- `Universal_Timer.ino`: 메인 루프, 버튼 분기 처리 및 화면 렌더링
- `TimerLogic.h / .cpp`: 시/분/초 증가 및 카운트다운 비즈니스 로직
- `config.h`: 타이머/버튼 타이밍/부저 관련 전역 상수 정의 (핀맵은 DeskStreamCore `ds_board.h`)

화면 전송(`ds::DisplayCore`)·버튼 자동 반복(`ds::Buttons`)·비차단 비프·알람(`ds::Buzzer`)은
공통 라이브러리 [DeskStreamCore](../libraries/DeskStreamCore/) — 빌드 전 저장소 루트에서 `bash scripts/link-arduino-lib.sh` 1회를 씁니다 (v1.1.0).

---

## 🚀 빌드 및 업로드

```bash
arduino-cli compile --fqbn esp32:esp32:nologo_esp32c3_super_mini .
arduino-cli upload  --fqbn esp32:esp32:nologo_esp32c3_super_mini -p /dev/tty.usbmodem* .
```

