# ESP32-C3 Ultra-Wide OLED (512x64) Matrix

이 프로젝트는 4개의 128x64 OLED(SSD1306)를 가로로 배치하여 하나의 **512x64 초광폭(Ultra-Wide) 디스플레이**로 제어하는 Arduino 프로젝트입니다. ESP32-C3의 하드웨어 및 소프트웨어 자원을 극한으로 활용하여 부드러운 고속 그래픽 출력을 구현했습니다.

## 🚀 주요 특징 및 성능 최적화

### 1. 하이브리드 I2C 드라이버
*   **HW I2C (Display 1, 2)**: ESP32-C3의 하드웨어 I2C 엔진을 사용하며, 클럭을 **600kHz**까지 상향 조정하여 전송 속도를 최적화했습니다.
*   **High-Speed SW I2C (Display 3, 4)**: 표준 GPIO 함수 대신 **레지스터 직접 제어(Direct Register Access - W1TS/W1TC)** 방식을 적용하여 하드웨어 I2C보다 빠른 **627kHz**의 비트뱅잉 속도를 달성했습니다.

### 2. 지능형 렌더링 엔진 (Smart Flush)
*   **Shadow Buffering**: 512x64 가상 캔버스의 내용을 별도의 그림자 버퍼와 비교하여 **바뀐 부분(Dirty Page)만** I2C로 전송합니다.
*   **LovyanGFX + U8g2 조합**: 고속 그리기 연산은 LovyanGFX가 담당하고, OLED 전송은 최적화된 콜백을 가진 U8g2가 담당하는 하이브리드 구조입니다.

### 3. 안정적인 프레임 제어 (Frame Pacing)
*   **Target FPS (30 FPS)**: 성능이 남을 때 속도를 제어하는 프레임 제한기를 적용하여, 애니메이션이 급격히 빨라지거나 느려지는 현상을 방지했습니다.
*   **smartDelay**: 지연 시간(Pause) 중에도 화면 갱신 엔진을 계속 구동하여 실시간 FPS 수치를 일정하게 유지합니다.
*   **FPS 인터럽트 모니터링**: Ticker 라이브러리를 사용해 1초마다 시리얼 모니터에 정확한 실시간 FPS를 보고합니다 (부하 시 최소 25 FPS 방어).

## 🛠 하드웨어 구성
- **MCU**: ESP32-C3
- **Display**: SSD1306 128x64 OLED x 4
- **핀 맵**:
  - HW I2C: SDA(GPIO 5), SCL(GPIO 6)
  - SW I2C: SDA(GPIO 2), SCL(GPIO 3)

## � 설치가 필요한 라이브러리
이 소스 코드를 컴파일하려면 아두이노 라이브러리 매니저에서 다음 라이브러리들을 설치해야 합니다:
*   **U8g2**: OLED 디스플레이 드라이버 및 폰트 제어
*   **LovyanGFX**: 고속 그래픽 그리기 및 가상 캔버스 라이브러리

## �📺 포함된 데모 (Adafruit Style)
- 가로 512픽셀 전 영역 선 그리기 애니메이션
- 사각형 및 채우기 그래픽 테스트
- 중앙 기준 동심원 및 채우기 테스트
- 둥근 사각형 및 삼각형 데모
- 다양한 폰트 크기 및 512픽셀 너비의 흐르는 자막(Ticker)

## 📈 성능 결과
- **Idle 시**: 고정 30 FPS 유지
- **최대 부하 시 (전체 화면 선 그리기)**: 최소 25 FPS 유지
- **I2C 클럭**: HW 약 600kHz / SW 약 627kHz

---

# ESP32-C3 Ultra-Wide OLED (512x64) Matrix (English)

This project controls four 128x64 OLED (SSD1306) displays arranged horizontally as a single **512x64 Ultra-Wide Display** using Arduino. It pushes the boundaries of ESP32-C3 hardware and software resources to achieve smooth, high-speed graphical output.

## 🚀 Key Features and Performance Optimization

### 1. Hybrid I2C Driver
*   **HW I2C (Display 1, 2)**: Utilizes the ESP32-C3 hardware I2C engine, with the clock speed increased to **600kHz** to optimize transmission speed.
*   **High-Speed SW I2C (Display 3, 4)**: Implements **Direct Register Access (W1TS/W1TC)** on GPIO pins instead of standard library functions, achieving bit-banging speeds of **627kHz**, which is even faster than the hardware I2C.

### 2. Intelligent Rendering Engine (Smart Flush)
*   **Shadow Buffering**: Compares the content of the 512x64 virtual canvas with a separate shadow buffer, transmitting only the **modified pages (Dirty Pages)** via I2C.
*   **LovyanGFX + U8g2 Integration**: A hybrid structure where LovyanGFX handles high-speed drawing operations and U8g2 manages OLED transmission through optimized callbacks.

### 3. Stable Frame Pacing
*   **Target FPS (30 FPS)**: Implements a frame limiter to prevent unnatural speed spikes or drops, ensuring consistent animation feel.
*   **smartDelay**: Keeps the screen refresh engine running during idle periods (pauses) to maintain constant real-time FPS reporting.
*   **FPS Interrupt Monitoring**: Uses the Ticker library to report accurate real-time FPS via serial monitor every second (guaranteed minimum 25 FPS even under high load).

## 🛠 Hardware Configuration
- **MCU**: ESP32-C3
- **Display**: SSD1306 128x64 OLED x 4
- **Pin Mapping**:
  - HW I2C: SDA (GPIO 5), SCL (GPIO 6)
  - SW I2C: SDA (GPIO 2), SCL (GPIO 3)

## 📦 Required Libraries
To compile this project, you need to install the following libraries via the Arduino Library Manager:
*   **U8g2**: OLED display driver and font management
*   **LovyanGFX**: High-speed graphics drawing and virtual canvas library

## 📺 Included Demos (Adafruit Style)
- Full-width (512px) line drawing animations
- Rectangle and filled rectangle graphics tests
- Center-based concentric circle and fill tests
- Round rectangle and triangle demos
- Various font sizes and a scrolling ticker across the full 512px width

## 📈 Performance Results
- **Idle**: Consistent 30 FPS
- **Peak Load (Full-screen line drawing)**: Minimum 25 FPS
- **I2C Clock**: HW ~600kHz / SW ~627kHz
