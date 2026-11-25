**English** | [한국어](README.ko.md)

# Chinese Clock (Chinese Word Clock) - v1.1.1

A premium ESP32-C3 desk gadget that syncs time from an NTP server and shows hours, minutes and seconds in **Chinese characters** on four OLED displays. It inherits the hardware, UI, animation and web pipeline of `Hangeul_Clock` / `ENG_Clock` as-is and is a port that **replaces only the text generator with Chinese**.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

Design rationale and implementation records (plan corrections, assumptions, defects) are in [`PLAN.md`](./PLAN.md). This document covers **how to use it** only.

## ✨ Major Features

### 1. 4-Screen Structure (1:1 with the Korean and English editions)

Each screen fits exactly **four 32×32 square cells** (4 × 32 = 128px = screen width).

| Screen | 12-hour | 24-hour |
|:--:|:--|:--|
| 0 | `上午` / `下午` | `星期六` (weekday only) |
| 1 | Hour — `三點` | Hour — `十三点` / `十三點` |
| 2 | Minute — `四十五分` | Minute — `四十五分` |
| 3 | Second — `四十五秒` | Second — `四十五秒` |

The longest expression is **4 characters**, so there are no line breaks. The two-line layout, pitch ladder and ink width measurement that the English edition introduced because of `SEVENTEEN` (9 letters) are **all unnecessary.**

### 2. Standard Chinese Expressions (Standard Form)

| Rule | Example | Why it matters |
|:--|:--|:--|
| ① `点` = hour | `三点四十五分` | Order is "larger unit → smaller unit" |
| ② 2 is **`两`**, not `二` | `两点` | The most common beginner mistake |
| ③ `零` in the tens place for minutes under 10 | `三点零五分` (❌ `三点五分`) | Without it, it reads as `3:5`/`3:35` |
| ④ 30 minutes is **`半`**, not `三十` | 3:30 → `三点半` / 3:31 → `三点三十一分` | Formal writing always uses `半` |
| ⑤ On the hour is `整` | `九点整` | Zero in the minute/second unit |
| ⑥ AM/PM are 2 characters | `上午` (0–11) / `下午` (12–23) | Midnight (0h) as `上午` follows common Chinese practice |
| ⑦ Weekday is 3 characters | `星期日`–`星期六` | Same in Simplified and Traditional |
| ⑧ `秒` is fully symmetric with `分` | `四十五秒` | Keeps the code from splitting into two branches |

Rejected expressions: colloquial forms (`四点差一刻` — omitting `差` flips the meaning) and traditional double-hours (`申时` — 2-hour units **lose seconds**).

> ⚠ **Unverified**: The table above is the reference for implementation and tests. Whether it reads well on a real OLED
> has not been checked yet. See the **"⚠️ Incomplete — Needs Hardware Check"** section below.

### 3. Simplified / Traditional (简体 ⇄ 繁體)

Only **3 characters** differ — `点`/`點`, `时`/`時`, `两`/`兩`. Instead of keeping two copies of every string,
one substitution table handles it, so there is exactly one place in the code where the rules could diverge.

- **3-step display cycle** — `简体 → 繁體 → 數字` (BTN2 short / web item 3)
- **Character set ↔ font slot are independent.** A slot holds **the union of both sets, 37 characters**,
  so any slot works for either character set (the 3 differing characters = 864 B).
- If a slot lacks the characters specific to a character set, switching is **refused** and `No Trad Font` / `No Simp Font` is shown.

### 4. POSIX TZ Time Zone System

- **Automatic DST**: Uses POSIX TZ strings (`CST-8`, `KST-9`, etc.) instead of fixed offsets.
- **Time zone selection in the web UI**: Regional presets + custom input, persisted in NVS (Preferences).
- **Input validation**: Whitelist validation with `ds::isValidTimezone` keeps bad settings from breaking the system time.
- The default is `CST-8` (China).

### 5. Font Studio (in-browser font studio)

- **One-stop TTF → bitmap conversion**: Upload a TTF in the browser and it bakes **37 characters** (34 Simplified ∪ 3 Traditional-only)
  into **288 B (48×48) rasters** and uploads them to the device.
- **5 font slots**: LittleFS `/f0`–`/f4`. Store a different font in each slot and select it on the web.
- **A slot stores only the name of "the font this slot was baked from".** The `.ttf` source does not fit
  (free LittleFS space of 1.37 MB ≈ 1.32 MB, while Chinese TTFs are 5–20 MB).
