**English** | [한국어](GUIDE.ko.md)

# Screen_To_OLEDs Detailed Guide - v2.3.1

The detailed companion to the [README](README.md).

## 🏗 Architecture

The project follows a design modularized by function for high-performance streaming.

- **`screen_streamer.py`**: Main application. Runs the UI, manages the screen capture (producer) and data sending (sender) threads, and controls the whole pipeline.
- **`image_processor.py`**: Image processing module. Concentrates the fast dithering algorithms and 1-bit packing logic accelerated with Numba JIT.
- **`network_manager.py`**: Communication layer. Manages ESP32 discovery (IP discovery) and the UDP data streaming loop.
- **`overlay_manager.py`**: UI helper module. Handles the overlay window that visualizes the capture area and the drag control logic.
- **`config.json`**: External user settings file. Adjust the IP and system parameters without editing the source. (The executable uses `~/.deskstream/screen_to_oleds.json`)
- **`screen_to_oleds.spec`** · **`requirements.txt`**: PC client executable build (PyInstaller) and dependencies.

## 🚀 Key Features

### macOS Streamer (`screen_streamer.py`)
- **Intelligent capture engine**:
    - **Automatic monitor detection**: With 'Full', it automatically recognizes the monitor the capture handle is on and captures its whole area.
    - **Handle-anchored resizing**: Adjust the capture width precisely while the top-left handle position stays fixed.
    - **Free boundaries**: The box may be placed beyond the screen edge, maximizing multi-monitor usability.
    - **Live coordinate display**: A dedicated info box next to the handle shows the current `(x, y)` position.
- **Live source settings pipeline**:
    - Change **capture width, layout (horizontal/vertical) and overlay visibility** instantly without stopping the stream.
- **Numba JIT acceleration**: A numeric compiler processes even complex dithering algorithms in real time at C-level speed.
- **Direct Mode (1:1)**: Sends the screen directly to the device at the given IP address (default).
- **Broadcast Mode (1:N)**: Sends the screen simultaneously to every DeskStream device on the network, not just one.
- **Multiple binarization algorithms**: 6 modes — Atkinson (default), Threshold, Floyd-Steinberg, Stucki, Sierra, Ordered (Bayer).
- **Performance and stability optimizations**:
    - **Thread-safe settings cache**: Resolves UI variable contention between the main and background threads, preventing FPS drops.
    - **Generational GC strategy**: Splits short-cycle partial collections from long-cycle full collections to minimize stutter from GC spikes.
    - **Standard logging and status diagnostics**: The `logging` module records system status in real time, and network errors give immediate feedback in the UI.
    - **Externalized settings (`config.json`)**: Change the IP address and system limits easily without editing the source code.
    - **Retina high-resolution optimization**: Automatic downsampling cuts the physical pixel processing load of macOS Retina displays by 4×.
    - **Network pacing**: A small delay between UDP chunks (500μs) minimizes packet loss.
    - **Static frame skipping**: A smart idle mode that drives system resource use (CPU, network) to a minimum when the screen does not change (with 100% exact change detection and a 1-second KeepAlive).

### ESP32 Firmware
- **Fast UDP reception**: Receives each frame split into 1024-byte chunks and rebuilds the 4096-byte 1-bit bitmap.
- **Dirty-tile rendering**: Sends only changed tiles over the HW and SW buses in parallel — DeskStreamCore `ds::DisplayCore::pushCanvas()` (v2.3.0+).
- **WiFi setup portal**: If no router is saved, it opens a setup AP (`ESP_SCREEN_OLED`) to receive the SSID and password from your phone (built into DeskStreamCore).
- **Non-blocking buzzer**: Indicates the operating state with gentle sounds.

## 🛠 Installation and Usage

### As an Executable (no Python needed)
Choose Screen To OLEDs in the [web flasher](https://worynim.github.io/DeskStream/) to download the macOS (Apple Silicon) or Windows (x64) app (GitHub Release `Screen_To_OLEDs-client-v<version>`).
- The app is unsigned, so on first launch macOS needs **System Settings → Privacy & Security → "Open Anyway"**, and Windows needs SmartScreen's **More info → Run anyway**.
- On macOS, grant the app the **Screen Recording** and **Local Network** permissions. Without Screen Recording, other apps' windows are left out and only the desktop is streamed, and a red bar at the top of the window explains how to set it. Because it is unsigned, you may need to grant them again after replacing the app with a new version.
- The app reads its settings from `~/.deskstream/screen_to_oleds.json` (same format as `config.json`; defaults are used if it does not exist).
- Numba is included, but the executable cannot use the disk cache, so it JIT-compiles for a few seconds on every start.
- Build it yourself: `pip install -r requirements.txt pyinstaller && pyinstaller screen_to_oleds.spec`


### macOS (Python)
1. Install the required libraries (essential for performance):
   ```bash
   pip install Pillow mss numpy numba
   ```
2. Run the streamer:
   ```bash
   python screen_streamer.py
   ```
3. Enter the ESP32's IP or press the `Scan` button to find it automatically.
4. Press the `START STREAMING` button to start streaming.

### Hardware (ESP32)
1. Upload the `.ino` file in the `Screen_To_OLEDs` folder with the Arduino IDE. (Required libraries: U8g2 · LovyanGFX + repository `libraries/DeskStreamCore` — run `bash scripts/link-arduino-lib.sh` once from the root)
2. At boot, connect to the WiFi AP shown on the OLED (`ESP_SCREEN_OLED`) and complete the network setup.
3. Enter the IP address shown after a successful connection into the streamer.

## 🕹 Hardware Controls (buttons)
- **BTN1**: Long press - rotate the screen 180 degrees (flip) / Short press - (reserved)
- **BTN2**: Short press - **cycle through 4 brightness levels** / Long press - **show the help screen**
- **BTN3**: (not used)
- **BTN4**: Short press - exit the help/idle screen / Long press - (reserved)
