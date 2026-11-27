**English** | [한국어](GUIDE.ko.md)

# ENG Clock Detailed Guide - v1.1.1

The detailed companion to the [README](README.md).

## ✨ Major Features

### 1. 2-Line Word Layout
- **Beyond the 4-character-per-screen limit**: Word-based line breaking was introduced to fit the longest English word, `SEVENTEEN` (9 letters). With two or more words the text is always split into two lines, and each line is centered horizontally.
- **No forced character breaks**: Breaking by character count would split `WEDNESDAY`, so it was rejected.
- **Precomputed single-line vertical alignment**: `LayoutChar::y` carries the precomputed vertical position of each line, so the same arithmetic is not duplicated by callers.

### 2. POSIX TZ Timezone System
- **Automatic DST**: Uses POSIX TZ strings (`KST-9`, `EST5EDT,...`, etc.) instead of fixed offsets, so daylight saving time is applied automatically.
- **Time zone selection in the web UI**: 12 regional presets + custom input. The setting is persisted in NVS (Preferences).
- **Input validation**: TZ string whitelist validation (`ds::isValidTimezone`) keeps bad settings from breaking the system time.

### 3. Modular Architecture — same as the Korean edition
- **H/CPP separation**: Firmware logic fully split into per-class modules (.h/.cpp).
- **Pure logic separation (native-testable)**: Time conversion (`english_time_core`) and layout (`layout_engine` + `renderer_geometry`) are pure C++ with no Arduino dependency, so they can be **unit-tested on the host**.
- **Hardware Abstraction Layer (HAL)**: Hardware-dependent I2C driver logic is separated into the `I2CPlatform` class.
- **Flat Buffer & Static Allocation**: A Flat Buffer strategy that eliminates heap fragmentation and memory leaks.

### 4. Font Studio (in-browser font studio)
- **One-stop TTF → bitmap conversion**: Upload a TTF in the browser and it rasterizes 38 English glyphs (A–Z, 0–9, `,` `'` `.`, etc.) into high-resolution 64px bitmaps and uploads them directly to the device.
- **Ink-centered rasterizer**: Instead of the em-box center, it places **the union of measured ink boxes** (based on uppercase A–Z) at the vertical center of the raster, so top and bottom margins stay even as the size grows.
- **Per-glyph horizontal centering**: Both wide (`W`) and narrow (`I`) glyphs have their own ink center at the cell center.
- **5 font slots**: Store font sets in LittleFS slots `/f0`–`/f4` and select one on the web.
- **Live preview and simulator**: Canvas preview before upload and real-time simulation of device settings.
- **Behavior without an uploaded font**: Word mode turns on only with a custom font cache (otherwise numeric mode + `Font Required!` — by design). If a 256B/512B font from another project (Korean or Chinese edition) remains in a slot, it has no English glyphs, so the screen shows the fallback and the IP screen also uses the default font (detailed analysis: `PLAN.md` §6.15).

### 5. Interactive Control & Animation — same as the Korean edition
- **4-button input** (GPIO 1/4/10/9): Instantly change hourly chime, display mode, time format, screen flip and more.
- **5 non-blocking animations**: Scroll Up/Down, Vertical Flip, Dithered Fade, Zoom. In the two-line layout, the travel distance shrinks to `LINE_HEIGHT` and each line is isolated with a band clip.
- **12/24-hour + English weekdays**: Both 12H (AM/PM) and 24H are supported, and weekdays are always in English (`MONDAY`) regardless of mode.
- **Unit labels in numeric mode**: Unit suffixes are attached inline, as in `02 H 15 M 30 S`.

### 6. Hardware Self-Healing & Test Infrastructure
- **I2C Recovery System**: Automatic re-initialization and address reassignment when a bus error is detected.
- **Host test infrastructure**: 3 native C++ unit test suites + 6 web↔firmware cross-validation checks (shared module tests live in DeskStreamCore) (see `test/` — `bash test/js/run_all.sh` verifies everything at once).

## 📁 Directory Structure

```
ENG_Clock/
├── ENG_Clock.ino        Main entry (setup / loop control)
├── config.h / config_manager.*   Global constants / settings field table (NVS keys, validation, web apply order)
├── english_time.*       Time → English text conversion (Arduino String interface)
├── english_time_core.*  Pure time conversion logic (native test target)
├── layout_engine.*      Word-based two-line layout engine
├── renderer_geometry.*  Glyph geometry detection (192B/384B)
├── renderer.*           U8g2 renderer (band clip, overlap rendering)
├── display_manager.*    4-OLED parallel output, time zone application, animation
├── web_manager.*        Page and font upload (/api/config is ds::WebConfigApi)
├── web_pages.h          Font Studio web UI (embedded HTML/JS)
├── PLAN.md              Design document
├── 수정할 사항.md        Bug/improvement tracking
└── test/                Host unit tests and cross-validation (test/README.md)
```

Display transfer (HW 1.1MHz + fast SW I2C), buttons, buzzer, WiFi, NTP supervision, settings storage, web settings API, logging and TZ validation
use the shared library [DeskStreamCore](../libraries/DeskStreamCore/) (v1.1.0). Before building, run
`bash scripts/link-arduino-lib.sh` once from the repository root to link the library into your Arduino sketchbook.

## 🚀 Build

```bash
arduino-cli compile --fqbn esp32:esp32:makergo_c3_supermini:PartitionScheme=huge_app .
```

- In the Arduino IDE, choose **`Huge APP`** for `Partition Scheme` (to raise the size check limit).
- The actual partitions come from the sketch folder's `partitions.csv` — the **shared DeskStream layout** (app0 2.5MB / spiffs `0x290000` 1.375MB, no OTA)
  always takes precedence. The spiffs location matches the `default` scheme, so uploaded fonts are preserved when swapping firmware with other projects.
- Since v1.1.0 — with the `default` scheme (app0 1.25MB), only 141 B were left after switching to DeskStreamCore.

## 🧪 Tests

```bash
bash test/js/run_all.sh   # all JS ↔ firmware cross-validation
cd test && g++ -std=c++11 -I.. test_english_time.cpp ../english_time_core.cpp -o /tmp/t && /tmp/t   # native unit test
```

---
© 2026 DeskStream Project. — worynim@gmail.com
