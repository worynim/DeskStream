**English** | [한국어](README.ko.md)

# ⏱️ DeskStream Universal Timer - v1.3.0

A countdown timer that shows hours, minutes, seconds and status on the four OLEDs, one each, set with the buttons below them.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## ✨ Key Features

- **One button per screen**: set hours · minutes · seconds with the button under each screen; hold to count up fast
- Screen flashing and a music-box alarm when time is up; any button restores the time you first started with
- **Remembers the last set time**: saved on START, so it starts from that time after power-off

## 🎮 Buttons

| Button | Short | Hold |
| :--- | :--- | :--- |
| BTN 1 | Hour +1 | Hours up fast |
| BTN 2 | Minute +1 | Minutes up fast |
| BTN 3 | Second +1 | Seconds up fast |
| BTN 4 | Start / pause / stop alarm | Reset to 00:00:00 (1 s) |

## 🚀 Setup

Pick it in the [web flasher](https://worynim.github.io/DeskStream/), or build it yourself.

```bash
arduino-cli compile --fqbn esp32:esp32:nologo_esp32c3_super_mini .
```

## 📖 More

Feature details, hardware and files are in [GUIDE.md](GUIDE.md).
