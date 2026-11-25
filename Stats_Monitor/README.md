**English** | [한국어](README.ko.md)

# 🖥️ Stats_Monitor

A desk gadget that shows PC/Mac system metrics (CPU, GPU, RAM, SSD, temperature, power consumption) in real time.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## 🛠️ Hardware
- **MCU**: ESP32-C3
- **Display**: 4 x SSD1306 OLED (128x64)
- **Interface**: I2C (2 on HW, 2 on SW)

## 📡 Key Features
- Real-time CPU/GPU usage and temperature monitoring
- RAM/SSD usage, network traffic and power display
- **180-degree screen rotation (flip)**: Automatically reverses the screen order to match the physical placement
- **OLED brightness control**: 4 (customizable) brightness levels with a visual guide
- **Saved settings (NVS)**: The last rotation and brightness are saved to flash and restored after a reboot
- **Buzzer feedback**: Audible feedback on button presses and system start
- **Fast rendering optimization**: Smooth UI updates with dirty checking and fast GPIO

## 🕹️ Hardware Controls
| Button | Action (short press) | Description |
| :--- | :--- | :--- |
| **BTN 1** | Rotate screen 180° | Flips the display and reverses the data output order (4-3-2-1). |
| **BTN 2** | Brightness | Cycles OLED brightness through 4 levels (1 > 50 > 100 > 240). |
| **BTN 3** | (unassigned) | Reserved for future features. |
| **BTN 4** | Toggle help | Shows each button's function and the current settings (angle, brightness) on screen. |

## 🐍 Python Client Setup
The following setup is needed to send the PC's system metrics to the ESP32 over BLE.

> **Firmware build**: Display transfer, buttons and buzzer use the shared library [DeskStreamCore](../libraries/DeskStreamCore/) — run `bash scripts/link-arduino-lib.sh` once from the repository root before building (v1.6.0+).

### 1. Install Required Libraries
With Python 3.10 or later, run the following command to install the required packages.
```bash
pip install -r client_py/requirements.txt
```

**Main packages installed:**
- `psutil`: Collects system metrics (CPU, RAM, disk, network)
- `GPUtil`: Collects GPU information (NVIDIA)
- `bleak`: BLE (Bluetooth Low Energy) communication
- `Pillow`: Generates the system tray icon image
- `pystray`: Manages the interactive system tray icon

### 2. Run the Client
```bash
python client_py/main.py
```

### 3. Build a Standalone Executable (PyInstaller)
`client_py/stats_monitor.spec` builds an app that runs with a double-click, without installing Python. Cross-building for another OS is not possible, so build on each OS.
```bash
cd client_py
pip install -r requirements.txt pyinstaller
pyinstaller stats_monitor.spec --noconfirm
```
- **macOS (arm64)**: `dist/StatsMonitor.app` — menu bar only (no Dock icon). Allow the Bluetooth permission on first launch.
- **Windows (x64)**: `dist/StatsMonitor.exe` — a single executable with no console window.
- The app is unsigned, so on macOS it needs **System Settings → Privacy & Security → "Open Anyway"** once the first time.
- If you launch the executable inside the `.app` directly from a terminal, the Terminal becomes the owner of the Bluetooth permission and it may crash. Always launch it with a Finder double-click or `open`.

### 4. Release (GitHub Release)
Pushing a tag with the same version as `VERSION` in `stats_monitor.spec` makes the [Build PC clients](../.github/workflows/build-clients.yml) workflow build the macOS and Windows executables and upload them to a Release.
```bash
git tag Stats_Monitor-client-v2.4.2
git push origin Stats_Monitor-client-v2.4.2
```
- Outputs: `StatsMonitor-<version>-macos-arm64.zip`, `StatsMonitor-<version>-windows-x64.exe`
- When bumping the version, change both `VERSION` in the spec and the version in the `main.py` tray tooltip.
- After the Release is up, change `version` in [`firmware/clients.json`](../firmware/clients.json) to the same value and push to `main`, and the web flasher's download button points to the new version. (Check that it matches the spec with `node tools/collect-firmware.mjs --check`)

## 📋 Usage
1. Run the client program that sends data on the PC (`python client_py/main.py`)
2. When the ESP32-C3 board is powered, it connects over BLE automatically and shows data on the 4 OLEDs
3. Adjust the screen orientation or brightness with the hardware buttons if needed
