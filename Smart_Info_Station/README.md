**English** | [한국어](README.ko.md)

# ESP32-C3 4-OLED Smart Info Station (Smart_Info_Station) - v2.14.1

A desktop information display: pick information widgets for the four OLEDs and watch them update in real time.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ Key Features

- **Widgets**: Korean and world time, local and overseas weather, fine dust, YouTube subscribers, stock indexes (KOSPI, KOSDAQ, S&P 500), Bitcoin and Ethereum, exchange rates, D-day, sunrise/sunset and moon phase, weekly weather, temperature graph, watchlist stocks
- **Web settings**: screen layout, channels, regions, cities and stocks set in the browser by searching names (Korean/English)
- Data is fetched in the background so the screens never freeze; stock data updates only while markets are open
- Settings are stored in NVS and kept after power-off

## 🎮 Buttons

| Button | Short | Long |
| :--- | :--- | :--- |
| BTN 1 | Cycle screen orientation (Normal → Mirror H → 180° → Mirror V) | — |
| BTN 2 | 4 brightness levels | — |
| BTN 3 | — | Auto page cycling on/off |
| BTN 4 | Switch page | Refresh all widgets now |

Holding BTN 1 at power-on resets the WiFi settings. Page 5 shows button help.

## 🚀 Setup

1. Pick it in the [web flasher](https://worynim.github.io/DeskStream/), or build it yourself with **Huge APP** as the `Partition Scheme`.
2. On first boot, connect to the `Smart_Info_Station` hotspot and set up WiFi at `192.168.4.1`.
3. Open the IP shown on screen 1 in a browser and set up widgets, regions and so on.

## 📖 More

Feature details, hardware, build settings and file structure are in [GUIDE.md](GUIDE.md); per-version changes are in [RELEASE_NOTES.md](./RELEASE_NOTES.md).
