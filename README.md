**English** | [한국어](README.ko.md)

# 🖥️ DeskStream: ESP32-C3 4-OLED Multifunctional Board

DeskStream is a multifunctional desktop information station built on an ESP32-C3 and four OLED displays.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

> 🌐 **Web Flasher**: [https://worynim.github.io/DeskStream/](https://worynim.github.io/DeskStream/) — upload any project straight from the browser to an ESP32-C3 board connected over USB. ([How to use](#-web-flasher) · [Source](./web/))

## 🛠️ Hardware Specification
- **MCU**: ESP32-C3 (SuperMini)
- **Display**: 0.96" SSD1306 OLED (128x64) x 4 (side by side they form a 512x64 wide display)
- **Buttons**: 4 user input buttons (GPIO 1, 4, 10, 9)
- **Interface**:
  - **HW I2C**: SDA(GPIO 5), SCL(GPIO 6) - drives OLED 1, 2
  - **SW I2C**: SDA(GPIO 2), SCL(GPIO 3) - drives OLED 3, 4
- Details: see [hardware.md](./hardware.md)

## 📁 Sub-Projects
This repository contains 16 main programs developed for the DeskStream board.

### 1. [Smart_Info_Station: Smart Information Station (v2.10.1)](./Smart_Info_Station/)
- **Features**: Real-time clock, weather, fine dust, YouTube subscriber count, stock indices, cryptocurrency prices and exchange rates.
- **Highlights**: **Fully modular architecture**, live configuration from a web dashboard, asynchronous data refresh (FreeRTOS), fast SW I2C on the native HAL.
- **Buttons**: Button 1 cycles the HUD inversion mode, button 4 switches pages.

### 2. [GPS_DASH: Hardware GPS Dashboard](./GPS_DASH/)
- **Features**: Real-time speed, compass, altitude, latitude/longitude, satellite status and trip statistics.
- **Highlights**: NEO-6M GPS module (5Hz), direct UBX protocol control, 8 display modes.

### 3. [Sound_Visualizer_4_OLEDs: High-Speed Audio Visualizer](./Sound_Visualizer_4_OLEDs/)
- **Features**: 128-band real-time frequency visualization from an analog microphone input (G0).
- **Highlights**: ESP-DSP integer FFT acceleration (max 70+ FPS), 512x64 ultra-wide canvas, ADC DMA sampling, 6 visualization modes (spectrum · waveform · mirror · spectrogram · dot matrix · particles).

### 4. [OLED_Wide_test: Ultra-Wide Display Test](./OLED_Wide_test/)
- **Features**: Demo and test code that drives the four OLEDs as one 512x64 screen.
- **Highlights**: Transfer optimization through shadow buffering, fast graphics rendering with LovyanGFX.

### 5. [Stats_Monitor: System Status Monitor](./Stats_Monitor/)
- **Features**: Real-time CPU, GPU, RAM and SSD status and temperatures plus network traffic of a PC/Mac.
- **Highlights**: External client integration, hardware statistics gadget mode, visualization of system vital signs.

### 6. [DS_DECK: Programmable Macro Pad (v1.6.1)](./DS_DECK/)
- **Features**: Assign text macros (Korean/English), key combos, media controls or a **web browser launcher (Browser)** to the four buttons.
- **Highlights**: **Premium macOS launcher support** (Spotlight automation), fast BLE HID engine, web-based live macro configuration, support for every special key including F1–F12.

### 7. [Screen_To_OLEDs: Real-Time Screen Streamer (v2.3.1)](./Screen_To_OLEDs/)
- **Features**: Captures part of the Mac screen in real time and sends it wirelessly to the DeskStream 4-OLED matrix (512x64).
- **Highlights**: Smart capture engine (full-screen aware), Numba JIT acceleration, 6 dithering algorithms, live settings changes.
- **Buttons**: Press button 2 to adjust brightness; long-press button 1 to rotate (flip) the screen 180 degrees.

### 8. [Hangeul_Clock: High-Quality Font Korean Clock (v2.11.1)](./Hangeul_Clock/)
- **Features**: NTP-synchronized high-quality Korean (Hangeul) word clock with a high-end font rendering system.
- **Highlights**:
    - **Zero-Stutter web API**: Caching font slot names in memory eliminates system freezes when the web dashboard calls the API.
    - **Robust stability and security**: **Hardware self-healing (I2C Recovery)** and **API input range validation (whitelist)**.
    - **Flat Buffer memory integrity**: A **Flat Buffer** strategy that prevents heap fragmentation and memory leaks.
    - **Non-blocking engine**: State-machine animation loop and `refreshNow()` ghosting prevention.
    - **Web preview sync**: The 'invert colors' setting is reflected live in the web dashboard canvas preview.
    - **64px Modern Font Engine**: Full support for high-resolution 64px (512B) bitmap fonts without clipping.
    - **Smart 24H and date system**: 24-hour format support.
    - **Interactive control**: The four physical buttons instantly toggle **hourly chime, screen inversion, Hangeul/numeric mode and time format (12/24H)**.
    - **Ecosystem integration**: Real-time web simulator (Font Studio) and a 3-second asynchronous sync engine.

### 9. [Game_Of_Life: Standalone Game of Life](./Game_Of_Life/)
- **Features**: Runs Conway's Game of Life simulation endlessly on the 512x64 large screen.
- **Highlights**: 1-bit memory optimization (8KB total), four buttons for **play/pause, single step, speed (5/10/20 FPS) and pattern reset**.

### 10. [ENG_Clock: High-Quality Font English Clock (v1.1.1)](./ENG_Clock/)
- **Features**: NTP-synchronized high-quality English word clock. Keeps the architecture and quality of `Hangeul_Clock` and ports only the text to English.
- **Highlights**:
    - **2-Line Word Layout**: A word-based two-line layout that fits the longest English word (`SEVENTEEN`, 9 letters).
    - **POSIX TZ time zone system**: Automatic DST handling, 12 regional presets plus custom input in the web UI.
    - **Font Studio**: Rasterizes a TTF in the browser into ink-centered 64px bitmaps and uploads them directly.
    - **Host test infrastructure**: Native C++ unit tests plus web↔firmware cross-validation (`bash test/js/run_all.sh`).

### 11. [Chinese_Clock: High-Quality Font Chinese Clock (v1.1.1)](./Chinese_Clock/)
- **Features**: NTP-synchronized Chinese character word clock. Inherits the hardware, UI, animation and web pipeline of `Hangeul_Clock` / `ENG_Clock` and ports only the text generator to Chinese.
- **Highlights**:
    - **4-screen 32×32 cell layout**: The longest expression is 4 characters (`四十五分`), so each screen shows 4 square cells with no line breaks.
    - **Standard Chinese expressions**: Standard rules such as `两点`, `三点零五分`, `三点半`, `九点整`, `上午`/`下午`, `星期日`–`星期六`.
    - **Simplified / Traditional / numeric display cycle**: `简体 → 繁體 → 數字` (the 3 differing characters are handled by one substitution table).
    - **POSIX TZ time zone system**: Automatic DST handling, default `CST-8`, web UI presets plus custom input.
    - **Font Studio**: Bakes a TTF in the browser into 48×48 bitmaps for 37 characters (Simplified ∪ Traditional) and uploads them to 5 font slots.

### 12. [Universal_Timer: 4-OLED Universal Timer (v1.3.0)](./Universal_Timer/)
- **Features**: A hardware timer that shows and adjusts hours, minutes and seconds on separate OLEDs and runs a countdown.
- **Highlights**:
    - **4-way interactive layout**: OLED 1 (hours / 00H), OLED 2 (minutes / 00M), OLED 3 (seconds / 00S), OLED 4 (START / PAUSE / RESET).
    - **Auto-Repeat acceleration**: Holding a button increments rapidly every 60ms.
    - **Timeout auto-restore memory**: When the countdown alarm sounds, any button restores the time that was first STARTed (regardless of pauses in between).
    - **Remembers the last set time**: Time changed with the buttons is saved to NVS on START and kept across power cycles.
    - **Event-driven dirty checking**: Only changed screens are sent, giving instant response and an accurate 30ms beep.
    - **Music-box alarm**: The end alarm is a music-box arpeggio; alarm and boot melodies lower the PWM duty from 50% to 3% so they ring out like a bell.

### 13. [Agent_Monitor: AI Coding Agent Monitor (v0.8.2)](./Agent_Monitor/)
- **Features**: Shows the state of each Claude Code session (thinking / running / awaiting approval / awaiting input / done / error) live, one session per OLED, with buzzer alerts.
- **Highlights**: Claude Code hooks (CLI and desktop app) → PC daemon (Python, tray icon) → USB serial or BLE (signed with a key registered over USB). Claude Code is unaffected when the daemon is off. Approve/deny permission requests with the device buttons (waits alongside the terminal dialog, never auto-allows). Korean text display (Galmuri7 font), session detail screen (4 pages of the last reply · close session), mute and OLED brightness in a config file, and long-press buttons to run PC commands (bring app to front · shell command · key input).

### 14. [Wireless_Analyzer: 2.4GHz Wireless Environment Analyzer (v0.5.1)](./Wireless_Analyzer/)
- **Features**: Uses the ESP32-C3's own WiFi/BLE radio as a measuring tool to show 2.4GHz channel 1–13 congestion with a recommended channel, nearby AP signal strength trends and tracking, and the number of nearby BLE devices on the 512x64 screen. A standalone device that never joins a router.
- **Highlights**: 6 screens (channel spectrum · channel congestion · AP list · AP tracking · BLE devices · summary). The recommended channel combines a channel cost that separates same-channel (time sharing) from adjacent-channel (noise) interference with channel activity measured by a promiscuous sweep (control/data frames per second). AP tracking mode scans only that channel quickly and guides you by signal strength with the buzzer. A scheduler shares the single radio between WiFi scans, sweep measurements and BLE scans. BLE device addresses are never shown or transmitted. Korean text display (Galmuri7), mode/brightness/flip saved to NVS, short button click sounds.

### 15. [World_Clock: Four-City World Clock (v0.1.0)](./World_Clock/)
- **Features**: Shows the local time of four places on Earth, one city per OLED. Large AM/PM text, sun/moon icons and a 0–24h day bar distinguish morning/afternoon and day/night, and `+1d`/`-1d` (`+1일`/`-1일` in Korean) appears when the date differs from the reference city (screen 1).
- **Highlights**: **Set places by pressing the screens** — no web page. Long-press a screen to make it the preview while the other three become world map (cancel) · west · east buttons (short = one city, hold = skip by time zone). 113 cities (Korean/English), automatic daylight saving (based on tzdata 2026f, checked against the system zoneinfo by host tests). 12/24h · brightness · Korean/English toggle, saved to NVS. 0.1.0 is released before testing on real hardware.

### 16. [Page_Turner: BLE Page Turner (v1.1.0)](./Page_Turner/)
- **Features**: A Bluetooth remote you hold upright and press with your thumb to page through web novels, webtoons and YouTube Shorts. Also works as a smartphone camera shutter (including delayed shutter) and a PC presentation slide clicker (PageUp/PageDown).
- **Highlights**: BLE HID (NimBLE) combining keyboard, media keys and mouse wheel. 7 modes named after apps (YouTube · Kakao Webtoon/Novel · Series Webtoon/Novel · Camera · PPT), pre-tuned with the signals and scroll amounts found by testing on real Android devices. 3 paired-device slots (long-press BTN3 → press a slot button twice to switch, long press = new device). Settings, last mode and slot saved to NVS; screen off after 1 minute, deep sleep after 5 minutes. The portrait screen shows each button's role with arrows and icons — drawn as outlines only at minimum brightness to reduce glare in the dark and save power.

- **Hybrid I2C architecture**: Hardware I2C and fast software I2C with direct register control run in parallel to drive the four screens without stutter.
- **Performance optimization**: Dirty page checking and graphics acceleration techniques for smooth animation.
- **Standardized documentation**: Doxygen-style headers and author information in the source code of every project for better maintainability.

## 🧩 Shared Library (DeskStreamCore)

Features shared by several projects (display engine, buttons, buzzer, WiFi, NTP, settings storage, web settings engine)
are being gathered into the Arduino library [`libraries/DeskStreamCore`](./libraries/DeskStreamCore/).
**All 16 projects** use this library (display transfer on HW 1.1MHz + fast SW I2C, sending only changed tiles in parallel — measured 45.7fps across 4 screens on the Korean clock).

### First-Time Setup (once per PC)

The Arduino IDE looks for `#include <DeskStreamCore.h>` only in the **sketch folder** and the **sketchbook's `libraries/` folder**.
The library source lives in the repository's `libraries/DeskStreamCore/`, so create a **link (shortcut)** to it inside the sketchbook
`libraries/` folder once. Because it is a link rather than a copy, editing the library in the repository or running `git pull`
takes effect in every sketch immediately.

```
<sketchbook>/libraries/DeskStreamCore   ──(link)──▶   DeskStream/libraries/DeskStreamCore
```

**1. Find the sketchbook location** — Arduino IDE **Settings → Sketchbook location**.
The default is `~/Documents/Arduino` on macOS and `C:\Users\<user>\Documents\Arduino` on Windows.

**2-A. macOS / Linux** — Run from the repository root (the DeskStream folder).

```bash
bash scripts/link-arduino-lib.sh
```

- The sketchbook path is taken from the argument → `arduino-cli config get directories.user` → `~/Documents/Arduino`, in that order.
  If it is not in the default location, pass it as an argument: `bash scripts/link-arduino-lib.sh "/path/to/sketchbook"`
- On success it prints `✅ 연결 완료` (linked), or `✅ 이미 연결됨` (already linked) if the link exists (safe to run repeatedly).
- If the repository was moved and the link points elsewhere, it is replaced with the new location. If a **real folder** (not a link)
  already exists, the script stops without overwriting it — check its contents, move or delete it, then run again.

**2-B. Windows** — `scripts/link-arduino-lib.sh` is for bash, so create a directory junction from the **Command Prompt (cmd)**
(no administrator rights needed). Adjust the two paths below for your PC.

```bat
mkdir "%USERPROFILE%\Documents\Arduino\libraries"
mklink /J "%USERPROFILE%\Documents\Arduino\libraries\DeskStreamCore" "C:\path\DeskStream\libraries\DeskStreamCore"
```

- The first line is needed only if the `libraries` folder does not exist (an "already exists" message is fine).
- On success it prints `Junction created for ...`.
- With PowerShell: `New-Item -ItemType Junction -Path "$env:USERPROFILE\Documents\Arduino\libraries\DeskStreamCore" -Target "C:\path\DeskStream\libraries\DeskStreamCore"`
- Do not use `ln -s` in Git Bash — depending on settings it may create a **copy** instead of a link.
- If you moved the repository: delete just the junction with `rmdir "%USERPROFILE%\Documents\Arduino\libraries\DeskStreamCore"`
  (`rmdir` removes only the junction and leaves the source folder alone), then create it again.

**3. Install external libraries** — Install **U8g2** from the Arduino IDE Library Manager (all projects).
The WiFi setup portal is built into DeskStreamCore, so WiFiManager is not needed (v0.6.0+).
Canvas projects (Sound_Visualizer · Screen_To_OLEDs · OLED_Wide_test) also need **LovyanGFX**.

**4. Verify** — **Fully quit and restart** the Arduino IDE (it reads the library list at startup), open any project (e.g. `Game_Of_Life`)
and click **Verify (✓)**. If you get `DeskStreamCore.h: No such file or directory`, check that the link is in the sketchbook location from step 1.

- Terminal check: macOS `ls -l ~/Documents/Arduino/libraries | grep DeskStreamCore` (an arrow `->` means OK),
  Windows `dir "%USERPROFILE%\Documents\Arduino\libraries"` (shown as `<JUNCTION>` means OK)
- The link points to a real path on the PC, so create it once **per PC** (links are not carried over when the repository syncs via Google Drive etc.).
- When building or exporting the three clocks, also choose `Partition Scheme = Huge APP` (see each project's README).
- Library host tests: `bash libraries/DeskStreamCore/test/run_all.sh`
- **When creating a new project with AI**: have it read [`AI_GUIDE.md`](./AI_GUIDE.md) first — library usage rules, per-module APIs, templates and checklists.

## 🌐 Web Flasher

**https://worynim.github.io/DeskStream/** — Flash any of the 16 projects straight from the browser to an ESP32-C3 board connected over USB, using Chrome/Edge on desktop or Chrome on Android. No extra software to install.

- Based on **Web Serial + [esptool-js](https://github.com/espressif/esptool-js)**. Supports Chrome/Edge desktop and **Android Chrome 148+ (USB OTG)**. iPhone/iPad (Safari), Firefox Android and in-app browsers are not supported
- **Korean / English UI** — switch at the top right of the page (browser language auto-detected, choice remembered)
- The **README ↗** link on each project card opens that project's documentation in a new tab.
- Flashing **preserves NVS (WiFi and display settings)** by default; an "Erase all" option is available when needed
- **Launcher (esp-launcher)** entry: installs a launcher that lets you pick firmware on the board's screen and install it over Wi-Fi ([below](#-launcher-esp-launcher))
- Firmware integrity is verified by **SHA-256** (download) and **MD5** (after writing), with automatic reboot after flashing
- 16 projects: Smart Info Station, GPS Dashboard, Sound Visualizer, Ultra-Wide OLED Test, System Status Monitor, DS DECK, Screen Streamer, Game of Life, Korean/English/Chinese clocks, Universal Timer, AI Agent Monitor, Wireless Analyzer, World Clock, Page Turner
- **Hangeul Clock BB (Hangeul_Clock_bitbang_i2c) uses a different 1.3" SH1106 · 4-bus circuit and is excluded from the web flasher**.
  Because the hardware differs, it has been maintained in the separate **DeskStream_V2** folder since 2026-11-08.

### Using a Smartphone (Android)

With Android Chrome 148 or later and a **USB OTG adapter** (plus a USB data cable) you can flash from a smartphone.

1. Update Chrome to the latest version (148+).
2. Connect a USB data cable to a USB-C OTG adapter and plug in the ESP32-C3 board.
3. Open the address by typing it directly into **Chrome, not an in-app browser** such as KakaoTalk or Naver.
4. Choose a project, tap **Start flashing**, and pick the board in the port selection dialog.

> If the USB device does not appear in the Web Serial port list, check **USB (WebUSB) mode** in the flash options and try again. It is an experimental workaround that accesses the device directly through WebUSB (CDC-ACM).
>
> On iPhone/iPad, Safari (WebKit) does not support the Web Serial API, so browser flashing is not possible. Please use Chrome/Edge on a PC.

### 🚀 Launcher (esp-launcher)

A launcher that lets you switch projects on the board without a PC. The launcher lives in the factory partition and the chosen project goes into the app partition (ota_0).

1. In the web flasher, choose **Launcher (esp-launcher)** and flash it once. The current app is erased; Wi-Fi settings and fonts (LittleFS) are kept.
2. On the launcher's **Wi-Fi** screen, scan the QR code with your phone to open the setup portal and save an AP (it uses the same storage as the apps, so apps use it too).
3. In **Firmware**, download the list ([`firmware/manifest.launcher.json`](./firmware/manifest.launcher.json)), pick a project and install it — the board reboots into that app.
4. To return to the launcher, **hold BTN1 and press and release RESET**, then keep holding BTN1 for **2 more seconds**.
5. When a new launcher is released, update it on the device from the launcher's **Info → Update** (installed apps are erased, so reinstall them). For launcher 0.1.x, install "Launcher" from the Firmware list.

- If you flash another project with the web flasher onto a board that has the launcher, the flasher reads the board's partition table and installs **only the app** into ota_0, **keeping the launcher**.
- To remove the launcher, turn on **Erase all** and flash any project (all 4 images are written the old way).
- To return to the launcher from app code, call `ds::returnToLauncher()` from DeskStreamCore 0.7.0.
- The launcher source lives in the separate `esp-launcher` repository. The released files are a bundle exported with `tools/mkbundle.py deskstream_c3 <DeskStream>/firmware/Launcher`.

### Related Files and Links

| Item | Link |
|---|---|
| Web flasher app (Vite + TypeScript + esptool-js) | [`web/`](./web/) |
| **Web flasher release notes** | [`web/RELEASE_NOTES.md`](./web/RELEASE_NOTES.md) |
| Firmware collection and manifest generation script | [`tools/collect-firmware.mjs`](./tools/collect-firmware.mjs) |
| Project display metadata (name, version, icon, description) | [`firmware/projects.json`](./firmware/projects.json) |
| Released firmware list, offsets and SHA-256 | [`firmware/manifest.json`](./firmware/manifest.json) |
| Firmware list read by the launcher on the device (generated) | [`firmware/manifest.launcher.json`](./firmware/manifest.launcher.json) |
| PC client list shown in the web flasher | [`firmware/clients.json`](./firmware/clients.json) |
| PC client build and Release workflow | [`.github/workflows/build-clients.yml`](./.github/workflows/build-clients.yml) |
| GitHub Pages deploy workflow | [`.github/workflows/deploy-web.yml`](./.github/workflows/deploy-web.yml) |

### Updating Firmware

1. Select the board in the Arduino IDE: **ESP32C3 Super Mini** (`makergo_c3_supermini` board package)
2. Run `Sketch → Export Compiled Binary`
3. Run the collection script from the repository root → updates `firmware/<project>/` and `firmware/manifest.json` · `manifest.launcher.json`

   ```bash
   node tools/collect-firmware.mjs
   ```

4. Commit the `firmware/` changes and push to `main` → GitHub Actions builds and deploys the web flasher automatically

> To add or remove a project in the web flasher, edit [`firmware/projects.json`](./firmware/projects.json) and run `node tools/collect-firmware.mjs` again.
> Local preview: `cd web && npm install && npm run dev` → http://localhost:5173/DeskStream/

### One-Time Setup

Set the repository's **Settings → Pages → Build and deployment → Source** to **GitHub Actions**. After that, `.github/workflows/deploy-web.yml` deploys automatically on every push to `main`.

---

© 2026 DeskStream Project.
