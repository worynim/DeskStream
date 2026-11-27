**English** | [한국어](README.ko.md)

# 📡 Wireless_Analyzer: 2.4GHz Wireless Environment Analyzer (v0.5.1)

A standalone device that uses the ESP32-C3's own WiFi/BLE radio as a measuring tool and shows the surrounding 2.4GHz environment on the 512x64 screen.
It never joins a router and needs no PC or internet connection. The on-device UI is in Korean (Galmuri7 font).

## ✨ Key Features

- **6 screens**: channel spectrum · channel congestion · AP list · AP tracking · BLE devices · summary
- **Recommended channel**: picks the quietest channel from same-channel and adjacent-channel overlap plus measured channel activity
- **AP tracking**: fast signal strength updates for the chosen AP with buzzer guidance (for finding a router)
- **BLE device count**: counts by distance band with names and manufacturers — device addresses are never shown or sent
- Mode, brightness and flip are saved and kept after power-off

## 🎮 Buttons

| Button | Short | Long |
|:--|:--|:--|
| BTN1 | Previous mode | Flip screen 180° |
| BTN2 | Next mode | Pause/resume scanning |
| BTN3 | List/tracking: previous (stronger) AP | Tracking: buzzer on/off |
| BTN4 | List/tracking: next (weaker) AP | Brightness level |

## 🚀 Setup

Choose "Wireless Analyzer" in the [web flasher](https://worynim.github.io/DeskStream/).
To build it yourself, choose **Huge APP** for `Partition Scheme`; it needs DeskStreamCore, U8g2 and NimBLE-Arduino.

## 📖 More

Per-screen details, channel cost calculation, measurement limits, serial debug, files, build and tests are in [GUIDE.md](GUIDE.md).
See [PLAN.md](./PLAN.md) for the development plan and [p0_spike/RESULTS.md](./p0_spike/RESULTS.md) for measurement verification results.
