**English** | [한국어](GUIDE.ko.md)

# Smart Info Station Detailed Guide - v2.14.1

The detailed companion to the [README](README.md).

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
    - **Korean / English page**: Pick 한국어 or EN at the top right of the web page. The choice is remembered in that browser; at first the page follows the browser language.
5.  **Stability and persistent storage**
    - **NVS persistence**: All settings are stored safely inside the ESP32 and kept across power cycles. The 20KB store is shared with other firmware, so per-screen settings are packed into one string per screen, and the web page reports "Save failed" if a write fails (**v2.13.0**).
    - **Retry everywhere**: Every data widget retries up to 3 times and resets its state automatically for maximum stability.
6.  **User interaction and 4-button navigation (v2.7.3)**
    - **Audio feedback (non-blocking)**: Every button press gives audible feedback through the passive buzzer (GPIO 7). (Short: 3000Hz, Long: 2000Hz)
    - **Non-blocking buzzer engine**: Implemented with `millis()`, so the clock and data collection keep running while a tone plays.
    - **Startup melody**: A cheerful rising 4-note melody (C-E-G-C) signals that booting is complete.
    - **Initial loading priority**: At boot the current page's widgets load first and immediately show "Updating...", while the rest are collected in the background, improving the initial loading experience.
    - **BTN 1 (Flip/HUD)**: Cycles the whole screen through [Normal -> Mirror H -> 180° -> Mirror V] to suit HUD mounting and stand orientation. (**v2.8.0**: The help page shows the current mode name as text. / Holding it at power-on resets the WiFi settings.)
    - **BTN 2 (Brightness)**: 4 brightness levels from 25% to 100% to prevent OLED burn-in and protect night vision. (**v2.8.0**: The help page shows the current brightness level as a bar.)
    - **BTN 3 (Auto Loop)**: Page rollover that automatically cycles through pages 1–3 at a set interval (3–60 s on the web page, default 5 s). (Toggled with a long press)
    - **BTN 4 (Page/Refresh)**: A short press switches pages manually; holding it for more than 1 second forces an immediate refresh of the current data. (**v2.7.1**: Forces market data to be read even outside trading hours and refreshes the widgets on every page at once.)
    - **Button help page**: Page 5 on the device shows the current function of each button in real time. (**v2.8.0**: Added flip mode and brightness status display)

---
> 💡 Detailed update history and per-version changes are in [RELEASE_NOTES.md](./RELEASE_NOTES.md).
7.  **Shared display engine (DeskStreamCore, v2.10.0)**
    - Display transfer, buttons, buzzer, WiFi and NTP supervision use the shared library [DeskStreamCore](../libraries/DeskStreamCore/) — run `bash scripts/link-arduino-lib.sh` once from the repository root before building.
    - HW I2C 1.1MHz + fast SW I2C with direct register control (about 1.15MHz effective) send only changed tiles to the 4 screens in parallel (previously: synchronous full-frame transfer at HW 400kHz).
8.  **More widgets and easier web settings (v2.11.0)** — all without API keys.
    - **New widgets**: overseas weather (Open-Meteo), world clock, exchange rates (JPY/EUR/CNY), Ethereum and D-day. World clock and D-day can be placed on several screens with **a different city or date per screen** (world clock defaults to New York).
    - **12/24-hour Korean and world clocks**: in 12-hour mode AM/PM appears at the top left.
    - **Search by name**: local weather by neighborhood name (KMA neighborhood search), overseas weather and world clock by city name (Korean or English), then pick from a list.
    - **Only the settings you need**: the web page shows only the setting sections used by widgets in the screen layout. Pressing Enter in a text box no longer saves; only the save button does.
    - **Display**: the local-weather top line shows the neighborhood name and forecast time, and the fine-dust top line shows the area name (e.g. "가산동 Air pollution") in the [Galmuri7](https://github.com/quiple/galmuri) font; the three weather columns are centered.
    - **YouTube subscribers**: instead of reading the channel page (about 2.5MB), it reads about 10KB using the internal request the YouTube web page itself makes (the channel's "Search" tab).
    - **Exchange-rate updates**: follow the FX market — 24 hours on weekdays (closed only from Fri 17:00 to Sun 17:00 New York time).
9.  **Sunrise/sunset and weekly forecast (v2.13.0)** — Open-Meteo, no API key.
    - **Sunrise/sunset**: sunrise and sunset times, day length, moon illumination (%), a drawn moon phase and its name. The moon phase is computed from the date and mirrored in the southern hemisphere.
    - **Weekly forecast**: weekday, weather icon and high/low for the next 5 days, starting today.
    - **Per-screen city**: world clock, sunrise/sunset and weekly forecast each take a city chosen per screen under "City per Screen" on the web page (defaults: New York for the world clock, Seoul for the others).
10. **Temperature graph and watchlist stocks (v2.14.0)** — no API key.
    - **Temperature graph**: line graph of today's 0–23h forecast temperature for the per-screen city (Open-Meteo), with a dotted line at the current time and the current temperature at the top right.
    - **Watchlist stock**: one Korean or overseas stock per screen (Naver Finance). Search by name on the web page (e.g. 삼성전자, apple) and pick one; it updates every minute while its market is open.
    - **D-day labels** are shown larger in the Galmuri11 (12px) font.

## 🛠 Hardware Components

-   **MCU**: ESP32 C3 SuperMini
-   **Display**: 0.96 inch SSD1306 OLED module (128x64px) x 4
-   **Buzzer**: Passive buzzer (GPIO 7)
-   The modules are connected as 2 pairs with I2C addresses `0x3C` and `0x3D`, each pair on a different pin bus (pins 5,6 / 2,3). (Pins 8 and 9 are used for other purposes)

## ⚙️ Arduino IDE Software Settings

1. **Partition Scheme**: You **must** choose `Huge APP (3MB No OTA / 1MB SPIFFS)`, because the app (about 1.43MB including its Korean fonts) exceeds the default partition size (~1.25MB)!
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
- `slot_city.h` : Per-screen city and the data of the widgets that use it — world clock, sunrise/sunset, weekly forecast, temperature graph (fetched by `get_world_clock()`, `get_sun()`, `get_weekly()` and `get_temp_graph()` in `data_manager.cpp`).
- `stock.h` : Per-screen watchlist stock and quote data (fetched by `get_stock()` in `data_manager.cpp`; name search via `/stock_search` in `web_handlers.cpp`).
- `stream_scanner.h` : Scanner that reads the weather and fine-dust pages and the YouTube response through a fixed-size buffer and extracts the value after each key, so even a multi-megabyte line uses no extra memory.
- `u8g2_font_galmuri7_kr.h` : Galmuri7 Korean font (2,350 common KS X 1001 Hangul syllables, about 35KB, SIL OFL 1.1) — for place names, weekdays and stock names.
- `u8g2_font_galmuri11_kr.h` : Galmuri11 12px Korean font (2,350 common KS X 1001 Hangul syllables, about 60KB, SIL OFL 1.1) — for D-day labels and Korean YouTube channel names. Syllables outside the 2,350 are not drawn in either font.

## 📝 Quick Start

1. After flashing the code successfully, power it on and screen 1 shows **"WiFi Connecting..."**.
2. Connect to the **`Smart_Info_Station`** hotspot in the Wi-Fi settings of your PC or smartphone.
3. Enter your home or office router details on the portal page (or `192.168.4.1`).
4. When the connection finishes, the newly assigned **IP address (e.g. `192.168.0.x`)** appears at the bottom of screen 1.
5. Open that IP address in a web browser on a smartphone/PC on the same network to freely set local information, the target YouTube channel and your country's **time zone (offset)**!
