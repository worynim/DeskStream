**English** | [한국어](GUIDE.ko.md)

# Hangeul Clock Detailed Guide - v2.11.1

The detailed companion to the [README](README.md).

## ✨ Major Features

### 1. Modern Modular Architecture
- **H/CPP separation**: Firmware logic is fully split into per-class modules (.h/.cpp) for maximum maintainability and readability.
- **WebManager encapsulation**: The web server engine is built as an independent class to reduce coupling with the main logic.
- **Hardware Abstraction Layer (HAL)**: Hardware-dependent I2C driver logic is separated into the `I2CPlatform` class, giving a layered architecture.

### 2. Multi-Level Control & Audio UX
- **OLED brightness control**: Fine brightness control in 1–255 steps from the web dashboard, persisted in flash.
- **Cheerful chime melody**: Instead of a flat beep, the hourly chime is a bright two-note high-register (C7-G7) "ding-ling~".
- **Non-blocking input**: A FreeRTOS-based non-blocking input system guarantees instant button response even during animations.

### 3. Flat Buffer Memory Optimization
- **Flat Buffer & Static Allocation**: Moving from per-glyph dynamic allocation to a single contiguous memory block (Flat Buffer) and **static allocation** of button objects eliminates heap fragmentation and memory leaks.
- **Reliable RAM management**: At boot it pre-scans the total font resource size and allocates memory 'exactly once', giving professional-grade long-term stability.
- **Flash-based web UI**: The large HTML data moved to flash memory (PROGMEM) to free system resources.

### 4. Non-blocking Animation State Machine
- **Real-time responsiveness**: The animation loop was redesigned from `delay()`-based to an `updateTick()` state machine, so button presses and web setting changes take effect immediately even while effects play.
- **Frame synchronization**: Per-mode frame budgets (default 16 frames × 10ms, snow assembly 48 frames × 16ms) are separate constants, so each mode's playback time is precisely tick-controlled. Split flap reuses the default budget (16 × 10ms).

### 5. 64px Modern Font Engine
- **Wide bitmap support**: Beyond the old 32px limit, it fully supports **64px (512-byte) high-resolution glyphs**, so large fonts render cleanly without clipping.
- **Smart format detection**: A hybrid encoding pipeline that detects and handles 32px (legacy) and 64px (modern) data at runtime.

### 6. Advanced Layered Rendering
- **Transparent overlap**: `setBitmapMode(1)` removes interference from the bitmaps' black backgrounds.
- **Artistic typography**: Glyphs naturally cross and overlap each other, giving a dimensional, refined layout within the narrow OLED screens.

### 7. Hardware Self-Healing & Data Integrity
- **I2C Recovery System**: When an error is detected during live communication monitoring, it automatically re-initializes the bus and reassigns display addresses, recovering from freezes on its own.
- **Atomic cache update**: Bitmap loading uses a transaction approach with a temporary buffer and `std::move`, keeping the existing cache safe even on file system errors.
- **Concurrency guard**: Synchronization timeouts between FreeRTOS tasks prevent data corruption (race conditions).

### 8. Premium Startup Experience
- **Startup melody**: Plays a cheerful melody right after power-on to signal the device is ready.
- **OLED boot log**: Shows the boot and network connection process as a live log on screen 1.
- **Progress animation**: For waits such as WiFi/NTP sync, a progress animation adds dots (`.`).

### 9. Smart Time & Date System
- **Full 24-hour support**: Choose freely between 12H (오전/오후, AM/PM) and 24H (0–23) formats.
- **Smart automatic date display**: In 24H mode, the first OLED automatically shows the current date (e.g. '13일', the 13th) with a smart layout.
  - The date unit "일" (day) is aligned to **the same right column (x=96)** as the hour/minute/second screens (aligned even for 2–3 character dates).
- **Zero-padding everywhere**: In numeric mode, `%02d` formatting pads hours, minutes and seconds with zeros (as in '05분') for maximum legibility.

### 10. Interactive Web Dashboard & Real-time Sync
- **Font Studio v2+**: One-stop TTF font upload, glyph bitmap conversion and live preview in the web dashboard.
- **Zero-Stutter web API**: Caching font slot names in memory removes the LittleFS I/O bottleneck (5-second freeze) on web calls.
- **Real-time web simulator**: Simulate and control the device's current settings (display mode, time format, color inversion, etc.) live on the web.
- **3-second asynchronous polling**: A robust sync engine **reflects physical button presses on the web page within 3 seconds** without a refresh.
- **Instant font application**: A font uploaded from the web is applied to the device screen immediately, without a reboot.

