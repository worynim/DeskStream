# 🎵 DeskStream Chiptune Buzzer (부저 칩튠 테스트)

DeskStream 보드(ESP32-C3)의 부저(GPIO 7)로 4채널 칩튠 음악을 재생하는 테스트 스케치입니다.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

Linus Åkesson의 [Hardware Chiptune](http://www.linusakesson.net/hardware/chiptune.php) 플레이어를 Stimmer가 Arduino로 옮긴 버전([Instructables "Arduino Chiptunes"](https://www.instructables.com/Arduino-Chiptunes/))을 ESP32-C3로 다시 포팅했습니다.

---

## 📌 주요 특징

1. **4채널 소프트웨어 신디사이저**
   - 채널마다 삼각파, 톱니파, 펄스파(듀티 가변), 노이즈 중 하나를 선택
   - 16비트 위상 누산기(DDS) 방식으로 음정 생성
2. **트래커 방식 곡 재생**
   - 비트 단위로 압축된 곡 데이터(2105바이트)를 실시간으로 풀어 재생
   - 악기 스크립트로 아르페지오, 비브라토, 피치 벤드, 포르타멘토, 볼륨 페이드, PWM 음색 변조 지원
3. **PWM 오디오 출력**
   - LEDC PWM 78.125kHz(8비트) 캐리어로 부저를 구동해 간이 8비트 DAC처럼 사용
   - 부저 코일과 진동판이 PWM 평균값을 따라 움직이는 원리
4. **곡 종료/정지 시 출력 차단**
   - 소리가 없을 때 PWM duty를 0으로 내려 부저 코일에 불필요한 전류가 흐르지 않게 함

---

## 🛠️ 하드웨어 구성

- **MCU**: ESP32-C3 (RISC-V Single-core, 160MHz)
- **Buzzer**: 패시브 부저 (GPIO 7 → 1kΩ → NPN 트랜지스터 → 100Ω → 부저 → V_IN)
- **Buttons**:
  - **BTN 1 (GPIO 1)**: 처음부터 재생
  - **BTN 2 (GPIO 4)**: 정지
- OLED는 사용하지 않습니다.

---

## 🎮 조작 가이드

| 입력 | 동작 |
| :--- | :--- |
| 전원 인가 / 리셋 | 자동으로 재생 시작 |
| **BTN 1** | 처음부터 다시 재생 |
| **BTN 2** | 재생 정지 |

시리얼 모니터(115200bps)에 `play` / `stop` / `end` 상태가 출력됩니다.

---

## ⚙️ 동작 구조

```
[하드웨어 타이머 ISR, 약 15.9kHz]         [loop(), 약 88Hz 틱]
 ├ 직전 샘플을 LEDC duty로 출력            ├ 곡 순서표 / 트랙 해석 (5틱마다 한 줄)
 ├ 32비트 LFSR 노이즈 갱신                 ├ 악기 스크립트 실행
 ├ 4채널 파형 합성 → 8비트 샘플            └ 주파수·볼륨·듀티·비브라토 갱신
 └ 180샘플마다 tickCount++ ─────────────→ playroutine() 호출
```

| 항목 | 값 |
| :--- | :--- |
| 샘플레이트 | 1MHz 타이머 / 63 ≈ **15,873Hz** |
| 플레이어 틱 | 샘플레이트 / 180 ≈ **88Hz** |
| 곡 진행 | 5틱마다 1줄, 트랙당 32줄, 곡 55구간 (약 101초) |
| PWM | 80MHz / 256 / 4 = **78,125Hz**, 8비트 |

---

## 🔁 원본(AVR) 대비 변경점

| 원본 (Arduino Uno) | 이 스케치 (ESP32-C3) |
| :--- | :--- |
| Timer0 CTC 인터럽트 | `timerBegin()` 하드웨어 타이머 ISR |
| Timer2 Fast PWM → `OCR2B` (pin 3) | LEDC PWM (GPIO 7). ISR에서는 `hal/ledc_ll.h`로 레지스터를 직접 갱신 |
| `typedef char s8;` | `typedef int8_t s8;` (RISC-V는 `char`가 unsigned라 그대로 두면 음정·효과가 깨짐) |
| `timetoplay++ / --` 공유 변수 | ISR은 `tickCount`만 증가, loop는 `ticksDone`과 비교 (경합 제거) |
| `main()`, `cli/sei`, `CLKPR`, `DDRx` | `setup()/loop()`로 변경, AVR 레지스터 코드 삭제 |
| `PORTB`로 LED 깜빡임 (pin 10, 12) | 삭제 |
| 곡 종료 시 마지막 샘플값 유지 | 1.5초 후 PWM duty 0으로 출력 차단 |

추가로 범위를 벗어난 명령 번호나 음표 번호를 막는 방어 코드와, 악기 스크립트 무한 루프 방지(틱당 최대 64명령)를 넣었습니다.

---

## 🔧 조정 가능한 설정 (`Chiptune_Buzzer.ino` 상단)

| 상수 | 기본값 | 설명 |
| :--- | :--- | :--- |
| `TICK_SAMPLES` | 180 | 템포. 값을 키우면 느려짐 |
| `SAMPLE_PERIOD` | 63 | 샘플 주기(µs). 바꾸면 음정도 함께 변함 |
| `PWM_FREQ` | 78125 | PWM 캐리어 주파수 |
| `BUZZER_PIN` / `BTN_PLAY_PIN` / `BTN_STOP_PIN` | 7 / 1 / 4 | 핀 설정 |

---

## 🔊 음질 관련 참고

- 부저가 2.7kHz 공진형이라 **저음(베이스 채널)은 약하고**, 2~4kHz 대역이 강조된 날카로운 소리가 납니다.
- 멜로디와 아르페지오는 잘 들리지만 원곡보다 가볍게 들립니다.
- 단일 코어라서 다른 프로젝트와 합칠 경우, ISR이 SW I2C 전송 사이에 끼어들어 화면 fps가 약간 떨어질 수 있습니다. Wi-Fi 사용 시 인터럽트 지연으로 잡음이 생길 수 있습니다.

---

## 📁 파일 구성

- `Chiptune_Buzzer.ino`: 사운드 ISR, 곡 플레이어, 버튼 처리
- `song_data.h`: 곡 데이터(PROGMEM)와 `TRACKLEN` / `MAXTRACK` / `SONGLEN` 정의

---

## 🧪 빌드

- 보드: `esp32:esp32:makergo_c3_supermini` (Arduino-ESP32 코어 3.x)
- 외부 라이브러리 없음

```bash
arduino-cli compile --fqbn esp32:esp32:makergo_c3_supermini Chiptune_Buzzer
```

---

## 📜 출처

- 플레이어 원작: Linus Åkesson, Hardware Chiptune — http://www.linusakesson.net/hardware/chiptune.php
- Arduino 포트: Stimmer (Arduino 포럼), Instructables "Arduino Chiptunes" by blinkyblinky
