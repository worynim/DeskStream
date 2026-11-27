**English** | [한국어](README.ko.md)

# 🖥️ DeskStream: ESP32-C3 4-OLED Multifunctional Board

DeskStream is a multifunctional desktop information station built on an ESP32-C3 and four OLED displays.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

> 🌐 **Web Flasher**: [https://worynim.github.io/DeskStream/](https://worynim.github.io/DeskStream/) — upload any project straight from the browser to an ESP32-C3 board connected over USB. ([How to use](GUIDE.md#-web-flasher) · [Source](./web/))

## 🛠️ Hardware Specification
- **MCU**: ESP32-C3 (SuperMini)
- **Display**: 0.96" SSD1306 OLED (128x64) x 4 (side by side they form a 512x64 wide display)
- **Buttons**: 4 user input buttons (GPIO 1, 4, 10, 9)
- **Interface**:
  - **HW I2C**: SDA(GPIO 5), SCL(GPIO 6) - drives OLED 1, 2
  - **SW I2C**: SDA(GPIO 2), SCL(GPIO 3) - drives OLED 3, 4
- Details: see [hardware.md](./hardware.md)

## 📁 Sub-Projects
16 programs for the DeskStream board. The numbers match the web flasher and launcher list (0 is the launcher).
Click a name to open that project's README.

| # | Project | Description |
|---|---|---|
| | **Clocks & time** | |
| 1 | [Hangeul_Clock](./Hangeul_Clock/) (v2.11.1) | High-quality font Korean word clock |
| 2 | [ENG_Clock](./ENG_Clock/) (v1.1.1) | High-quality font English word clock |
| 3 | [Chinese_Clock](./Chinese_Clock/) (v1.1.1) | High-quality font Chinese character clock (Simplified/Traditional) |
| 4 | [World_Clock](./World_Clock/) (v0.1.0) | Four world cities, one per screen |
| 5 | [Universal_Timer](./Universal_Timer/) (v1.3.0) | Countdown timer with hours/minutes/seconds set per screen |
| | **Information & measurement** | |
| 6 | [Smart_Info_Station](./Smart_Info_Station/) (v2.14.1) | Weather, air quality, stocks, exchange rates, YouTube and more |
| 7 | [GPS_DASH](./GPS_DASH/) (v2.8.0) | GPS dashboard with speed, compass and altitude |
| 8 | [Wireless_Analyzer](./Wireless_Analyzer/) (v0.5.1) | 2.4GHz channel congestion, AP and BLE device analyzer |
| | **PC companions** | |
| 9 | [Stats_Monitor](./Stats_Monitor/) (v1.6.1) | PC/Mac CPU, GPU, RAM and temperature monitor |
| 10 | [Screen_To_OLEDs](./Screen_To_OLEDs/) (v2.3.1) | Streams part of the Mac screen to 512x64 in real time |
| 11 | [Agent_Monitor](./Agent_Monitor/) (v0.8.2) | Claude Code session status and permission approval |
| | **Bluetooth input** | |
| 12 | [DS_DECK](./DS_DECK/) (v1.6.1) | Four-button BLE macro pad |
| 13 | [Page_Turner](./Page_Turner/) (v1.1.0) | BLE page turner for web novels, webtoons, Shorts and slides |
| | **Graphics & demos** | |
| 14 | [Sound_Visualizer_4_OLEDs](./Sound_Visualizer_4_OLEDs/) (v3.6.0) | 128-band audio visualizer from a microphone |
| 15 | [Game_Of_Life](./Game_Of_Life/) (v1.1.1) | Game of Life on 512x64 |
| 16 | [OLED_Wide_test](./OLED_Wide_test/) (v1.3.1) | Demo that drives the four screens as one 512x64 screen |

## 🚀 Getting Started

- **Just use it**: pick a project in the [web flasher](https://worynim.github.io/DeskStream/) and flash it (Chrome/Edge desktop, Android Chrome 148+).
- **Build it yourself**: every project uses the shared library [DeskStreamCore](./libraries/DeskStreamCore/). Once per PC, run the command below from the repository root and install **U8g2** from the Arduino IDE Library Manager.

  ```bash
  bash scripts/link-arduino-lib.sh
  ```

For Windows setup, the launcher, the firmware release process and other details, see [GUIDE.md](GUIDE.md).
When creating a new project with AI, have it read [`AI_GUIDE.md`](./AI_GUIDE.md) first.

---

## 📜 License

| Scope | License | File |
|---|---|---|
| Hardware design (schematic [`DeskStream_Schematic.pdf`](./DeskStream_Schematic.pdf), [`hardware.md`](./hardware.md)) | CERN-OHL-S-2.0 | [`LICENSE-HARDWARE`](./LICENSE-HARDWARE) |
| Software (firmware, the DeskStreamCore library, PC clients, the web flasher, scripts, and everything else) | MIT | [`LICENSE`](./LICENSE) |

- Bundled third-party font data (converted from Galmuri7, Noto Sans KR/SC, Source Han Sans, Roboto Condensed, Nanum and others) remains under each font's original license. Galmuri7 is under SIL OFL 1.1.
- If you make and distribute boards from this design, CERN-OHL-S-2.0 requires you to mark them with the design's source location (this repository's URL).

---

© 2026 DeskStream Project.
