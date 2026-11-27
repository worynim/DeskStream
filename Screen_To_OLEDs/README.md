**English** | [한국어](README.ko.md)

# Screen_To_OLEDs (Screen Streamer) - v2.3.1

Captures part of the PC screen in real time and sends it over WiFi (UDP) to DeskStream's four OLEDs (512x64).

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ Key Features

- Move and resize the capture area with an on-screen box, multi-monitor support
- 6 dithering modes (Atkinson by default), Numba JIT acceleration
- Send to one device (Direct) or to every device on the network at once (Broadcast)
- Pauses sending when nothing on screen changes, saving CPU and network

## 🎮 Buttons

| Button | Short | Long |
| :--- | :--- | :--- |
| BTN1 | — | Flip screen 180° |
| BTN2 | Cycle 4 brightness levels | Help screen |
| BTN3 | — | — |
| BTN4 | Close help/standby screen | — |

## 🚀 Setup

1. **Firmware** — pick it in the [web flasher](https://worynim.github.io/DeskStream/), or build it yourself. On first boot, connect to the `ESP_SCREEN_OLED` AP and set up WiFi.
2. **PC streamer** — download the macOS (Apple Silicon) or Windows (x64) app from the web flasher, or run it with Python.
   On macOS, allow the **Screen Recording** and **Local Network** permissions.

   ```bash
   pip install Pillow mss numpy numba
   python screen_streamer.py
   ```

3. Enter the board's IP or find it with `Scan`, then press `START STREAMING`.

## 📖 More

Architecture, feature details, notes on the executable and the settings file are in [GUIDE.md](GUIDE.md).