- **Live preview and simulation**: Canvas preview before upload + device settings synced by 5-second polling.

### 6. Interactive Control & Animation

**4 buttons** (GPIO 1 / 4 / 10 / 9) — every action goes through the same functions as the web UI.

| Button | Short | Long |
|:--|:--|:--|
| BTN1 | Hourly chime ON/OFF | Screen flip (180°) |
| **BTN2** | **Display mode 简体 → 繁體 → 數字** | 12H ⇄ 24H |
| BTN3 | Cycle animation (6 kinds) | **Cycle font slot (0–4)** |
| BTN4 | Switch page (clock → IP → help) | Color invert |

**6 animations**: OFF · Scroll Up · Scroll Down · Vertical Flip · Dithered Fade · Zoom.
It is non-blocking and never blocks `loop()`; during I2C transfers, `on_yield()` handles web and input first.

### 7. Modular Architecture

- **Pure logic separated**: `chinese_time_core` · `layout_engine` · `renderer_geometry` have no Arduino dependency, so they can be **unit-tested on the host**.
- **HAL**: HW/SW I2C drivers separated into `I2CPlatform` + `recoverBus()` self-recovery.
- **Static allocation**: Bitmaps live in one contiguous `flatBuffer` block, eliminating heap fragmentation and leaks.
- **Single-line layout engine**: Only one function, `layoutLine()` (60–70 LOC), holds the rules.

## 📁 Directory Structure

```
Chinese_Clock/
├── Chinese_Clock.ino      Main entry (setup / loop control, composes the 4-screen text)
├── config.h               Global constants (geometry 32/48/6 · animation · character set)
├── config_manager.h/.cpp  Settings field table (NVS keys, validation, web apply order) — stored via ds::ConfigStore
├── chinese_time.h/.cpp    Time → Chinese text (Arduino String wrapper)
├── chinese_time_core.h/.cpp ★ Time → Chinese text pure logic (native test target)
├── layout_engine.h/.cpp   ★ Single-line layout engine (replaces the English edition's two-line engine)
├── renderer_geometry.*    Glyph geometry detection (288B → glyphW 32 / drawW 48 / xOffset −8)
├── renderer.*             Bitmap renderer + dither/zoom/squash pixel operations
├── renderer_layout.*      Shared layout helpers
├── display_manager.*      4-OLED parallel output, UI stages, animation, slot/character set switching
├── web_manager.*          Page and font upload API + presentation keys (/api/config is ds::WebConfigApi)
├── web_pages.h            Font Studio web UI (embedded HTML/JS in PROGMEM)
├── test/                  Host unit tests and cross-validation (see "Tests" below)
├── PLAN.md                Design document + implementation records (corrections, defects, measurements)
├── README.md              This document (Korean: README.ko.md)
└── RELEASE_NOTES.md       Change history
```

Display transfer (HW 1.1MHz + fast SW I2C), buttons, buzzer, WiFi, NTP supervision, settings storage, web settings API, logging and TZ validation
use the shared library [DeskStreamCore](../libraries/DeskStreamCore/) (v1.1.0). Before building, run
`bash scripts/link-arduino-lib.sh` once from the repository root to link the library into your Arduino sketchbook.

## 🔌 Hardware (DeskStream board)

| Item | Spec |
|:--|:--|
| MCU | **ESP32-C3** (RISC-V single-core, 3.3V) |
| Displays | 4× SSD1306 128×64 — 2 on HW I2C (GPIO 5/6), 2 on SW I2C (GPIO 2/3) |
| I2C addresses | 0x3C / 0x3D (one per bus) |
| Buttons | BTN1=GPIO 1, BTN2=GPIO 4, BTN3=GPIO 10, BTN4=GPIO 9 (`INPUT_PULLUP`) |
| Buzzer | GPIO 7 |
| Storage | LittleFS (spiffs `0x290000`, `0x160000` ≈ 1.37 MB) + NVS (settings) — shared `partitions.csv` layout, app0 2.5MB |

See the parent [`../hardware.md`](../hardware.md) for the full pin map.

## 🚀 Build & Upload

