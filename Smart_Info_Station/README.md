**English** | [한국어](README.ko.md)

# ESP32-C3 4-OLED Smart Info Station (Smart_Info_Station)

A desktop information display that uses an **ESP32 C3 SuperMini** board to drive four OLED displays independently and monitor a variety of information in real time: **time, weather, fine dust, YouTube subscriber count, stock markets (KOSPI, KOSDAQ, S&P 500), Bitcoin (BTC) and the KRW/USD exchange rate**.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## 🚀 Key Features

1.  **Hybrid I2C architecture (stability and speed)**
    - **OLED 1, 2 (Hardware I2C)**: Assigned the fast hardware controller so the time, which changes every second, is shown as smoothly as possible with no lag.
    - **OLED 3, 4 (Software I2C)**: Update less often, so a separate software I2C bus keeps the display stable and avoids physical conflicts.
2.  **Asynchronous data refresh on a FreeRTOS task (non-blocking)**
    - Fetching **weather**, **dust**, **YouTube** and **stock** data from servers takes several seconds, so it is fully separated into a background task (`dataTask`).
    - As a result, **the OLED clock and display never freeze and respond immediately**, even while data is being refreshed.
3.  **Efficient data collection and smart update policy (v2.7.3)**
    - **Visibility-based parsing**: Widgets not currently shown on the OLEDs do not call their servers unnecessarily. (All data is still fetched once at boot or on a forced update.)
    - **Market-hours optimization**:
      - **Korean markets (KOSPI, KOSDAQ, KPI200)**: Auto-update only on weekdays (Mon–Fri) between 08:30 and 16:10.
      - **US markets (S&P 500, NASDAQ)**: Auto-update only on weekdays (Mon–Fri) between 04:00 and 20:00 US Eastern time (pre-market to after-hours), with daylight saving time. (**v2.10.3**: Market hours are judged in each market's local time, so changing the time zone on the web page does not affect them.)
    - **Force update**: Long-press BTN4 to immediately re-read the data of all active widgets from the servers, whether the market is open or not.
4.  **Powerful web dashboard and live settings**
    - Change the router, YouTube channel, dust measuring station, time zone and more in real time from a web browser, without editing the source code.
    - **Integrated screen layout system**: A 4-column UI that matches the actual OLED arrangement lets you configure widgets per slot intuitively.
5.  **Stability and persistent storage**
    - **NVS persistence**: All settings are stored safely inside the ESP32 and kept across power cycles.
    - **Retry everywhere**: Every data widget retries up to 3 times and resets its state automatically for maximum stability.
6.  **User interaction and 4-button navigation (v2.7.3)**
    - **Audio feedback (non-blocking)**: Every button press gives audible feedback through the passive buzzer (GPIO 7). (Short: 3000Hz, Long: 2000Hz)
    - **Non-blocking buzzer engine**: Implemented with `millis()`, so the clock and data collection keep running while a tone plays.
    - **Startup melody**: A cheerful rising 4-note melody (C-E-G-C) signals that booting is complete.
    - **Initial loading priority**: At boot the current page's widgets load first and immediately show "Updating...", while the rest are collected in the background, improving the initial loading experience.
    - **BTN 1 (Flip/HUD)**: Cycles the whole screen through [Normal -> Mirror H -> 180° -> Mirror V] to suit HUD mounting and stand orientation. (**v2.8.0**: The help page shows the current mode name as text. / Holding it at power-on resets the WiFi settings.)
    - **BTN 2 (Brightness)**: 4 brightness levels from 25% to 100% to prevent OLED burn-in and protect night vision. (**v2.8.0**: The help page shows the current brightness level as a bar.)
    - **BTN 3 (Auto Loop)**: Page rollover that automatically cycles through pages 1–3 every 5 seconds. (Toggled with a long press)
    - **BTN 4 (Page/Refresh)**: A short press switches pages manually; holding it for more than 1 second forces an immediate refresh of the current data. (**v2.7.1**: Forces market data to be read even outside trading hours and refreshes the widgets on every page at once.)
    - **Button help page**: Page 5 on the device shows the current function of each button in real time. (**v2.8.0**: Added flip mode and brightness status display)

---
> 💡 Detailed update history and per-version changes are in [RELEASE_NOTES.md](./RELEASE_NOTES.md).
7.  **Shared display engine (DeskStreamCore, v2.10.0)**
    - Display transfer, buttons, buzzer, WiFi and NTP supervision use the shared library [DeskStreamCore](../libraries/DeskStreamCore/) — run `bash scripts/link-arduino-lib.sh` once from the repository root before building.
    - HW I2C 1.1MHz + fast SW I2C with direct register control (about 1.15MHz effective) send only changed tiles to the 4 screens in parallel (previously: synchronous full-frame transfer at HW 400kHz).

## 🛠 Hardware Components

-   **MCU**: ESP32 C3 SuperMini
-   **Display**: 0.96 inch SSD1306 OLED module (128x64px) x 4
-   **Buzzer**: Passive buzzer (GPIO 7)
-   The modules are connected as 2 pairs with I2C addresses `0x3C` and `0x3D`, each pair on a different pin bus (pins 5,6 / 2,3). (Pins 8 and 9 are used for other purposes)

## ⚙️ Arduino IDE Software Settings

1. **Partition Scheme**: You **must** choose `Huge APP (3MB No OTA / 1MB SPIFFS)`, because the Korean font included in the project (`u8g2_font_unifont_t_korean2`) exceeds the default partition size (~1.25MB)!
   - However, the sketch folder's `partitions.csv` always takes precedence (prebuild hook in `platform.txt`), so in practice this file's **shared DeskStream layout** (app0 2.5MB / spiffs `0x290000` 1.375MB) is used.
   - The spiffs start address is aligned to `0x290000`, the same as the `default` scheme, so **file system areas do not overlap** when you swap firmware with other projects (Korean clock, etc.).
   - This project does not use a file system (LittleFS/SPIFFS); all settings are stored in NVS (`0x9000`) and preserved under any scheme.

### Required Libraries
- `U8g2` (powerful graphics and OLED font library)
- `DeskStreamCore` (repository `libraries/DeskStreamCore` — run `bash scripts/link-arduino-lib.sh` once before building)
- `UrlEncode` (URL encoding library for HTTP web requests)

## 📁 File Structure by Widget

- `Smart_Info_Station.ino` : 4-button event handling, page switching logic, and the core `setup()` and `loop()`. (Keeps the main framework lightweight)
- `config.h` : Base settings file defining global data types (`WidgetType`), `WidgetUpdateInfo` holding update state, and other global pins/constants.
- `display_utils.h / .cpp` : Shared graphics helpers such as text centering and the update status dot (`.`) animation (display transfer is `ds::DisplayCore`).
- `widgets_display.h / .cpp` : Independent rendering logic that nicely `render()`s each widget — clock, calendar, icons, coins, stocks, etc. — on the OLEDs.
- `data_manager.h / .cpp` : The main collection engine; manages the headers with the network parsing functions for weather, fine dust, stocks, coins, etc., collects data through the background `dataTask`, and manages the state (`update_flag`).
- `web_handlers.h / .cpp` : Generates the internal settings web page UI reachable after connecting to WiFi, and saves the entered settings (channel, region, etc.) to flash (`handleRoot`, `handleSet`).
- `weather.h`, `dust.h`, `youtube.h`, `market.h`, `btc.h`, `usdkrw.h` : Declarations of the parsing function for each individual data source. (With their own include guards and extern variable declarations)
- `stream_scanner.h` : Scanner that reads the weather, fine-dust and YouTube pages through a fixed-size buffer and extracts the value after each key, so even a multi-megabyte line uses no extra memory.

## 📝 Quick Start

1. After flashing the code successfully, power it on and screen 1 shows **"WiFi Connecting..."**.
2. Connect to the **`Smart_Info_Station`** hotspot in the Wi-Fi settings of your PC or smartphone.
3. Enter your home or office router details on the portal page (or `192.168.4.1`).
4. When the connection finishes, the newly assigned **IP address (e.g. `192.168.0.x`)** appears at the bottom of screen 1.
5. Open that IP address in a web browser on a smartphone/PC on the same network to freely set local information, the target YouTube channel and your country's **time zone (offset)**!
