**English** | [한국어](GUIDE.ko.md)

# Game of Life Detailed Guide - v1.1.1

The detailed companion to the [README](README.md).

## 🚀 Key Features

- **Large-screen simulation**: Drives four 128x64 OLEDs in parallel for a 512x64 resolution.
- **Memory optimization**: 1-bit bitmasking keeps ESP32-C3 memory use minimal (8KB total).
- **Fast rendering**: Dirty tile tracking that sends only changed tiles, plus asynchronous parallel I2C transfers.
- **Standalone**: Runs on device power alone, without Wi-Fi or a PC.
- **Physical control**: Control the simulation in real time with the four buttons.
- **Visual/audio feedback**: Boot melody, splash screen and button beeps.

## 🛠 Hardware

- **MCU**: ESP32-C3 (RISC-V)
- **Display**: 128x64 SSD1306 OLED x 4 (512x64 total)
- **Input**: Tactile button x 4 (GPIO 1, 4, 10, 9)
- **Audio**: Active/passive buzzer (GPIO 7)

## 🎮 Controls

| Button | Function | Description |
| :--- | :--- | :--- |
| **BTN 1** | **Play / Pause** | Stops or resumes the simulation. |
| **BTN 2** | **Single step** | Works only while paused; advances one generation per press. |
| **BTN 3** | **Speed** | Switches the simulation speed (5fps ➔ 10fps ➔ 20fps). |
| **BTN 4** | **Reset pattern** | Clears the current state and generates a new random cell pattern. |

## 📦 Installation and Build

1.  **Required libraries**: Install `U8g2` from the Arduino IDE Library Manager, and link the shared library [DeskStreamCore](../libraries/DeskStreamCore/) by running `bash scripts/link-arduino-lib.sh` once from the repository root before building. Display transfer, buttons and buzzer use this library (v1.1.0).
2.  **Board settings**: Select `ESP32C3 Dev Module`.
3.  **Compile and upload**: Open `Game_Of_Life.ino` and upload it.

---
Developed by Antigravity AI for DeskStream Project.