```bash
# Build
arduino-cli compile --fqbn esp32:esp32:makergo_c3_supermini:PartitionScheme=huge_app .

# Upload (baud rate 115200)
arduino-cli upload  --fqbn esp32:esp32:nologo_esp32c3_super_mini -p /dev/cu.usbmodem* .

# Serial log
arduino-cli monitor -p /dev/cu.usbmodem* -c baudrate=115200
```

**Partition**: In the Arduino IDE choose **Huge APP** for `Partition Scheme` (used for the size check limit). The actual table is the sketch folder's
`partitions.csv` — the shared DeskStream layout (app0 2.5MB / spiffs `0x290000`), so fonts are preserved (v1.1.0+).

**Boot tip**: On the ESP32-C3 native USB, if the host does not read the port,
`HWCDC::write()` blocks for up to 2 seconds per call. `Serial.setTxTimeoutMs(0)` is set so boot does not
stall even without a serial monitor open.

**Factory reset**: Power on **while holding BTN1** right at boot. It formats LittleFS, resets WiFi settings and reboots.

## 🧪 Tests

```bash
bash test/run_all.sh      # everything (native C++ + JS cross-validation)
```

**Current status: 4 passed / 0 failed** (run on 2026-11-09 — 3 dedicated suites + JS cross-validation).
TZ validation, UTF-8, I2C retry and NTP policy tests moved to the shared library: `bash libraries/DeskStreamCore/test/run_all.sh`

| Suite | Assertions | What it checks |
|:--|--:|:--|
| `test_chinese_time` | **483** | Every entry of the §3 expression rule table (24 hours × 12H/24H × 2 sets, minutes/seconds 60 × 2 × 2, weekdays 7 × 2, AM/PM 24 × 2) + boundary values + completeness of the 3 Traditional substitutions |
| `test_layout` | **69** | 4 characters fit / 5 are clipped, 3-byte CJK coordinates, 4 blank cells for `"13 时"`, horizontal centering. **Iterates all 758 §3 expressions** — max 4 cells (limit 4, margin 0) |
| `test_geometry` | **34** | 288B mapping, 287 and 289 rejected |
| `layout_crosscheck` | **696 rows** | **Exhaustive comparison** of the JS in the shipped `web_pages.h` ↔ firmware C++ (0 mismatches) |
| `web_fixes_test` | **92** | Upload wiring, slot pinning, name tag contract, polling guard |

> **Why iterate everything**: If `layoutLine()` returns more than 4 cells, `display_manager` draws a **blank screen**
> and only logs it. There is no way to find out before looking at the OLED. A check using only representative strings
> could miss the longest expression, so all 758 are iterated. **The margin is 0**, so it breaks immediately if the
> expression rules or cell count change.

> **The cross-validation harness lives in `test/js/`, not `/tmp`.** It validates **the shipped `web_pages.h` itself**,
> not a mirror script — if a symbol disappears, extraction fails immediately.

## 📐 Memory Usage (measured 2026-11-09, v1.1.0)

| | Value | Free |
|:--|--:|--:|
| Flash (app0 2.5MB, `partitions.csv`) | **1,307,021 B** | about **1.31 MB** |
| RAM (globals) | 50,932 B (15%) | 276,748 B |

> v1.0.x used the `default` scheme (app0 1.25MB), leaving only 8 KB of flash, which is why **no CJK fallback font was included**.
> Since v1.1.0 app0 is 2.5MB so there is room, but there is still no fallback font — **uploading a custom font is required**,
> otherwise only numeric mode is shown.

## 🈴 Font Upload Procedure

**This is required.** Without a custom font, Chinese characters are not drawn.

1. Connect the device to WiFi and open the **device IP** in a browser (check the IP screen with BTN4 short).
2. In **1. Font file**, choose a `.ttf` / `.otf`.
   - Recommended: **Noto Sans SC**, **Source Han Sans** — CJK fonts that include both Simplified and Traditional.
   - ⚠ **It must include both character sets.** Without `點`·`時`·`兩`, switching to Traditional is refused.
3. Keep **2. Font size** = **32px** (default). Above 40px characters overlap their neighbors and CJK becomes unreadable.
4. **3. Character set** — display mode (简体/繁體/數字). Uploads **always** send all 37 characters regardless of this value.
5. **4. Storage slot** — the slot (0–4) to store into.
6. **Convert & Upload** → if serial prints `[WEB] Slot N name = ….ttf`, it worked.

