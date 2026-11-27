**English** | [한국어](README.ko.md)

<img width="778" height="211" alt="image" src="https://github.com/user-attachments/assets/0eabecfa-ce8a-4527-8e50-6d8101b9150c" />

<img width="879" height="1067" alt="image" src="https://github.com/user-attachments/assets/325bc0a3-bf28-45f6-92ca-6e8ce69a8893" />

# Hangeul Clock (Korean Clock) - v2.11.1

A clock that syncs time over NTP and shows hours, minutes and seconds in high-quality Korean (Hangeul) or numerals on four OLEDs.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ Key Features

- **Korean / numeric display**, 12H (AM/PM) and 24H (with date) formats
- **64px high-quality fonts**: pick a TTF in the web Font Studio and upload it directly, 5 font slots
- Several **digit transition animations**, hourly chime melody, brightness control
- **Web dashboard**: change settings with a live preview; settings changed by button show up within 3 seconds

## 🎮 Buttons

| Button | Short | Long (1 s) |
| :--- | :--- | :--- |
| BTN 1 | Hourly chime ON/OFF | Screen flip (180°) |
| BTN 2 | Korean ↔ numeric | 12H ↔ 24H |
| BTN 3 | Cycle animation | Cycle font slot (0–4) |
| BTN 4 | Clock → IP → help | Color invert |

Holding **BTN 1** during boot clears the WiFi settings and enters setup mode.

## 🚀 Setup

1. **Flash the firmware** — pick it in the [web flasher](https://worynim.github.io/DeskStream/), or build it yourself.
   In the Arduino IDE choose **Huge APP** for `Partition Scheme`.
2. **WiFi setup** — on first boot, connect to the `Hangeul_Clock_Setup` AP and enter your WiFi details.
3. **Upload a font** — check the IP with BTN 4, open it in a browser and upload a font in Font Studio.

## 📖 More

Feature details, file structure, hardware and build settings are in [GUIDE.md](GUIDE.md).

---
© 2026 DeskStream Project.
