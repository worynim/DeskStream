**English** | [한국어](README.ko.md)

# DeskStreamCore

An Arduino library of shared modules used by the DeskStream board (ESP32-C3 + SSD1306 128x64 × 4) sub-projects.
It gathers into this one library the code that used to be copied into each project (managed with `[SYNC]` comments).

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## Installation (once per PC)

```bash
bash scripts/link-arduino-lib.sh
```

Links the repository's `libraries/DeskStreamCore` into the `libraries/` folder of your Arduino sketchbook (default `~/Documents/Arduino/libraries`)
with a symbolic link. Both the Arduino IDE and arduino-cli find the library through this link.
For Windows (`mklink /J` junction), external library installation and verification, see ["First-Time Setup" in the root GUIDE](../../GUIDE.md#first-time-setup-once-per-pc).

## Usage

A sketch (.ino) must include the umbrella header. The Arduino IDE finds libraries by the names of headers at the root of `src/`,
so including only a subfolder header leaves the library out of the build.

```cpp
#include <DeskStreamCore.h>

ds::DisplayCore disp;
ds::Buttons buttons;

void setup() {
    disp.begin();                         // defaults = HW 1.1MHz · merged transfer · SW 100ns · U8G2_R2
    buttons.begin();
    buttons.setCallbacks(0, onShort, onLong);
}
void loop() {
    buttons.update();
    disp.screen(0).drawStr(0, 10, "Hello");
    disp.push();                          // only changed pages, HW and SW buses in parallel
}
```

All symbols are in `namespace ds`. Project-specific values are passed in the `ds::DisplayConfig`, `ds::WifiConfig`,
`ds::TimeSyncConfig` and `ds::ButtonTiming` structs (defaults = the values adopted by Hangeul_Clock).
The linker removes code from modules you do not use.

> Required library: `U8g2` (Arduino Library Manager). The WiFi setup portal is built into this library,
> so WiFiManager is not needed (v0.6.0+).

## Modules

| File | Contents | Origin |
|:--|:--|:--|
| `src/ds_board.h` | Shared pin map (buttons, buzzer, I2C), OLED addresses, screen dimensions | `hardware.md` |
| `src/display/ds_display.*` | **DisplayCore** — 4 U8G2 instances (separate buffers), shadow diff, HW/SW parallel transfer, brightness/invert/flip, FPS benchmark | Hangeul_Clock v2.10.0 |
| `src/display/ds_display_config.h` | Display engine settings (I2C speed, merged transfer, SW half period, timeouts, task) + measured rationale | Hangeul_Clock v2.10.0 |
| `src/display/ds_i2c_platform.*` | HW I2C asynchronous transfer task, shadow buffer, bus self-recovery | Hangeul_Clock v2.10.0 |
| `src/display/ds_sw_i2c_fast.*` | Fast SW I2C byte callback (direct register access, calibrated NOPs, ~1.19MHz effective) | Hangeul_Clock v2.10.0 |
| `src/display/ds_canvas.*` | Transposes a 512x64 1bpp canvas → per-screen page buffers (pure) — `DisplayCore::pushCanvas()` | Sound_Visualizer · Game_Of_Life |
| `src/display/ds_i2c_retry_policy.h` | Dirty mask update policy after HW I2C transfers (pure) | Hangeul_Clock v2.10.0 |
| `src/input/ds_buttons.*` | 4-button polling — callbacks (short/long/press/repeat) or event queries | Hangeul_Clock + Universal_Timer |
| `src/input/ds_button_fsm.h` | Button detection state machine — auto-repeat and stable debounce options (pure) | Hangeul · Universal_Timer · Game_Of_Life |
| `src/sound/ds_buzzer.*` | Non-blocking beeps, melodies, repeating alarms (FreeRTOS timer) | Hangeul_Clock (Universal_Timer v1.2.0+ uses its own `BellBuzzer` for duty decay) |
| `src/net/ds_wifi.*` | Non-blocking connection to the saved AP → **built-in setup portal** (open AP, captive DNS, SSID list) → reconnect on drop | Hangeul_Clock v2.10.0 + own portal |
| `src/net/ds_wifi_portal_util.*` | Portal input validation (SSID/password length), HTML escaping (pure) | New |
| `src/net/ds_time_sync.*` | NTP resync supervision and drift logging (replaces `sntp_sync_time`) | Hangeul_Clock v2.10.0 |
| `src/net/ds_time_sync_policy.h` | NTP resync timing and 1-second render trigger rules (pure) | Hangeul_Clock v2.10.0 |
| `src/net/ds_tz_util.*` | POSIX TZ string whitelist validation (pure) | Hangeul_Clock v2.10.0 |
| `src/net/ds_web_config.*` | Field-table-based `GET/POST /api/config` + `jsonEscape` | Hangeul · Chinese_Clock web_manager |
| `src/store/ds_config.*` | Field-table-based NVS loading, validation, deferred saving, migration hooks | Hangeul_Clock config_manager |
| `src/sys/ds_launcher.*` | `ds::returnToLauncher()` — restarts into the launcher (esp-launcher, factory partition) | New |
| `src/util/ds_json_lite.*` | Flat JSON key-value parser (pure) | New (replaces parseVal) |
| `src/util/ds_logger.*` | Boot log (serial + 4 lines on one OLED screen) | Hangeul_Clock v2.10.0 |
| `src/util/ds_utf8.h` | UTF-8 leading byte length detection (pure) | Hangeul_Clock v2.10.0 |

Sketches using it: Hangeul_Clock (v2.11.0+), ENG_Clock (v1.1.0+), Chinese_Clock (v1.1.0+), Game_Of_Life (v1.1.0+),
Universal_Timer (v1.1.0+), Stats_Monitor (v1.6.0+), GPS_DASH (v2.7.0+), DS_DECK (v1.6.0+), Smart_Info_Station (v2.10.0+),
Sound_Visualizer_4_OLEDs (v3.5.0+), Screen_To_OLEDs (v2.3.0+), OLED_Wide_test (v1.3.0+), Agent_Monitor (v0.1.0+),
Wireless_Analyzer (v0.1.0+), World_Clock (v0.1.0+), Page_Turner (v0.1.0+) — all 16 projects in the repository.

## Tests

Pure modules with no Arduino dependency are verified on the host (g++).

```bash
bash libraries/DeskStreamCore/test/run_all.sh
```

| File | Contents |
|:--|:--|
| `test/test_tz_util.cpp` | POSIX TZ whitelist validation |
| `test/test_utf8_len.cpp` | UTF-8 leading byte detection (4-byte misdetection regression, including Korean and Chinese characters) |
| `test/test_i2c_retry_policy.cpp` | Retry mask regression for pages that failed to transfer |
| `test/test_time_sync_policy.cpp` | NTP resync policy, 1-second render trigger, millis wraparound |
| `test/test_json_lite.cpp` | JSON key-value parser (key offset bug regression, keys inside strings, rejection of escapes and truncation) |
| `test/test_wifi_portal.cpp` | Portal SSID/password validation, SSID HTML escaping (script injection prevention) |
| `test/test_canvas.cpp` | Canvas transposition coordinates, screen boundaries, all-pixel round trip with random patterns |
| `test/test_button_fsm.cpp` | Button short/long boundary, bounce, millis wraparound, auto-repeat, simultaneous LONG\|REPEAT, stable debounce |

## Rules

- The library cannot see a sketch's `config.h`, so project-specific values are passed in settings structs, not macros.
- After changing the library, rebuild every sketch that has already switched to it. `tools/collect-firmware.mjs` also counts library changes in its "source newer than binary" check for sketches that include this header.
- Bump the version in both `version` of `library.properties` and `DS_CORE_VERSION` in `DeskStreamCore.h`. Record changes in [RELEASE_NOTES.md](./RELEASE_NOTES.md).
