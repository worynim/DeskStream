**English** | [한국어](GUIDE.ko.md)

# DS DECK Detailed Guide - v1.6.1

The detailed companion to the [README](README.md).

## 🛠 Required Development Environment
- **Board Manager**: ESP32 by Espressif Systems (v3.x or later recommended)
- **Board Selection**: ESP32C3 Dev Module

## 📚 Required Libraries (all must be installed)

### 1. Install from the Library Manager (searchable)
| Library | Purpose |
| :--- | :--- |
| **U8g2** | Controls the 4 OLED displays |
| **DeskStreamCore** | Repository `libraries/DeskStreamCore` — display transfer, buttons, buzzer, WiFi setup portal (v1.6.0, run `bash scripts/link-arduino-lib.sh` once before building) |
| **HijelHID\_BLEKeyboard** | BLE keyboard for the latest ESP32 3.x environment (NimBLE based) |
| **ArduinoJson** | Manages device settings |

*(Phase 3 of development added WiFi and large JSON handling. A 4096-byte DynamicJson structure was adopted to avoid size errors at compile time.)*

---

## 🚀 Hardware Pin Map (DeskStream Board)
- **Buttons**: GPIO 1, 4, 10, 9
- **I2C (HW)**: SDA(5), SCL(6) - OLED 1, 2
- **I2C (SW)**: SDA(2), SCL(3) - OLED 3, 4
- **Buzzer**: GPIO 7


---

## 📖 Usage Guide

### 1. Initial Wi-Fi and Network Setup
Follow these steps when turning the device on for the first time or changing networks.
1. When the device powers on, the OLEDs show a `Wi-Fi Setup` message.
2. On a smartphone or PC, search the Wi-Fi list and connect to **`DS_DECK_SETUP`**.
3. On the setup page that opens automatically, enter the SSID and password of your router.
4. Once connected, the device's **IP address** is shown on the OLED.

### 2. Bluetooth Pairing (PC/Mac)
Separately from the Wi-Fi setup, a Bluetooth connection is needed to use the keyboard features.
1. Open the Bluetooth settings on your PC or Mac.
2. In the device search, select the device named **`DS DECK`**.
3. Once it shows as connected, keyboard macros work immediately.
   - *Tip*: If the connection drops, rebooting the device or turning the PC's Bluetooth off and on reconnects automatically.

### 3. Web Dashboard (Configuration)
Once you know the IP address, you can configure macros freely from a web browser.
- Long-press BTN4 to see the IP address.
- Enter the IP shown on the OLED (e.g. `192.168.0.22`) in the browser address bar.
- The **DS DECK CONTROL PANEL** opens, where you can edit the label, mode and macro content of the 4 buttons in real time.
- Press **Save & Apply** and the device beeps and applies the settings immediately.

### 4. Button Modes and Rules (optimized for macOS)
- **Plain text (Macro)**: Types the long string you entered.
- **Browser mode (Browser)**: Uses the registered URL to summon macOS Spotlight and opens it right away in your default web browser. (⚠️ When using the browser launcher, the Mac must be in **'English input'** mode for Spotlight to recognize the address reliably.)
- **Korean/English switch token**: Put **`[#CAPS#]`** in the middle of a string and the standard macOS shortcut (`Ctrl + Space`) is performed automatically.
  - *Example*: `Hello[#CAPS#]안녕` -> types Hello, switches Korean/English automatically, then types 안녕.
- **Shortcut (Combo) mode**: Choose modifier keys such as `CMD`, `Shift`, `Alt` together with `F1–F12`, arrow keys and more to perform combined shortcuts.

### 4. Sound and Visual Feedback
- **Buzzer alerts**: A short beep plays when settings are saved or a button is pressed, so you can hear that it worked.
- **OLED status**: Each button's current function (label or media icon) is clearly shown on its own OLED.

---

## ⌨️ Advanced Korean/English Auto Switching (macOS only)

To type perfectly on macOS regardless of the input language state, DS DECK works together with **Karabiner-Elements**.

### 1. Karabiner Project Setup (required)
Copy the [`macro_language.json`] file in the project folder to the following path.
*   **Destination**: `~/.config/karabiner/assets/complex_modifications`

### 2. Enable the Rule in the Karabiner App
1. Launch the **Karabiner-Elements** app.
2. Click the **Complex Modifications** tab in the left menu.
3. Press the **Add rule** button at the bottom.
4. Find the **"매크로 키보드 한/영 강제 할당"** (macro keyboard Korean/English forced assignment) entry in the list.
5. Click the **Enable** button on the right to activate it.

---

## ⚡ Premium Features
- **Reliable Korean/English switching engine**: Instead of a simple toggle, it force-assigns `F19` (English) and `F20` (Korean) for typo-free typing.
- **Convenient macro input**: Enter frequently used sentences or complex shortcuts with a single button.
- **Full macOS compatibility**: An optimized input environment through Karabiner integration.
- **Dirty-checking display**: An algorithm that sends only changed pixels extends OLED life and improves responsiveness.

---

## ⚠️ Notes
- This project is optimized for **macOS**.
- Windows users need separate OS settings for Korean/English switching. (Planned for a future update)
- When compiling, set `Partition Scheme` to **`Huge App`** or **`No OTA (2MB APP/2MB SPIFFS)`**.
  - However, the sketch folder's `partitions.csv` always takes precedence (prebuild hook in `platform.txt`), so in practice this file's **shared DeskStream layout** (app0 2.5MB / spiffs `0x290000` 1.375MB) is used.
  - The spiffs start address is aligned to `0x290000`, the same as the `default` scheme, so **macro settings (`/config.json`) are preserved** even when you swap firmware with other projects (Korean clock, etc.).
