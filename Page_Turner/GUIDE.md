**English** | [한국어](GUIDE.ko.md)

# Page Turner Detailed Guide - v1.1.0

The detailed companion to the [README](README.md).

## 📌 Key Features

1. **7 modes named after apps**: Pre-tuned through testing on real Android devices with the signal each app responds to best (volume keys, arrow keys, mouse wheel, PageUp/Down) and the scroll amount.
2. **3 paired-device slots**: Remembers a phone, tablet and PC in slots 1–3, one each, and switches between them with two button presses. No re-pairing needed.
3. **Portrait screens**: Each OLED shows the role of the button next to it with arrows and icons. Brightness is set to minimum and everything is drawn as outlines only — no filled boxes or inversion — so it does not glare in the dark (fewer lit pixels also use less power).
4. **Configure with buttons only**: Wheel steps, arrow key count, gap, cursor hiding, delayed shutter time and button sound. Settings plus the last mode and slot are saved and kept when the power is off.
5. **Battery saving**: Screen off after 1 minute without input, deep sleep after 5 minutes without input.

---

## 📱 Modes (verified on Android)

| # | Mode | Signal sent | Defaults | Notes |
|---|---|---|---|---|
| 1 | YouTube | Mouse wheel | Steps x3, Gap 30ms, Cursor Hide | Flip through Shorts |
| 2 | Kakao Webtoon | Arrow keys ↑↓ | Keys x4, Gap 30ms | |
| 3 | Kakao Novel | Volume +/− | — | Turn on **page turning with volume keys** in the app |
| 4 | Series Webtoon | Mouse wheel | Steps x10, Gap 80ms, Cursor Hide | |
| 5 | Series Novel | Volume +/− | — | Turn on **page turning with volume keys** in the app |
| 6 | Camera | BTN1 volume+ / BTN2 delayed shutter | Timer 3s | BTN2 shoots after a few seconds (press again during the countdown to cancel) |
| 7 | PPT | PageUp / PageDown | — | PC presentations |

- **Wheel modes** do not send the wheel all at once; they send one step at a time with a gap for smooth scrolling.
- **Cursor Hide**: Moves the mouse cursor to the right edge of the screen before sending the wheel so it covers less of the screen.

---

## 🎮 Controls

Holding the device upright, the OLEDs are OLED1 to OLED4 from the top, with BTN1 to BTN4 next to each screen.
Each screen shows **what that button does at the top**, an icon in the middle, and **the button's label at the bottom** (`BTN1`, `MODE 4/7`, `BT1 ON` …).

| Button | Normal screen | Inside the settings menu |
|---|---|---|
| **BTN1** | Previous page / scroll up / shutter | Decrease value (choose Off·Show) |
| **BTN2** | Next page / scroll down / delayed shutter | Increase value (choose On·Hide) |
| **BTN3** | Next mode · **long = choose device (slot)** | Cancel (discard changes) |
| **BTN4** | Open the settings menu | Next item; saves on the last item |

### Settings

| Item | Meaning | Values | Modes |
|---|---|---|---|
| Steps | Wheel steps sent per press | x1 – x30 | YouTube, Series Webtoon |
| Keys | Arrow key presses sent per press | x1 – x20 | Kakao Webtoon |
| Gap | Interval between steps/keys | 10 – 250ms | The three modes above |
| Cursor | Move the cursor to the screen edge before the wheel | Hide / Show | YouTube, Series Webtoon |
| Timer | Time until the BTN2 delayed shutter fires | 2 / 3 / 5 / 10s | Camera |
| Beep | Button sound | On / Off | All modes |

---

## 🔗 Paired-Device Slots (BTN3 long)

Slots 1–3 each remember one device. The current slot appears on the OLED4 label as `BT1 ON` (connected) / `BT1 wait` (waiting).

| On the device selection screen | Action |
|---|---|
| BTN1 / BTN2 / BTN3 short | Select slot 1 / 2 / 3 (double outline) → press **the same button once more** to use that slot |
| BTN1 / BTN2 / BTN3 long | **Wait to pair a new device** in that slot |
| BTN4 | Cancel |

