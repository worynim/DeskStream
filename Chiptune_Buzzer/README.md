**English** | [한국어](README.ko.md)

# 🎵 DeskStream Chiptune Buzzer (Buzzer Chiptune Test)

A test sketch that plays 4-channel chiptune music through the buzzer (GPIO 7) of the DeskStream board (ESP32-C3).

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

It re-ports to the ESP32-C3 the Arduino version by Stimmer ([Instructables "Arduino Chiptunes"](https://www.instructables.com/Arduino-Chiptunes/)) of Linus Åkesson's [Hardware Chiptune](http://www.linusakesson.net/hardware/chiptune.php) player.

---

## 📌 Key Features

1. **4-channel software synthesizer**
   - Each channel selects one of triangle, sawtooth, pulse (variable duty) or noise
   - Pitch generated with a 16-bit phase accumulator (DDS)
2. **Tracker-style song playback**
   - Unpacks bit-packed song data (2105 bytes) in real time while playing
   - Instrument scripts support arpeggio, vibrato, pitch bend, portamento, volume fade and PWM timbre modulation
3. **PWM audio output**
   - Drives the buzzer with a LEDC PWM 78.125kHz (8-bit) carrier, using it like a simple 8-bit DAC
   - Works because the buzzer coil and diaphragm follow the PWM average value
4. **Output cut-off when the song ends/stops**
   - When silent, the PWM duty drops to 0 so no unnecessary current flows through the buzzer coil

---

## 🛠️ Hardware

- **MCU**: ESP32-C3 (RISC-V single-core, 160MHz)
- **Buzzer**: Passive buzzer (GPIO 7 → 1kΩ → NPN transistor → 100Ω → buzzer → V_IN)
- **Buttons**:
  - **BTN 1 (GPIO 1)**: Play from the beginning
  - **BTN 2 (GPIO 4)**: Stop
- The OLEDs are not used.

---

## 🎮 Controls

| Input | Action |
| :--- | :--- |
| Power on / reset | Starts playing automatically |
| **BTN 1** | Play again from the beginning |
| **BTN 2** | Stop playback |

The serial monitor (115200bps) prints the `play` / `stop` / `end` state.

---

## ⚙️ How It Works

```
[Hardware timer ISR, ~15.9kHz]            [loop(), ~88Hz ticks]
 ├ Output previous sample as LEDC duty      ├ Parse song order / tracks (one row every 5 ticks)
 ├ Update 32-bit LFSR noise                 ├ Run instrument scripts
 ├ Mix 4 channel waveforms → 8-bit sample   └ Update frequency, volume, duty, vibrato
 └ tickCount++ every 180 samples ────────→ calls playroutine()
```

| Item | Value |
| :--- | :--- |
| Sample rate | 1MHz timer / 63 ≈ **15,873Hz** |
| Player tick | Sample rate / 180 ≈ **88Hz** |
| Song progress | 1 row every 5 ticks, 32 rows per track, 55 song sections (about 101 seconds) |
| PWM | 80MHz / 256 / 4 = **78,125Hz**, 8-bit |

---

## 🔁 Changes from the Original (AVR)

| Original (Arduino Uno) | This sketch (ESP32-C3) |
| :--- | :--- |
| Timer0 CTC interrupt | `timerBegin()` hardware timer ISR |
| Timer2 Fast PWM → `OCR2B` (pin 3) | LEDC PWM (GPIO 7). The ISR updates the registers directly through `hal/ledc_ll.h` |
| `typedef char s8;` | `typedef int8_t s8;` (on RISC-V `char` is unsigned, so leaving it breaks pitches and effects) |
| Shared `timetoplay++ / --` variable | The ISR only increments `tickCount`; loop compares it with `ticksDone` (removes the race) |
| `main()`, `cli/sei`, `CLKPR`, `DDRx` | Changed to `setup()/loop()`, AVR register code removed |
| LED blinking via `PORTB` (pin 10, 12) | Removed |
| Keeps the last sample value when the song ends | Cuts output with PWM duty 0 after 1.5 seconds |

Defensive code was also added to block out-of-range command and note numbers, and to prevent infinite loops in instrument scripts (max 64 commands per tick).

---

## 🔧 Adjustable Settings (top of `Chiptune_Buzzer.ino`)

| Constant | Default | Description |
| :--- | :--- | :--- |
| `TICK_SAMPLES` | 180 | Tempo. Larger values are slower |
| `SAMPLE_PERIOD` | 63 | Sample period (µs). Changing it also changes the pitch |
| `PWM_FREQ` | 78125 | PWM carrier frequency |
| `BUZZER_PIN` / `BTN_PLAY_PIN` / `BTN_STOP_PIN` | 7 / 1 / 4 | Pin settings |

---

## 🔊 Notes on Sound Quality

- The buzzer resonates at 2.7kHz, so **low notes (bass channel) are weak** and the sound is sharp with the 2–4kHz band emphasized.
- Melodies and arpeggios are clearly audible but sound lighter than the original.
- It is single-core, so when combined with other projects the ISR can cut in between SW I2C transfers and lower the display fps slightly. With Wi-Fi in use, interrupt latency can cause noise.

---

## 📁 Files

- `Chiptune_Buzzer.ino`: Sound ISR, song player, button handling
- `song_data.h`: Song data (PROGMEM) and `TRACKLEN` / `MAXTRACK` / `SONGLEN` definitions

---

## 🧪 Build

- Board: `esp32:esp32:makergo_c3_supermini` (Arduino-ESP32 core 3.x)
- No external libraries

```bash
arduino-cli compile --fqbn esp32:esp32:makergo_c3_supermini Chiptune_Buzzer
```

---

## 📜 Credits

- Original player: Linus Åkesson, Hardware Chiptune — http://www.linusakesson.net/hardware/chiptune.php
- Arduino port: Stimmer (Arduino forum), Instructables "Arduino Chiptunes" by blinkyblinky
