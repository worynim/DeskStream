**English** | [한국어](README.ko.md)

# 🌍 World_Clock: Four-City World Clock (v0.1.0)

A clock that shows **the current time of four places on Earth**, one on each of the DeskStream board's four OLEDs.
You choose the places **by pressing the screens**, with no web page (on this board, pressing an OLED screen presses that screen's button).

https://youtu.be/2hlVDkWg71k

> See [PLAN.md](./PLAN.md) for the development plan and design rationale.
> Install it directly by choosing "World Clock" in the [web flasher](https://worynim.github.io/DeskStream/).
> **0.1.0 is the first release, checked only with the PC screen preview, host tests and builds. It has not yet been checked on a real board.**

## Clock Screen

One screen = one city. Screen 1 is the **reference city**.

| Element | Contents |
|:--|:--|
| Top line | City name · local date and weekday · `+1d` / `-1d` (`+1일` / `-1일` in Korean) when the date differs from the reference city |
| Large `AM` / `PM` | Bold 17px English. AM sits in the upper position and PM in the lower one, so the position also tells them apart (shown in 24-hour format too) |
| Sun / moon icon | Sun from 06 to 18 local time, moon otherwise (fixed hours — not actual sunrise/sunset) |
| Large digits | Hour:minute at 28px. The `:` blinks every 0.5 seconds |
| Day bar | A 0–24h bar at the bottom. The daytime part (6–18h) is filled with a vertical mark at the current time — putting the 4 screens side by side shows the time differences |

`--:--` is shown until the time has been received.

## Pressing

| State | Screen | Short | Long / hold |
|:--|:--|:--|:--|
| Clock | Screen 1 | 12-hour ↔ 24-hour format | Set this screen's place |
| Clock | Screen 2 | 4 brightness levels (10 → 50 → 128 → 255) | Set this screen's place |
| Clock | Screen 3 | Info card for 3 seconds (on all 4 screens: date with year · UTC offset · time difference from the reference city) | Set this screen's place |
| Clock | Screen 4 | Display language Korean ↔ English (city names, weekdays, setup screen text) | Set this screen's place |
| Setup | Screen being edited | **Save and exit** | — |
| Setup | Map screen | Cancel (do not save) | — |
| Setup | West screen | One city west | Keep skipping west by time zone |
| Setup | East screen | One city east | Keep skipping east by time zone |

### Setting a Place

**Long-press (1 second)** the screen you want to change and it turns into a preview with a blinking border (city name · current time · UTC offset · time difference from the reference city).
The other three screens become, from the left, **world map (cancel) · ◀ west · east ▶** buttons, and each shows what happens when you press it.

- Cities are in one line ordered by longitude (west → east), with the two ends connected. The west/east screens preview the city you will move to.
- Holding a button skips, after 0.5 seconds and then every 0.15 seconds, to **the next city with a different UTC offset**.
- The map shows a blinking dot at the chosen city's location and a vertical line at its longitude.
- After 20 seconds without input, it returns to the clock without saving.

The 4 cities, 12/24-hour format, brightness and display language are saved and kept across power cycles (saved 5 seconds after a change). On first power-up the cities are Seoul · London · New York · Sydney.

## Cities and Time Zones

- 113 cities (Korean and English names), including 30- and 45-minute time zones (India +5:30, Nepal +5:45, etc.).
- Daylight saving time is applied automatically: US/Canada, Europe, Australia, New Zealand, Chile, Egypt and Israel rules.
- The time zone rules are built into the firmware based on **tzdata 2026f**. If a country's policy changes, the firmware has to be updated.
  Places that change with Ramadan, such as Morocco, are not included.

## WiFi

The time is received via NTP (the board has no RTC or battery, so WiFi is needed at every power-on).
If it cannot connect to the saved WiFi within 10 seconds, a setup portal opens — connect your phone to WiFi `World_Clock_Setup`
and enter the SSID and password at `192.168.4.1`. If it is not set up within 2 minutes, it reboots and tries again.
WiFi information is shared with other DeskStream projects, so a board that already ran another project connects right away without the portal.
There is no web page for choosing cities (no web server is started).

## Files

| File | Contents |
|:--|:--|
| `World_Clock.ino` | setup/loop, WiFi portal guide, press handling, clock ↔ setup switching, NVS settings |
| `config.h` | Constants such as version, timeouts and brightness levels |
| `tz_calc.cpp/.h` | Pure logic: UTC epoch + time zone rules → local time, daylight saving, UTC offset |
| `city_table.cpp/.h` | City table (fixed ID · name · latitude/longitude · time zone), west/east movement, time zone skipping |
| `clock_view.cpp/.h` | Clock screen renderer |
| `setup_view.cpp/.h` | Setup screen renderers (preview · map · west/east) and screen role assignment |
| `misc_view.cpp/.h` | WiFi guide text · notification badge · info card |
| `lang.h` | Display language (Korean/English) |
| `world_map.h` | 128x64 world map bitmap (generated by `tools/gen_world_map.py`) |
| `fonts.cpp/.h` · `u8g2_font_galmuri7_kr.h` | Galmuri7 Korean font (SIL OFL 1.1) |
| `test/run_all.sh` | Host tests: unit tests + comparison of the time zone rules with the system zoneinfo |
| `test/render/run.sh` | Screen preview: runs the same renderers as the device on the PC and saves PNGs |

The renderers draw with the U8g2 C API (`u8g2_t*`), so the device and the PC preview share the same code.

## Build

- Board: `esp32:esp32:makergo_c3_supermini`, CPU 160MHz
- **Partition Scheme = Huge APP** (uses the shared DeskStream `partitions.csv` in the sketch folder). The app is about 1.26MB — the default partition (1.31MB) leaves no room.
- Libraries: DeskStreamCore (repository `libraries/`), U8g2

```bash
arduino-cli compile --fqbn "esp32:esp32:makergo_c3_supermini:CDCOnBoot=default,CPUFreq=160,PartitionScheme=huge_app" World_Clock
```

```bash
bash World_Clock/test/run_all.sh
```
