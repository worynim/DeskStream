**English** | [한국어](GUIDE.ko.md)

# OLED_Wide_test Detailed Guide - v1.3.1

The detailed companion to the [README](README.md).

## 🚀 Key Features and Performance Optimizations

### 1. Hybrid I2C Driver
*   **HW I2C (Display 1, 2)**: Uses the ESP32-C3's hardware I2C engine with a raised clock to optimize transfer speed.
*   **High-Speed SW I2C (Display 3, 4)**: Uses **direct register access (W1TS/W1TC)** instead of the standard GPIO functions.

### 2. Smart Rendering Engine (Smart Flush)
*   **Shadow buffering**: Compares the 512x64 virtual canvas with a separate shadow buffer and sends **only the changed parts (dirty pages)** over I2C.
*   **LovyanGFX + DeskStreamCore**: LovyanGFX handles drawing, and DeskStreamCore `ds::DisplayCore::pushCanvas()` handles transposition and parallel transfer of only changed tiles to the 4 screens (v1.3.0).

### 3. Stable Frame Pacing
*   **Target FPS (30 FPS)**: A frame limiter controls speed when there is spare performance, preventing animations from suddenly speeding up or slowing down.
*   **smartDelay**: Keeps the screen update engine running even during pauses, so the real-time FPS stays steady.
*   **FPS interrupt monitoring**: Uses the Ticker library to report the exact real-time FPS to the serial monitor every second.

## 🛠 Hardware
- **MCU**: ESP32-C3
- **Display**: SSD1306 128x64 OLED x 4
  - HW I2C: SDA(GPIO 5), SCL(GPIO 6)
  - SW I2C: SDA(GPIO 2), SCL(GPIO 3)
  - **BTN 1 (Pin 1)**: Rotate the screen 180 degrees (flip)
  - **Buzzer (Pin 7)**: Input feedback

## 📦 Required Libraries
To compile this source code, install the following libraries from the Arduino Library Manager:
*   **U8g2**: OLED display driver and font control
*   **DeskStreamCore**: Repository `libraries/DeskStreamCore` — 4-OLED transfer, buttons, buzzer (run `bash scripts/link-arduino-lib.sh` once from the root before building) (v1.3.0+)
*   **LovyanGFX**: Fast drawing and virtual canvas library

## 🕹️ Controls and Buttons (Interface)
This project supports real-time settings and haptic-style feedback.

*   **BTN 1 (GPIO 1)**:
    *   **Short press**: Toggle 180-degree screen rotation (flip).
    *   **Smart swap**: When the screen flips, the OLED transfer order is swapped automatically (`1-2-3-4` ↔ `4-3-2-1`) to keep horizontal graphics continuous.
*   **Audio feedback (buzzer)**:
    *   A short 3000Hz beep plays on input.
    *   The setting is saved automatically to non-volatile memory (NVS) and restored after a reboot.

## 📺 Included Demos (Adafruit Style)
- Line drawing animation across the full 512-pixel width
- Rectangle and fill graphics test
- Centered concentric circles and fill test
- Rounded rectangle and triangle demos
- Various font sizes and a 512-pixel-wide scrolling ticker
