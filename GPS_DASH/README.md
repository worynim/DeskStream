**English** | [한국어](README.ko.md)

# ESP32-C3 4-OLED GPS Dashboard (GPS_DASH)

A hardware dashboard that visualizes real-time GPS information using an **ESP32 C3 SuperMini** board and four OLED displays. It was refactored to remove all data collection and web server features and focus solely on **hardware control and GPS data display**.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## 🚀 Key Features

1.  **Real-time GPS visualization (cycles through 8 modes)**
    - **Integrated dashboard**: Place the information you want independently on each of the 4 OLEDs.
    - **Modes**: Clock (with date), speedometer, compass, satellite status (DOP), latitude/longitude, altitude, calendar (grid), trip statistics (timer/distance).
2.  **Independent screen control (4-point buttons)**
    - The button below each OLED changes what that screen shows independently.
3.  **Hybrid I2C architecture**
    - **OLED 1, 2 (Hardware I2C)**: SDA(5), SCL(6)
    - **OLED 3, 4 (Software I2C)**: SDA(2), SCL(3)
4.  **GPS interface (UART)**
    - Receives 5Hz high-rate data from a **NEO-6M** module on pins 20 (RX) and 21 (TX).
    - **GPS reliability optimizations**:
        - **Direct UBX binary control**: Uses u-blox UBX messages (CFG-MSG, CFG-RATE) instead of NMEA commands.
        - **No double initialization**: Removing the conflict with the library's internal initialization keeps the pin mapping stable.
        - **Direct GSV parsing**: Parses and shows in real time the 'total number of satellites in view', which the standard library does not provide.
        - **Time first**: Updates the time as soon as satellite signals are detected, even before a position fix.
5.  **Non-volatile settings (NVS)**
    - Each screen's display mode and inversion setting are saved in internal memory and restored automatically after a reboot.

## 🛠 Hardware Components

-   **Main Board**: ESP32 C3 SuperMini
-   **Display**: 0.96 inch SSD1306 OLED (128x64) x 4
-   **GPS Module**: **U-blox NEO-6M** (initialized at 9600bps, then fixed at 5Hz)
- **Layout**: Short press switches modes; holding for more than 1 second runs a special function.
- **Audio feedback**: The buzzer (GPIO 7) plays various melodies for system state and button input.
    - **System start**: A rising "dee-ree-ree-ring~" startup sound (4 ascending steps).
    - **GPS Lock**: A pleasant rising tone when a satellite fix is acquired.
    - **GPS Unlock**: A falling warning tone when the satellite signal is lost.
    - **Button input**: Distinct tones for short press (3000Hz) and long press (2000Hz).
- **Pins and functions**:
    - **BTN 1 (Pin 1)**: (Short) cycle OLED 1 mode / (Long) loop through whole-screen orientation (Normal/Mirror/180/Flip).
    - **BTN 2 (Pin 4)**: (Short) cycle OLED 2 mode / (Long) toggle **button help (INFO)** mode.
    - **BTN 3 (Pin 10)**: (Short) cycle OLED 3 mode / (Long) reset trip statistics.
    - **BTN 4 (Pin 9)**: (Short) cycle OLED 4 mode / (Long) toggle demo simulation mode.
    - **Wiring summary**: GPS UART: RX(20), TX(21) / I2C: SDA(5/2), SCL(6/3) / Buzzer(7).

## ⚙️ Software Setup

1.  **Required libraries**:
    - `U8g2`
    - `Adafruit GPS Library` (for GPS parsing)
    - Shared library [DeskStreamCore](../libraries/DeskStreamCore/) — run `bash scripts/link-arduino-lib.sh` once from the repository root before building (display transfer, buttons, buzzer, v2.7.0)
2.  **Board settings**:
    - Board: `ESP32C3 Dev Module`
    - USB CDC On Boot: `Enabled` (for debug logs)

## 📁 Files

- `GPS_DASH.ino`: Main control loop, button input handling and trip statistics.
- `display_handler.h`: Initialization of the 4 OLEDs, rendering engine for the 8 screen types and UI graphics.
- `gps_handler.h`: Dedicated module for GPS data reception, UBX configuration and NMEA/GSV parsing.
- `ota_handler.h`: WiFi AP mode, OTA firmware updates and real-time debug log.

---
