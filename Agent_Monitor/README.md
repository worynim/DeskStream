**English** | [한국어](README.ko.md)

# 🤖 Agent_Monitor — AI Development Agent Monitor (v0.8.2)

A desk monitor that shows the work status of Claude Code sessions on the DeskStream 4-OLED in real time.
Each OLED handles one session and shows **thinking / running / waiting for approval / waiting for input / done / error**, with a buzzer alert when the state changes.

> Current stage: **P4 — BLE · signing · tray · web flasher** complete. Plan and research: `PLAN.md`, hook verification results: [p0_spike/RESULTS.md](./p0_spike/RESULTS.md)

## ✨ Key Features

- **Session status**: one screen per session, tool name and command summary, elapsed time
- **Approve/deny permission requests with the device buttons** — waits together with the terminal dialog; whichever answers first decides (no auto-allow)
- **PC commands**: long-press a button to bring an app forward, run a shell command or type keys
- **Connection**: USB or BLE (signed with a key registered over USB), menu bar tray app
- Shows Claude Code usage (5-hour and weekly) on empty screens; Claude Code is unaffected even if the daemon is off

```
Claude Code ──HTTP hook──▶ PC daemon (bridge_py) ──USB or BLE──▶ DeskStream
```

## 🎮 Buttons

| Screen | Action | Function |
|:--|:--|:--|
| Overview | BTN i short / long | Slot i detail screen / run PC command i |
| Detail | BTN1 · BTN2 | Back to overview · close session |
| Detail | BTN3 · BTN4 | Previous · next page of the last reply |
| Approval | BTN1 · BTN2 · BTN3 · BTN4 | **Approve** · next request · **Deny** · later (decide in the terminal) |

## 🚀 Setup

1. **Firmware** — choose Agent Monitor in the [web flasher](https://worynim.github.io/DeskStream/), or build it yourself.
2. **PC daemon** — download the macOS (Apple Silicon) or Windows (x64) app from the web flasher, or run it with Python.

   ```bash
   cd Agent_Monitor/bridge_py
   pip install -r requirements.txt
   python3 agent_bridge.py tray
   ```

3. **Register the signing key** — run it once connected over USB (`--transport usb`). After that, use BLE.
4. **Connect Claude Code hooks** — "Install Claude Code hooks" in the tray menu, or `python3 agent_bridge.py install-hooks`.

## 📖 More

Screens, BLE security, the settings file (mute, brightness, usage polling, PC commands), Windows notes, files and tests are in [GUIDE.md](GUIDE.md).
