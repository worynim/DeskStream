**English** | [한국어](README.ko.md)

# 🌍 World_Clock: Four-City World Clock (v0.1.0)

A clock that shows **the current time of four places on Earth**, one on each of the DeskStream board's four OLEDs.
You choose the places **by pressing the screens**, with no web page (on this board, pressing an OLED screen presses that screen's button).

https://youtu.be/2hlVDkWg71k

> **0.1.0 is the first release, checked only with the PC screen preview, host tests and builds. It has not yet been checked on a real board.**

## ✨ Key Features

- **One screen = one city**: large AM/PM, sun/moon icon and a 0–24h day bar show morning/afternoon and day/night
- Screen 1 is the reference city — shows `+1 day` / `-1 day` when the date differs
- **113 cities** (Korean and English names), 30- and 45-minute time zones and automatic daylight saving time
- 12/24-hour · brightness · Korean/English switching; settings are saved and kept after power-off

## 🎮 Pressing (clock screen)

| Screen | Short | Long |
|:--|:--|:--|
| Screen 1 | 12h ↔ 24h | Set this screen's place |
| Screen 2 | 4 brightness levels | Set this screen's place |
| Screen 3 | 3-second info card (date · UTC offset · time difference) | Set this screen's place |
| Screen 4 | Korean ↔ English | Set this screen's place |

While setting a place, the other three screens become **map (cancel) · ◀ west · east ▶** buttons; a short press on the screen being edited saves.

## 🚀 Setup

1. Choose "World Clock" in the [web flasher](https://worynim.github.io/DeskStream/). When building yourself, choose **Huge APP** for `Partition Scheme`.
2. If no WiFi is saved, connect your phone to `World_Clock_Setup` and set up WiFi at `192.168.4.1` (shared with other DeskStream projects).

## 📖 More

Screen layout, place setting, time zone rules, files and build are in [GUIDE.md](GUIDE.md);
the development plan and design rationale are in [PLAN.md](./PLAN.md).
