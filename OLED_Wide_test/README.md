**English** | [한국어](README.ko.md)

# ESP32-C3 Ultra-Wide OLED (512x64) Matrix - v1.3.1

A graphics demo and test that joins the four OLEDs side by side into one **512x64 ultra-wide screen**.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ Key Features

- Line, rectangle, circle and triangle drawing across the full 512px, plus a scrolling ticker demo
- Sends only the changed parts for smooth 30 FPS, reports live FPS over serial

## 🎮 Buttons

| Button | Short |
| :--- | :--- |
| BTN 1 | Flip screen 180° (transfer order also becomes 4-3-2-1; kept after power-off) |

## 🚀 Setup

Pick it in the [web flasher](https://worynim.github.io/DeskStream/), or build it yourself (needs U8g2, LovyanGFX and DeskStreamCore).

## 📖 More

Rendering approach, frame pacing, hardware and the demo list are in [GUIDE.md](GUIDE.md).
