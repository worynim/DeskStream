**English** | [한국어](README.ko.md)

# 🖥️ Stats_Monitor - v1.6.1

A desk gadget that receives PC/Mac system metrics (CPU, GPU, RAM, SSD, temperature, power) over BLE and shows them on the four OLEDs in real time.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ Key Features

- CPU/GPU usage and temperature, RAM/SSD usage, network and power
- Connects to the board over BLE automatically when the PC client (tray app) starts
- Screen flip 180° and 4 brightness levels — kept after power-off

## 🎮 Buttons

| Button | Short |
| :--- | :--- |
| BTN 1 | Flip screen 180° (output order reversed to 4-3-2-1) |
| BTN 2 | Cycle 4 brightness levels |
| BTN 3 | (unassigned) |
| BTN 4 | Help on/off |

## 🚀 Setup

1. **Firmware** — pick it in the [web flasher](https://worynim.github.io/DeskStream/), or build it yourself.
2. **PC client** — download the macOS (Apple Silicon) or Windows (x64) app from the web flasher, or run it with Python.

   ```bash
   pip install -r client_py/requirements.txt
   python client_py/main.py
   ```

## 📖 More

Client packages, building the executable and the Release process are in [GUIDE.md](GUIDE.md).
