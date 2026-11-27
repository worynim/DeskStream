**English** | [한국어](README.ko.md)

# ESP32-C3 4-OLED GPS Dashboard (GPS_DASH) - v2.8.0

A dashboard that shows GPS information on the four OLEDs from a connected NEO-6M GPS module. No WiFi or web features — it focuses on GPS display only.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ Key Features

- **8 screens**: clock, speedometer, compass, satellite status (DOP), latitude/longitude, altitude, calendar, trip statistics
- Choose what each screen shows with its button; settings are kept after power-off
- 5Hz GPS, shows time even before a position fix, sounds on satellite lock/loss

## 🎮 Buttons

| Button | Short | Long |
| :--- | :--- | :--- |
| BTN 1 | Cycle OLED 1 screen | Cycle whole-display orientation |
| BTN 2 | Cycle OLED 2 screen | Button help on/off |
| BTN 3 | Cycle OLED 3 screen | Reset trip statistics |
| BTN 4 | Cycle OLED 4 screen | Demo mode on/off |

## 🚀 Setup

1. Connect the GPS module: RX (GPIO 20), TX (GPIO 21).
2. Pick it in the [web flasher](https://worynim.github.io/DeskStream/), or build it yourself (needs `U8g2`, `Adafruit GPS Library` and DeskStreamCore).

## 📖 More

Feature details, hardware wiring, software setup and file structure are in [GUIDE.md](GUIDE.md).