### 11. Visual Integrity Protection
- **refreshNow() protection**: Forces rendering so the clock digits do not freeze mid-animation even during slow work such as font loading.

## 📂 Project Structure

The firmware is modularized by function. Features shared with other DeskStream projects — display transfer, buttons, buzzer, WiFi, NTP supervision, settings storage and the web settings API —
use the shared library [DeskStreamCore](../libraries/DeskStreamCore/) (v2.11.0),
and this folder contains only code specific to the Korean clock.

| File | Type | Main role |
| :--- | :--- | :--- |
| **Hangeul_Clock.ino** | **Entry** | Program entry point. System initialization (setup) and main service loop |
| **config.h** | **Config** | Project-specific constants (WiFi portal name, animation, fonts, UI). Pin and I2C performance values live in DeskStreamCore |
| **config_manager.h/cpp** | **Service** | Settings struct and **field table** (NVS keys, validation ranges, web apply order), orientation default migration. Loading and deferred saving via `ds::ConfigStore` |
| **display_manager.h/cpp** | **Manager** | High-level display control, UI stages such as IP check/help, and animation triggers |
| **hangeul_time.h/cpp** | **Logic** | Business logic converting time into Korean text and numeric strings (including 24H / on-the-hour handling) |
| **renderer.h/cpp** | **Engine** | Bitmap cache management and high-level graphics rendering engine (Dither, Zoom, Flip, etc.). Also holds the pixel kinematics of snow assembly mode (hash, arrival time, gravity trajectory) and the fold geometry of split flap mode (axis projection, top/bottom half bands) |
| **web_manager.h/cpp** | **Web** | Dashboard page, font upload and cache refresh routes. `/api/config` is handled by `ds::WebConfigApi` from the field table |
| **web_pages.h** | **Resource** | Embedded HTML/JavaScript/CSS resources for Font Studio and the dashboard |

DeskStreamCore modules used: `ds::DisplayCore` (4-OLED shadow diff, HW/SW parallel transfer), `ds::Buttons`, `ds::Buzzer`,
`ds::WifiService`, `ds::TimeSync`, `ds::ConfigStore`, `ds::WebConfigApi`, `ds::Logger`, `ds::utf8CharLen`.

## 🛠 Hardware
- **MCU**: ESP32-C3 (SuperMini)
- **Display**: 4x SSD1306 OLED (128x64) - fast I2C. HW I2C 1.1MHz (screens 0·1) and fast SW I2C (screens 2·3) in parallel give a 4-screen full frame in 22.4ms / 44.6fps (v2.10.0)
- **Input**: 1x Reset/Function Button (GPIO 1)
- **Output**: Passive Buzzer (GPIO 7) - precise non-blocking buzzer logic

## 🎮 Button Controls

The four buttons on the front change various settings instantly.

| Button | Short Press | Long Press (1 second) |
| :--- | :--- | :--- |
| **BTN 1** | **Hourly chime ON/OFF**: sound on the hour | **Screen flip**: rotate the display 180 degrees |
| **BTN 2** | **Switch display mode**: Korean ↔ numeric | **Switch time format**: 12H ↔ 24H |
| **BTN 3** | **Toggle animation**: cycle digit transition effects | **Switch font**: cycle saved font slots (0–4) |
| **BTN 4** | **Switch screen**: clock ↔ IP check ↔ help | **Color invert**: black-white inversion |

> [!TIP]
> **WiFi reset**: Holding **BTN 1** while the device boots deletes the saved WiFi settings and enters setup mode.

## 📖 Usage
1. **WiFi setup**: On first boot, connect to the `Hangeul_Clock_Setup` AP it creates and enter your WiFi details.
2. **Help**: While running, press **BTN 4** to open the help page and see the current settings (CHIME, FLIP, etc.).
3. **Reset settings**: Holding the button (BTN 1) during boot resets the WiFi information and system settings.

## 📚 Library Dependencies
- `DeskStreamCore` (repository `libraries/DeskStreamCore` — run `bash scripts/link-arduino-lib.sh` once before building)
- `U8g2` (fast graphics control)
- `LittleFS` (bitmap and settings storage)

## 🔧 Build Settings
- Choose **Huge APP** for `Partition Scheme` when building (used for the size check limit). The actual partitions come from the sketch folder's `partitions.csv` —
  the **shared DeskStream layout** (app0 2.5MB / spiffs `0x290000` 1.375MB, no OTA) always takes precedence, and since the spiffs location
  matches the `default` scheme, uploaded fonts are preserved when swapping firmware with other projects (v2.11.0+).

---
© 2026 DeskStream Project.
