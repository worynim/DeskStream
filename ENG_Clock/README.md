**English** | [한국어](README.ko.md)

# ENG Clock (English Language Clock) - v1.1.1

A clock that syncs time over NTP and shows hours, minutes and seconds as **English words** on four OLEDs. It keeps the structure of `Hangeul_Clock` and replaces only the text with English.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ Key Features

- **English word / numeric display**: long words (`SEVENTEEN`) are split into two lines by word
- **12H (AM/PM) and 24H formats**, English weekdays (`MONDAY`)
- **Automatic time zone and DST**: 12 regional presets or custom input on the web
- **Font Studio**: pick a TTF in the browser and upload it as 64px bitmaps, 5 font slots
- 5 animations, hourly chime

## 🎮 Buttons

| Button | Short | Long |
| :--- | :--- | :--- |
| BTN 1 | Hourly chime ON/OFF | Screen flip (180°) |
| BTN 2 | Word ↔ numeric | 12H ↔ 24H |
| BTN 3 | Cycle animation | Cycle font slot (0–4) |
| BTN 4 | Clock → IP → help | Color invert |

## 🚀 Setup

1. **Flash the firmware** — pick it in the [web flasher](https://worynim.github.io/DeskStream/), or build it yourself.
   In the Arduino IDE choose **Huge APP** for `Partition Scheme`.

   ```bash
   arduino-cli compile --fqbn esp32:esp32:makergo_c3_supermini:PartitionScheme=huge_app .
   ```

2. **WiFi setup** — on first boot, connect to the `ENG_Clock_Setup` AP and enter your WiFi details.
3. **Upload a font (required)** — without a font only numeric mode is shown (`Font Required!`). Check the IP with BTN 4, open it and upload in Font Studio.

## 📖 More

Feature details, directory structure, build and test instructions are in [GUIDE.md](GUIDE.md).

---
© 2026 DeskStream Project. — worynim@gmail.com
