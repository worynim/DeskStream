**English** | [한국어](README.ko.md)

# Chinese Clock (Chinese Word Clock) - v1.1.1

A clock that syncs time over NTP and shows hours, minutes and seconds in **Chinese characters** on four OLEDs. It keeps the structure of `Hangeul_Clock` / `ENG_Clock` and replaces only the text with Chinese.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ Key Features

- **4-screen display**: AM/PM (or weekday) · hour · minute · second, one per screen; at most 4 characters, so no line breaks
- **Standard Chinese expressions**: `两点`, `三点零五分`, `三点半`, `九点整`, `上午`/`下午`, `星期日`–`星期六`
- **Simplified / Traditional / numeric switching**: `简体 → 繁體 → 數字`
- **Font Studio**: pick a TTF in the browser and upload it to the device, 5 font slots
- **Automatic time zone and DST** (default `CST-8`), 6 animations, web settings page

## 🎮 Buttons

| Button | Short | Long |
|:--|:--|:--|
| BTN1 | Hourly chime ON/OFF | Screen flip (180°) |
| BTN2 | Display mode 简体 → 繁體 → 數字 | 12H ⇄ 24H |
| BTN3 | Cycle animation (6 kinds) | Cycle font slot (0–4) |
| BTN4 | Switch page (clock → IP → help) | Color invert |

## 🚀 Setup

1. **Flash the firmware** — pick it in the [web flasher](https://worynim.github.io/DeskStream/), or build it yourself.
   In the Arduino IDE choose **Huge APP** for `Partition Scheme`.

   ```bash
   arduino-cli compile --fqbn esp32:esp32:makergo_c3_supermini:PartitionScheme=huge_app .
   ```

2. **Upload a font (required)** — without a font, Chinese characters are not shown. Open the device IP (BTN4 short) and
   upload a CJK font that covers both Simplified and Traditional (Noto Sans SC, Source Han Sans, etc.).

## 📖 More

Screen layout, expression rules, the web API, tests, memory and known limitations are in [GUIDE.md](GUIDE.md).
For the step-by-step font upload, see [Font Upload Procedure](GUIDE.md#-font-upload-procedure).

---
© 2026 DeskStream Project. — worynim@gmail.com
