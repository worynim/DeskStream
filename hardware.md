# 🛠 Hardware Specification: DeskStream board

DeskStream board 하드웨어 구성 및 핀 맵 정보입니다. 새로운 프로젝트 시작 시나 배선 확인용으로 참조하세요.

## 1. 메인 컨트롤러 (Controller)
| 항목 | 사양 | 비고 |
| :--- | :--- | :--- |
| **MCU** | **ESP32-C3** | RISC-V Single-core |
| **Operating Voltage** | 3.3V | 모든 로직 레벨 동일 |

## 2. 디스플레이 모듈 (Display)
4개의 OLED를 제어하기 위해 2개의 I2C 버스(HW 1개, SW 1개)를 사용합니다.

| 모듈 ID | 모델 | 인터페이스 | SDA (Pin) | SCL (Pin) | I2C Addr | 비고 |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **OLED 1** | SSD1306 | HW I2C | **GPIO 5** | **GPIO 6** | 0x3C |
| **OLED 2** | SSD1306 | HW I2C | **GPIO 5** | **GPIO 6** | 0x3D |
| **OLED 3** | SSD1306 | SW I2C | **GPIO 2** | **GPIO 3** | 0x3C |
| **OLED 4** | SSD1306 | SW I2C | **GPIO 2** | **GPIO 3** | 0x3D |


## 3. 사용자 입력 (Buttons)
모든 버튼은 내부 풀업(`INPUT_PULLUP`)을 사용하며, GND와 연결 시 동작합니다.

| 버튼 ID | 연결 핀 (GPIO) | 
| :--- | :--- | :--- |
| **BTN 1** | **GPIO 1** | 
| **BTN 2** | **GPIO 4** | 
| **BTN 3** | **GPIO 10** |
| **BTN 4** | **GPIO 9** | 


## 📍 전체 핀 맵 요약 (Pin Map Summary)
- **D0 (GPIO 0)**: ADC
- **D1 (GPIO 1)**: BTN 1
- **D2 (GPIO 2)**: SW I2C SDA (OLED 3, 4)
- **D3 (GPIO 3)**: SW I2C SCL (OLED 3, 4)
- **D4 (GPIO 4)**: BTN 2
- **D5 (GPIO 5)**: HW I2C SDA (OLED 1, 2)
- **D6 (GPIO 6)**: HW I2C SCL (OLED 1, 2)
- **D7 (GPIO 7)**: Buzzer
- **D8 (GPIO 8)**: (사용 안 함/on board LED)
- **D9 (GPIO 9)**: BTN 4
- **D10 (GPIO 10)**: BTN 3
- **TX (GPIO 21)**: uart TX
- **RX (GPIO 20)**: uart RX
