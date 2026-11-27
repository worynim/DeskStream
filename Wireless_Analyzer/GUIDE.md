**English** | [한국어](GUIDE.ko.md)

# Wireless_Analyzer Detailed Guide - v0.5.1

The detailed companion to the [README](README.md).

## Features

| Mode | Contents |
|:--|:--|
| **Channel spectrum** | Overlays a ±2-channel-wide hill for each AP with height set by signal strength (−95 to −30dBm). Shows the SSIDs (including Korean) of up to 4 strongest APs, and the AP count at the top left. Hidden SSIDs show as `(숨김)` (hidden) |
| **Channel congestion** | For each channel 1–13: left = a bar for "the cost of putting your router on this channel", right = a **measured activity** bar (hollow, 0–400 frames/s), and above the bars the number of APs centered on that channel (including 0 — a bar with 0 means overlap from neighboring channels). The lowest-cost channel is the **recommended channel**, shown with its channel number inverted, and the reason is written at the top left (e.g. `추천 ch5 (같은 채널 1 / 겹침 16 / 활동 0/s)` = recommended ch5 (same channel 1 / overlap 16 / activity 0/s)) |
| **AP list** | 6 lines in order of signal strength: SSID · channel · RSSI · sparkline of the last 60 RSSI readings (each line zoomed to that AP's min–max, at least a 10dB span — scans where it was not seen leave gaps). `>` marks the selected AP |
| **AP tracking** | For the selected AP: current RSSI (bold), strength bar, best/average over the last 60 readings, and a graph of the last 190 RSSI readings (dotted lines at −40, −60, −80dBm). In this mode it repeatedly scans **only that AP's channel**, updating about every 0.15 seconds. With the buzzer on, it beeps faster as the signal gets stronger (every 1 second at −90dBm to every 0.1 second at −30dBm) — for finding where a router is |
| **BLE devices** | OLED1: number of nearby BLE devices (distinct addresses seen in the last 30 seconds) and counts by distance band (near: stronger than −60dBm / mid / far: weaker than −80dBm). OLED2: device count trend. OLED3·4: 6 lines in order of signal strength — name (only for devices that advertise one, otherwise `-`) · manufacturer (Apple, Samsung, etc., or the company ID if unknown) · RSSI. **Device addresses are not shown.** BLE scans (active, 3 seconds) repeat only on this screen, and WiFi scanning pauses meanwhile |
| **Summary** | One tile per screen: WiFi AP count and channels in use / recommended channel and most congested channel / strongest AP (SSID, channel, RSSI) / BLE device count and distance bands. On this screen it runs WiFi scan → channel sweep → BLE scan, refreshing about every 7 seconds per round |

- It repeats WiFi active scans (120ms per channel, about 2.3 seconds per scan) without pause. Buttons and screens never freeze during a scan.
- After the first scan, the strongest AP is selected. If the selected AP disappears from the list, the tracking screen keeps the last information and shows `신호 없음` (no signal).
- An AP once seen stays in the list for 60 seconds (absorbing weak APs that appear and disappear between scans).
- **BLE privacy**: Since this is information about other people's devices, only name, manufacturer and RSSI are shown on screen, and addresses are used only to count the same device once.
  No information is stored or sent off the device (not even printed to serial debug). Many phones (especially Apple) do not advertise a name and show as `-`.
- **Channel cost**: Strong APs on the same channel (−82dBm or above) take turns transmitting, so they count as a "time sharing" cost (their signal strength as-is);
  partially overlapping adjacent-channel APs and weak same-channel APs are noise that causes collisions, so they count as overlap ratio × 2.
  The recommendation picks from all of 1–13 but gives channels outside 1·6·11 a 3dB penalty — if APs are concentrated on 1·6·11 it picks among 1·6·11,
  and only recommends another channel when APs are scattered and that channel is clearly quieter. When measured activity is available, 3dB is added per 100 frames/s.
  See [PLAN.md](./PLAN.md) §3.3 for the detailed rationale.
- **Measured channel activity**: In congestion mode only, between scans it dwells 150ms on each of channels 1–13 and counts **control and data frames per second**
  (about 2 seconds per round, averaged per round). Management frames such as beacons and error frames are excluded. As a result, AP information updates about every 4.3 seconds in this mode.
  The ESP32-C3 has a single stream, so it cannot decode the main body of high-volume transfers (2 streams) between phones and routers; it gauges busyness from their responses (Block ACK, etc.) —
  so it is relative activity, not "percentage used". A channel with video streaming showed 153 frames/s while the rest showed 0–6, a clear difference.
  See [PLAN.md](./PLAN.md) §3.5 for the rationale.
- **Measurement limits**: Only APs that send WiFi beacons are visible. Non-WiFi interference such as microwave ovens and wireless mice cannot be measured,
  and the channel cost is an estimate of "how strongly nearby APs overlap", not actual traffic volume.

## Buttons

| Button | Short | Long |
|:--|:--|:--|
| BTN1 | Previous mode | Flip the screen 180° (when standing the board upside down) |
| BTN2 | Next mode | Pause/resume scanning (`일시정지` (paused) at the top right) |
| BTN3 | List/tracking: select previous (stronger) AP | Tracking: buzzer on/off (off by default) |
| BTN4 | List/tracking: select next (weaker) AP | Brightness level (10 → 50 → 128 → 255) |

A short button click (20ms · 3000Hz) plays the moment a button is pressed (both short and long).

Mode order: 1 channel spectrum → 2 channel congestion → 3 AP list → 4 AP tracking → 5 BLE devices → 6 summary → (back to the start).
When the mode changes (including right after boot), the mode name appears in the middle of OLED2 for 1.5 seconds, like `2/4 채널 혼잡도` (channel congestion) or `5/6 BLE 기기` (BLE devices). Entering tracking mode or changing the AP restarts the tracking graph.

**Last mode, brightness and flip** are saved and kept across power cycles (saved 5 seconds after a change). The tracking buzzer is not saved and always starts off.
At power-on the boot screen (`Wireless` `Analyzer` …, version) is shown for 1.5 seconds.

## Serial Debug (115200)

| Input | Action |
|:--|:--|
| `d` | Print the 4 screen buffers as `DUMP <screen> <1024-byte hex>` lines (for checking the screens) |
| `m` | Next mode (same as BTN2 short) |
| `n` | Select next AP (same as BTN4 short, list/tracking modes) |
| `f` | Flip the screen 180° (same as BTN1 long) |
| `t` | Print measured activity and measurement count per channel (`TRAFFIC ch10 fps= 152.8 sweeps=9`) |

## Files

| File | Contents |
|:--|:--|
| `Wireless_Analyzer.ino` | setup/loop, buttons, mode switching, settings storage (NVS), serial debug |
| `config.h` | Version, scan and screen constants |
| `scheduler.cpp/.h` | Radio time sharing — alternates WiFi scans, channel sweep measurement and BLE scans |
| `ble_scan.cpp/.h` | NimBLE active scan → updates the BLE device table |
| `ble_table.cpp/.h` | Pure logic — BLE device table (deduplicated by address, kept 30 seconds), distance bands, manufacturer names |
| `wifi_scan.cpp/.h` | Asynchronous WiFi scan (all channels or one) → updates the AP table and RSSI history |
| `wifi_sniff.cpp/.h` | Promiscuous channel sweep → control/data frame counts per channel |
| `traffic.cpp/.h` | Pure logic — averages channel activity (frames per second) |
| `analysis.cpp/.h` | Pure logic — AP table and RSSI history, channel cost, recommended and most congested channels, buzzer interval, sparkline range, line clipping |
| `canvas512.h` | Helper for drawing the 4 screens as one 512x64 sheet |
| `ui.cpp/.h` | Screen mode renderers |
| `u8g2_font_galmuri7_kr.h` | Galmuri7 Korean font (same as Agent_Monitor, SIL OFL 1.1) |
| `test/test_analysis.cpp` | Host unit tests for `analysis` · `traffic` · `ble_table` |
| `p0_spike/` | Temporary sketches and results for verifying the measurement features |

## Build

- Required libraries: **DeskStreamCore** (repository `libraries/`, `bash scripts/link-arduino-lib.sh`), **U8g2**, **NimBLE-Arduino** (verified with 2.5.0)
- Board: `esp32:esp32:makergo_c3_supermini`, CPU 160MHz, **Partition Scheme = Huge APP** (uses the shared DeskStream `partitions.csv` in the sketch folder)

```bash
arduino-cli compile --fqbn "esp32:esp32:makergo_c3_supermini:CDCOnBoot=default,CPUFreq=160,PartitionScheme=huge_app" Wireless_Analyzer
```

Host tests (from the repository root):

```bash
g++ -std=c++17 -IWireless_Analyzer Wireless_Analyzer/test/test_analysis.cpp Wireless_Analyzer/analysis.cpp Wireless_Analyzer/traffic.cpp Wireless_Analyzer/ble_table.cpp -o /tmp/test_analysis && /tmp/test_analysis
```
