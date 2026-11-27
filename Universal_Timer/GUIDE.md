**English** | [한국어](GUIDE.ko.md)

# Universal Timer Detailed Guide - v1.3.0

The detailed companion to the [README](README.md).

## 📌 Key Features

1. **4-way interactive UI**:
   - **OLED 1**: Shows hours (00 – 99); press BTN 1 to add 1
   - **OLED 2**: Shows minutes (00 – 59); press BTN 2 to add 1
   - **OLED 3**: Shows seconds (00 – 59); press BTN 3 to add 1
   - **OLED 4**: Shows the START / PAUSE / RESET state; control the timer with BTN 4
2. **Auto-repeat acceleration**:
   - Holding a button (BTN 1, 2, 3) increments the number rapidly and continuously.
3. **Intuitive start / pause / reset (BTN 4)**:
   - **Short press**: Start (START) or pause (PAUSE) the countdown
   - **Long press (1 second)**: Reset everything to 00:00:00 (RESET)
4. **Visual and audio feedback**:
   - A short, crisp beep on every button press
   - When the countdown ends, OLED 4 blinks inverted and a music-box arpeggio alarm plays repeatedly
   - Alarm and boot melodies lower the PWM duty from 50% to 3% so they ring out like a bell (v1.2.0)
   - Pressing any button during the alarm stops it immediately and restores the time that was first STARTed (even if paused in between)
6. **Remembers the last set time (v1.3.0)**:
   - After changing the time with the hour/minute/second buttons, pressing START saves that time to NVS, and the timer starts with it after the next power-on
   - Resuming after a pause does not save (the remaining time is not saved)
5. **Fast dual I2C architecture**:
   - Adopts the proven fast I2C approach from `Sound_Visualizer_4_OLEDs`
   - **OLED 1, 2**: Hardware I2C with the ESP-IDF native driver (GPIO 5, 6)
   - **OLED 3, 4**: Fast software I2C with direct W1TS/W1TC register control (GPIO 2, 3)

---

## 🛠️ Hardware

- **MCU**: ESP32-C3 (RISC-V single-core, 3.3V)
- **Display**: 0.96 inch SSD1306 128×64 OLED × 4 (I2C)
  - **HW I2C (OLED 1, 2)**: SDA (GPIO 5), SCL (GPIO 6)
  - **SW I2C (OLED 3, 4)**: SDA (GPIO 2), SCL (GPIO 3)
- **Buttons**:
  - **BTN 1 (GPIO 1)**: Adjust hours
  - **BTN 2 (GPIO 4)**: Adjust minutes
  - **BTN 3 (GPIO 10)**: Adjust seconds
  - **BTN 4 (GPIO 9)**: Start / pause / reset
- **Buzzer**: Passive buzzer (GPIO 7)

---

## 🎮 Controls

| Input | Short Press | Hold |
| :--- | :--- | :--- |
| **BTN 1 (OLED 1)** | Hour +1 | Fast continuous hour increase |
| **BTN 2 (OLED 2)** | Minute +1 | Fast continuous minute increase |
| **BTN 3 (OLED 3)** | Second +1 | Fast continuous second increase |
| **BTN 4 (OLED 4)** | Start (START) / pause (PAUSE) the timer / stop the alarm | **Reset everything to 00:00:00 (RESET)** |

---

## 📁 Project Files

- `Universal_Timer.ino`: Main loop, button dispatch and screen rendering
- `TimerLogic.h / .cpp`: Business logic for hour/minute/second increments and the countdown
- `BellBuzzer.h / .cpp`: Non-blocking buzzer with duty decay (beep, boot melody, alarm). `ds::Buzzer` uses `tone()`, which fixes the duty at 50%, so this lives inside the project
- `config.h`: Global constants for timer, button timing and buzzer (the pin map is in DeskStreamCore `ds_board.h`)

Display transfer (`ds::DisplayCore`) and button auto-repeat (`ds::Buttons`) use
the shared library [DeskStreamCore](../libraries/DeskStreamCore/) — run `bash scripts/link-arduino-lib.sh` once from the repository root before building (v1.1.0).

---

## 🚀 Build and Upload

```bash
arduino-cli compile --fqbn esp32:esp32:nologo_esp32c3_super_mini .
arduino-cli upload  --fqbn esp32:esp32:nologo_esp32c3_super_mini -p /dev/tty.usbmodem* .
```