> ⚠ **If `Slot name rejected:` is printed, the upload failed.** It does not fail silently.
> The cause is a font name longer than 64 bytes (`FONT_NAME_MAX_LEN` in `config.h`).
> Names with license suffixes like `-Noncommercial-Regular` quickly exceed 64 bytes.

## 🕸️ Web Dashboard

`http://<device IP>/` — 11 settings and Font Studio are on one page.

| # | Item | Matching button |
|:--:|:--|:--|
| 1 | Font file | — |
| 2 | Font size (20–40px) | — |
| 3 | **Character set** (简体/繁體/數字) | BTN2 short |
| 4 | **Storage slot** (0–4) | BTN3 long |
| 5 | Animation (0–5) | BTN3 short |
| 6 | Hour format (12H/24H) | BTN2 long |
| 7 | Hourly chime | BTN1 short |
| 8 | Screen flip | BTN1 long |
| 9 | Colour invert | BTN4 long |
| 10 | OLED brightness | — |
| 11 | Timezone | — |

**API**

| Method | Path | Description |
|:--|:--|:--|
| `GET` | `/api/config` | All current settings + list of slot names |
| `POST` | `/api/config` | Change settings (JSON). Integers, booleans, display mode, time zone, font name |
| `POST` | `/upload?slot=N&font=<name>` | Glyph upload (file body) + slot name tag |
| `POST` | `/api/refresh_cache` | Reload the cache (for the Font Studio preview) |

> ⚠ **The web server has no authentication.** This behavior is inherited from the originals (`Hangeul_Clock`/`ENG_Clock`),
> and `/api/config` is open on shared networks. Use it only on trusted networks.

## ⚠️ Incomplete — Needs Hardware Check

Recorded honestly. These are **things not yet checked in front of the device**.

| # | Item | Status |
|:--:|:--|:--|
| 1 | **Verification gate** — 32px Chinese readability, 4-character width fit, correctness of `三点零五分`, the `三点半` rule, vertical centering | Not passed. If 32px is too tight, a 28px pitch is the fallback (PLAN §7) |
| 2 | **Manual check of the 5 animations** (Step 12) | Not done |
| 3 | **Recheck of the blank web page fix** — needs a hardware recheck after the `send()`→`send_P()` fix | Unchecked |
| 4 | **Re-upload slots 0 and 2** — the name tag defect (§12.10→12.13→12.14) was fixed three times, so it needs a hardware check | Unchecked |
| 5 | **Real multilingual rendering** — alternately upload a Simplified and a Traditional font and visually check the `点`/`點`, `时`/`時`, `两`/`兩` differences | Not done |
| 6 | **No need to erase NVS** — character set validation only applies when switching. With 37 characters in a slot, any character set passes | Confirmed |

## 📌 Known Limitations

1. **A custom font is required** — no CJK fallback font is included (8 KB flash margin). Without an upload, only numeric mode is shown.
2. **`FALLBACK_BASELINE 42` is a calculated value** — not measured. It must be checked by eye on hardware.
3. **Saving is delayed 5 seconds** — all saves happen only through `setDirty()` → a delay timer. If you power off within 5 seconds
   of changing a mode, that change is lost.
4. **Font Studio does not know the slot inventory** — there is no API to ask the firmware which characters a slot currently holds.
   Switching slots resets the badge to "unknown" (chosen so it never falsely claims "present"). If you need an accurate inventory,
   a query API such as `/api/glyphs?slot=N` must be added.
5. **The `.ttf` source is not stored** — a slot keeps only the font **name**.
6. **Flash 99%** — always re-measure the margin before adding new features.

## 🔗 Related Projects

- [`../Hangeul_Clock`](../Hangeul_Clock/) — Korean edition (v2.9.1) · the structural original of this project
- [`../ENG_Clock`](../ENG_Clock/) — English edition (v1.0.1) · for comparison when a two-line layout is needed
- [`PLAN.md`](./PLAN.md) — design document, full expression rule table, implementation records (corrections, defects, measurements)

> 📌 **Language-independent defects fixed in the Chinese edition** (blank web page `send_P`, font name tags ①②③④,
> slot badge state, unified empty slot labels) **have not yet been propagated to the English and Korean editions.**
> For the order and exact file locations when fixing them, see [`PLAN.md` §📌 language-neutral fixes](./PLAN.md).

---
© 2026 DeskStream Project. — worynim@gmail.com
