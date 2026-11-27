**English** | [한국어](README.ko.md)

# ESP32-C3 Ultra-Wide OLED (512x64) Matrix (v3.6.0)

An audio visualizer that shows microphone sound in real time on a **512x64 ultra-wide screen** made of the four OLEDs.
ESP-DSP integer FFT reaches up to 70+ FPS.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ Key Features

- **6 modes**: spectrum (128 bands) · waveform · mirror · spectrogram · dot matrix · particles
- Automatic gain control (AGC) for the input level, noise calibration
- Analog microphone (GPIO 0) sampled at 40kHz over DMA

## 🎮 Buttons

| Button | Short | Long |
| :--- | :--- | :--- |
| BTN 1 | Flip screen 180° | — |
| BTN 2 | — | Button help |
| BTN 4 | Cycle visualization mode | Noise calibration |

## 🚀 Setup

1. Connect an analog microphone module to GPIO 0.
2. Pick it in the [web flasher](https://worynim.github.io/DeskStream/), or build it yourself (needs U8g2, LovyanGFX and DeskStreamCore).

## 📖 More

Performance optimizations, hardware, libraries and per-mode details are in [GUIDE.md](GUIDE.md).
