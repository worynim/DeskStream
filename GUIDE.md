**English** | [한국어](GUIDE.ko.md)

# 📘 DeskStream Development & Release Guide

Details that do not fit in the [README](README.md): shared library setup, the web flasher and launcher, and the firmware release process.

## 🏗️ Common Design

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
- 16 projects (in web flasher order): Korean/English/Chinese clocks, World Clock, Universal Timer, Smart Info Station, GPS Dashboard, Wireless Analyzer, System Status Monitor, Screen Streamer, AI Agent Monitor, DS DECK, Page Turner, Sound Visualizer, Game of Life, Ultra-Wide OLED Test
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
4. To return to the launcher, unplug USB, then **plug USB back in while holding BTN1** and keep holding BTN1 for **2 more seconds**. (The RESET button is not reachable once the board is in a case, so power it up by plugging in USB.)
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