- The slot in use has an outline around its number (`In use`).
- Using a paired slot connects **only that device**. The remote is invisible to other devices.
- Choosing an empty slot or long-pressing a slot shows the **waiting to pair** screen. Find `DS Page Turner` on the new device and pair it.
  - The device previously in the slot is replaced **at the moment the new device pairs**.
  - **BTN3 short** or **60 seconds elapsed** → returns to the device you were using.
  - If pairing fails while changing a slot with all 3 slots full, the slot's previous device has to be paired again.
- On first power-up, already-paired devices are placed into empty slots automatically.

### First Connection

1. Turn on the board. If no device is paired, the slot 1 waiting-to-pair screen appears right away.
2. Find **`DS Page Turner`** in your phone's Bluetooth settings and pair it. OLED4 changes to `BT1 ON`.
3. Use BTN3 to choose the mode for the app you want to use.

---

## 🔋 Screen Off and Deep Sleep

| After the last button press | Action |
|---|---|
| 1 minute | Turns the screen off (the Bluetooth connection stays). The next first button press **only turns the screen on** and sends no signal. |
| 5 minutes | Enters deep sleep (the connection drops). **Wake it with BTN1 or BTN2** — BTN3 and BTN4 cannot wake it (the ESP32-C3 can wake from deep sleep only on GPIO 0–5). After waking it reconnects to the device you were using within a few seconds. |

USB also disconnects during deep sleep, so wake it with a button first before uploading firmware.

---

## 🛠️ Hardware

- **MCU**: ESP32-C3 SuperMini
- **Display**: 0.96" SSD1306 128×64 OLED × 4 (rotated to portrait, 64×128 per screen)
- **Buttons**: BTN1 (GPIO 1), BTN2 (GPIO 4), BTN3 (GPIO 10), BTN4 (GPIO 9)
- **Buzzer**: Passive buzzer (GPIO 7)
- **Power**: Connect a battery separately for handheld use (to be built into the V2 hardware).

---

## 📁 Project Files

| File | Contents |
|---|---|
| `Page_Turner.ino` | Buttons → modules, screen off and deep sleep |
| `config.h` | Version, BLE name, pairing wait time, OLED brightness |
| `model.h/.cpp` | Mode table, settings items, value lists (hardware-independent) |
| `menu.h/.cpp` | Settings menu state machine (hardware-independent) |
| `shutter_timer.h/.cpp` | Delayed shutter countdown (hardware-independent) |
| `power_policy.h` | Idle time → screen off / deep sleep decision (hardware-independent) |
| `slots.h/.cpp` | Paired-device slot table (hardware-independent) |
| `hid_ble.h/.cpp` | BLE HID (keyboard, media keys, mouse wheel), signal sending task, slot connection and pairing |
| `view.h/.cpp` | Drawing the 4 screens |
| `settings_store.h/.cpp` | Settings and last mode saved to NVS (`ds::ConfigStore`) |
| `build_opt.h` | NimBLE subscription (CCCD) storage limit 8 → 24 (prevents pairings being erased from the second device on) |
| `partitions.csv` | Shared DeskStream partitions |
| `test/` | Host (PC) unit tests |
| `DESIGN.md` | Design document |

Display, buttons, buzzer and settings storage use the shared library [DeskStreamCore](../libraries/DeskStreamCore/), and BLE uses **NimBLE-Arduino 2.5.0**.
Before building, run `bash scripts/link-arduino-lib.sh` once per PC from the repository root.

---

## 🚀 Build · Upload · Test

```bash
arduino-cli compile --fqbn "esp32:esp32:makergo_c3_supermini:CDCOnBoot=default,CPUFreq=160,PartitionScheme=huge_app" Page_Turner
```

```bash
arduino-cli upload --fqbn "esp32:esp32:makergo_c3_supermini:CDCOnBoot=default,CPUFreq=160,PartitionScheme=huge_app" -p /dev/cu.usbmodem* Page_Turner
```

```bash
bash Page_Turner/test/run_all.sh
```

In the Arduino IDE, choose `Partition Scheme = Huge APP` (the sketch folder's `partitions.csv` is used).

---

## 🗺️ Next Version

- Testing on real iOS devices (keyboard and volume keys expected to work; the wheel may need AssistiveTouch)
- V2 hardware: built-in battery, battery level display
