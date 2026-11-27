**English** | [한국어](GUIDE.ko.md)

# Sound Visualizer 4 OLEDs Detailed Guide - v3.6.0

The detailed companion to the [README](README.md).

## 🚀 Key Features and Performance Optimizations

### 1. ESP-DSP Integer FFT Acceleration (High-Speed Engine)
*   **Integer-only math**: Uses a 16-bit integer (`int16_t`) FFT to remove the `float` bottleneck caused by the ESP32-C3 having no FPU.
*   **Zero-loss truncation guard**: Amplifies the input signal **16× (<< 4)** to keep precision and prevent data loss during integer math.
*   **Fast magnitude approximation**: Uses the approximation `max(a, b) + 0.5 * min(a, b)` instead of the heavy `sqrt()`, cutting computation time by **more than 90%**.
*   **Manual window generation**: A built-in window generator (Hann, Hamming, etc.) based on the math formulas, avoiding library version fragmentation.

### 2. High-Resolution Visualization and UI Animation
*   **128 bands**: Improved performance allows processing 128 fine frequency bands in real time.
*   **Sum pooling (high-frequency weighting)**: To improve high-frequency visibility, it renders the **sum** of frequency energy rather than just the maximum.
*   **Peak hold animation**: For visual stability at high frame rates, peak dots hang for a while and then fall under gravity.
*   **Oscilloscope mode**: A real-time time-domain waveform mode with **auto-scaling** that detects the maximum amplitude every frame.
*   **6 visualization modes (v3.6.0)**: See the "Visualization Modes" table below. Switch modes with a short press of BTN 4.

### 3. Zero-Conflict Parallel Transfer Engine (Smart Flush 2.0)
*   **Distributed processing**: Even on the ESP32-C3's single core, FreeRTOS tasks split I2C transfers and CPU computation efficiently.
*   **Shared display engine (v3.5.0)**: Canvas transposition and parallel transfer of only changed tiles over HW (1.1MHz background task) and SW (fast routine) are handled by DeskStreamCore `ds::DisplayCore::pushCanvas()`.
*   **True parallelism**: While the hardware sends data, the CPU immediately starts bit-banging OLED 3 and 4. The two I2C buses transfer data at the same time, minimizing total latency.
*   **No mixed data**: Fully separated code paths eliminate data crosstalk between screens and graphics corruption.
*   **Tile-based dirty checking**: Detects changes in **8-pixel tiles** and sends only areas whose data actually changed, minimizing I2C bandwidth use.

### 4. Real-Time Sound Analysis (ADC DMA & Optimized FFT)
*   **ADC DMA continuous sampling**: Uses the **ADC Continuous Driver (DMA)** instead of a hardware timer to collect audio data automatically in the background, guaranteeing precise 40kHz sampling without CPU interference.
*   **Auto Gain Control (AGC)**: Adjusts sensitivity automatically to the input volume, with a noise suppression floor (`AGC_MIN_AMP`). Since **v3.3.0** it scales the mapped band amplitude and the waveform amplitude separately.
*   **Performance monitoring (debug mode)**: Measures FFT/render time and I2C transfer time separately in µs for detailed bottleneck profiling.

## 🛠 Hardware
- **MCU**: ESP32-C3 (single-core RISC-V, without FPU)
- **Display**: SSD1306 128x64 OLED x 4 (512x64 total)
- **Microphone**: Analog mic module (GPIO 0 / ADC1_CH0)
- **Pin map**:
  - HW I2C (OLED 1, 2): SDA(GPIO 5), SCL(GPIO 6)
  - SW I2C (OLED 3, 4): SDA(GPIO 2), SCL(GPIO 3)
  - Buttons: BTN1(GPIO 1), BTN2(GPIO 4), BTN4(GPIO 9)

## 📦 Required Libraries
To compile this source code, install the following libraries from the Arduino Library Manager:
*   **U8g2**: OLED display driver and font control
*   **DeskStreamCore**: Repository `libraries/DeskStreamCore` — 4-OLED transfer, buttons, buzzer (run `bash scripts/link-arduino-lib.sh` once from the root before building) (v3.5.0+)
*   **LovyanGFX**: Fast graphics drawing and virtual canvas library
*   **ESP-DSP**: Built-in library for hardware-accelerated FFT on the ESP32-C3 (Arduino ESP32 Core 3.0+ recommended)

## 🕹️ Controls and Buttons (Interface)
This project supports **non-blocking** short/long presses with passive buzzer (GPIO 7) feedback.

*   **BTN 1 (GPIO 1)**:
    *   **Short press**: Toggle 180-degree screen rotation (flip). (Saved to NVS automatically)
*   **BTN 2 (GPIO 4)**:
    *   **Long press**: Toggle the button help screen.
*   **BTN 4 (GPIO 9)**:
    *   **Short press**: Cycle visualization modes (in the "Visualization Modes" order below).
    *   **Long press**: Start noise calibration.

## 🎨 Visualization Modes (cycle with a short press of BTN 4)
| # | Mode | Display |
|---|---|---|
| 1 | Spectrum | 128-band bars + peak dots |
| 2 | Waveform | Oscilloscope waveform (auto-scaling) |
| 3 | Mirror | Bars mirrored above and below a center line + peak dots |
| 4 | Spectrogram | Horizontal = time (newest on the right), vertical = frequency (high at the top), intensity = Bayer-dithered dot density |
| 5 | DotMatrix | LED dot columns at 4px spacing, screen-inversion flash on every beat |
| 6 | Particles | Dots keep dropping from the frequency position of bands where spectrum bars rise (bass on the left → treble on the right), with probability proportional to intensity, and bounce off the floor (tail proportional to speed) |

*   Beat detection (DotMatrix flash): A beat is detected when the low-band (lowest `BASS_BANDS` bands) intensity exceeds `BEAT_RATIO` × its moving average and is at least `BEAT_MIN_LEVEL`, at most once every `BEAT_COOLDOWN` frames.
*   All tuning constants are in `config.h` (`BASS_BANDS`, `BEAT_*`, `DOT_*`, `PARTICLE_*`).

## 📺 Included Features (demo and visualizer)
- Drives the 4 OLEDs as one 512x64 wide canvas
- **Real-time sound visualizer**
  - 128-band spectrum with 2-pixel-thick peak dots
  - **Oscilloscope mode**: Real-time waveform visualization around a 0V baseline
  - **4 additional modes**: Mirror spectrum · spectrogram · dot matrix · particles
  - Smooth bar fall effect and hardware-accelerated FFT
  - Precise FPS control with hybrid delays (vTaskDelay + delayMicroseconds)
  - Button help overlay system
