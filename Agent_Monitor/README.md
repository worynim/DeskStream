**English** | [한국어](README.ko.md)

# 🤖 Agent_Monitor — AI Coding Agent Monitor (v0.8.2)

A desk monitor that shows the working state of Claude Code sessions live on the DeskStream 4-OLED.
Each OLED handles one session and shows **thinking / running (tool · command) / awaiting approval / awaiting input / done / error**,
and the buzzer alerts you when the state changes. Permission requests can be **approved or denied with the device buttons**, and long-pressing a button runs a **PC command** (bring app to front · shell command · key input). It connects over a USB cable or **BLE wireless**, and can be started from a menu bar **tray icon**.

> Current stage: **P4 — BLE · signing · tray · web flasher** complete. Plan and research: `PLAN.md`, hook verification results: [p0_spike/RESULTS.md](./p0_spike/RESULTS.md)

## Architecture

```
Claude Code ──HTTP hooks──▶ bridge_py/agent_bridge.py (PC daemon) ──USB serial or BLE (JSON Lines)──▶ DeskStream
                            127.0.0.1:9230                                                          4 OLEDs · buttons · buzzer
```

- Connection is **USB first, BLE otherwise** ("DeskStream Agent"). If USB is plugged in while using BLE, it switches to USB.
- Both the Claude Code CLI and **desktop app (Code tab) sessions** send hooks (when hooks are installed globally).

- State hooks get an **immediate empty response**. Claude Code work is unaffected even if the daemon or device is off.
- A permission request (PermissionRequest) holds the response for up to **290 seconds** waiting for a button decision, but only while the device is connected.
  The terminal dialog is shown at the same time, and **whichever answers first** decides.
  If the device is disconnected, times out or LATER is chosen, an empty response is returned → you decide in the terminal (**no auto-allow or auto-deny**).
- If you decide in the terminal first, the device's approval screen closes on the session's next event (tool completed, turn ended, etc.).
- A single font, **Galmuri7** (8px, OFL-1.1), displays both English and Korean (11,172 syllables). Characters not in the font (Chinese characters, emoji, etc.) are not drawn. Text limits are in bytes, so in Korean that is about 6–7 characters for names and tools, 32 characters (2 lines) for the overview summary, 80 characters for the approval summary, and 576 characters (4 pages) for the detail reply.

### BLE Security — Key Registration over USB + Signing

Anyone nearby can connect over BLE, so a **fake device** with the same name could send "approve" or "PC command" signals to the daemon.
(macOS's BLE library cannot tell whether a device is paired, so BLE pairing alone cannot prevent this.)

1. On the **first USB connection**, the daemon creates a 32-byte secret key, stores it in `~/.deskstream/device_key` (mode 600) and registers it with the device. The device accepts key registration **over USB only**.
2. On every BLE connection, the daemon sends a new random value (nonce).
3. Button and approval signals arriving over BLE carry an `HMAC-SHA256(key, nonce + body)` signature and an increasing counter, and the daemon accepts them only when both are valid. Fake devices, tampering and replays are rejected.

- Even when the daemon runs BLE-only (the default), if the device key differs it **briefly opens the USB port just to register the key and closes it right away**. If USB is not plugged in, it retries every 30 seconds, so you can plug in the cable later.
- The device stores **only one** key. If you switch PCs, the key of the last PC that registered remains — to use BLE from several PCs, make `~/.deskstream/device_key` the same file on each.
- If a device with no (or a different) key is used over BLE, the approval screen shows `PAIR VIA USB` and button signals (detail screen, approval, PC commands) are rejected → connect once over USB.
- The daemon → device direction (screen display) is not signed. A nearby device could connect and **show a fake screen**, but it cannot trigger anything on the PC.

## Screens

| State | Icon | Shows |
|:--|:--|:--|
| IDLE | Empty circle | Waiting |
| THINKING | 3-dot animation | Processing the prompt |
| RUNNING | Rotating spinner | Tool name + command/file summary |
| APPROVE? | `!` | Permission request — whole screen blinks inverted + alert sound |
| NEED INPUT | `?` | Waiting for a question/input — inverted blinking + alert sound |
| DONE | Check + inverted below the divider | Last reply summary + done sound |
| ERROR | X | Turn ended with an API error + error sound |

- Top right: time elapsed since the current state started. If a tool failed, `E<count>` appears at the right of the tool line.
- With 5 or more sessions, a new session takes the slot of the oldest session that is not awaiting approval/input.
- If there is no PC daemon signal (2-second heartbeat) for 6 seconds, "PC link lost" is shown.

### Approval Screen (while a permission request is pending — 4 screens as one)

```
┌──────────────┬──────────────┬──────────────┬──────────────┐
│APPROVE?      │web-app       │Bash          │1/2 0:12      │  ← inverted bar, blinking
│rm -rf node_mo│dules && npm i│nstall        │              │  ← command summary (one 128px line per screen × 2 lines)
│  [APPROVE]   │   [NEXT >]   │    [DENY]    │   [LATER]    │
└──────────────┴──────────────┴──────────────┴──────────────┘
     BTN1           BTN2           BTN3           BTN4
```

### Detail Screen (short press of a slot button on the overview)

| Screen 1 | Screen 2 | Screens 3 · 4 |
|:--|:--|:--|
| Name · state · elapsed time · permission mode (`mode plan` etc.) · sub-agent/error counts | Last 4 tools — `>` running · `+` succeeded · `!` failed | Last reply — up to 4 pages (about 576 Korean / 750 English characters), one page = 9 lines |

- Bottom buttons: **BTN1 = BACK** (to overview) · **BTN2 = CLOSE** (close session) · **BTN3 = < PREV** · **BTN4 = NEXT >** — **page through** the last reply (the labels appear only when there are pages to turn, and a page count like `1/4` shows at the top right of screen 3). To see another session, go BACK to the overview and press that slot's button.
- It returns to the overview after 60 seconds without a button press or when the session ends.
- PC command results appear for 4 seconds as a `RUN …` → `OK … exit 0` / `FAIL … exit 2` strip at the bottom of that button's screen.
- While muted (config file `[device] mute = true`), a speaker+X icon appears at the top right of OLED 4 on the overview, and button, alert and boot sounds are all off.
- **Empty slots**: empty OLEDs show **Claude Code usage** — the last two empty screens show, in order, 5-hour (`5H USAGE`) and weekly (`WEEK USAGE`) usage (number + bar) with the time until reset below (`reset 2h 05m`). For example: if all are empty, OLED 3 = 5-hour · 4 = weekly; if only OLED 2 and 4 are empty, 2 = 5-hour · 4 = weekly; if only one is empty, it shows 5-hour. Other empty screens show `SLOT n  empty`, and `--` until a value has been received. They reappear when a session ends and its slot empties.
  - Usage is a value Claude Code passes only to the **statusLine command**, so installing hooks also registers a command in `statusLine` of `~/.claude/settings.json` that forwards it to the daemon (it prints nothing, so the terminal status line stays empty). **If you already use a statusLine, it is not overwritten**, and usage is not shown in that case.
  - Usage arrives only when you are **logged in with a Claude subscription account (Pro/Max)**. If you run with API billing (`API Usage Billing` on the start screen) or another model server via `ANTHROPIC_API_KEY` · `ANTHROPIC_AUTH_TOKEN` · `ANTHROPIC_BASE_URL`, there is no value and it stays `--`. If your shell config sets these variables, run `env -u ANTHROPIC_API_KEY -u ANTHROPIC_AUTH_TOKEN -u ANTHROPIC_BASE_URL claude`.
  - The value changes only when the status line updates (when a Claude Code session exchanges replies). After restarting the daemon it shows `--` until the next update, and after the reset time passes it shows 0% until a new value arrives.
  - **If you use only the Claude desktop app (Code tab), the status line never runs and the value freezes.** In that case add `[usage] poll = true` to the config file and the daemon queries usage itself every 5 minutes (see [config file](#4-config-file--mute--brightness--usage-polling--pc-commands-optional) below).

## Buttons

| Screen | Action | Function |
|:--|:--|:--|
| Overview | BTN i short | Slot i detail screen (also acknowledges a done state → IDLE) |
| Overview · detail | BTN i long | Run PC command i from the config file |
| Detail | BTN1 / BTN2 | Back to overview / **close session** — removes the session from the device (hidden until a new question or approval request arrives; pending approvals go to the terminal) |
| Detail | BTN3 / BTN4 | Previous / next session |
| Approval | BTN1 | **Approve** — the tool runs immediately |
| Approval | BTN2 | Show the next pending request (only with 2 or more pending) |
| Approval | BTN3 | **Deny** — sends Claude a "denied on device" message |
| Approval | BTN4 | Later — don't decide on the device; leave it to the terminal dialog |

- Buttons are ignored for 0.7 seconds right after a new approval request appears (prevents approving by accident while pressing something else).
- There is no "always allow". Add allow rules yourself in the terminal.

## Usage

### 1. Firmware
- Required libraries: **DeskStreamCore** (repository `libraries/`, see "First-Time Setup" in the root README), **U8g2**
- Board: `esp32:esp32:makergo_c3_supermini`, USB CDC On Boot = Enabled, CPU 160MHz, Partition = Default

```bash
arduino-cli compile --fqbn "esp32:esp32:makergo_c3_supermini:CDCOnBoot=default,CPUFreq=160,PartitionScheme=default" Agent_Monitor
```

### 2. PC Daemon

**As an executable (no Python needed)** — Choose Agent Monitor in the [web flasher](https://worynim.github.io/DeskStream/) to download the macOS (Apple Silicon) or Windows (x64) app (GitHub Release `Agent_Monitor-client-v<version>`).
- Double-click to run in tray mode (BLE). The tray menu's **Install / Remove Claude Code hooks** does the same as `install-hooks` · `uninstall-hooks` in step 3 (the result is shown on a menu line).
- The app is unsigned, so on first launch macOS needs **System Settings → Privacy & Security → "Open Anyway"**, and Windows needs SmartScreen's **More info → Run anyway**.
- On macOS, grant the Bluetooth and Accessibility (`keys` command) permissions to the **AgentMonitor app**. Because it is unsigned, you may need to grant them again after replacing the app with a new version.
- To register the signing key over USB the first time, run in USB mode (quit with "Quit" in the tray menu):
  ```bash
  open /Applications/AgentMonitor.app --args tray --transport usb      # macOS
  AgentMonitor-0.6.1-windows-x64.exe tray --transport usb               # Windows (Command Prompt)
  ```
- Build it yourself: `cd bridge_py && pip install -r requirements.txt pyinstaller && pyinstaller agent_monitor.spec`

**From source (Python 3.11+, verified on macOS)**

```bash
cd Agent_Monitor/bridge_py
pip install -r requirements.txt          # pyserial · bleak (BLE) · pystray/Pillow (tray)
python3 agent_bridge.py tray             # run with a menu bar icon (recommended) — running with no command also starts tray
python3 agent_bridge.py run -v           # run in the terminal · print events
```

- Connection: `--transport ble` (**default**: BLE only) · `usb` (USB only) · `auto` (USB first, BLE otherwise); port: `--serial /dev/cu.usbmodemXXXX` (USB only). BLE signals are accepted only when a signing key is registered on the device, so **connect once with `--transport usb` (or `auto`)** to register the key.
- Tray icon (4 bars) colors: **gray** no device · **green** USB · **blue** BLE · **orange** awaiting approval · **red** failed to start (hook port in use — quit the daemon that is already running first). Menu: status · **Buzzer sound** (checked = on; clicking toggles `[device] mute` in the config file and applies it to the device right away) · open config file · install/remove Claude Code hooks · quit
- BLE needs the macOS **Bluetooth permission**. Allow it once in the app that runs the daemon (Terminal etc.).
- The firmware can also be installed from the [web flasher](https://worynim.github.io/DeskStream/).

> ⚠️ **If another app such as AgentDeck has the board's serial port open**, both uploading and the daemon connection fail. Quit that app first.

### 3. Connect Claude Code Hooks

Apply to a single session without touching global settings:
```bash
python3 agent_bridge.py settings > ~/agent_monitor_hooks.json
claude --settings ~/agent_monitor_hooks.json
```

Apply to all sessions (adds hooks and a usage `statusLine` to `~/.claude/settings.json`, keeps existing hooks/statusLine and creates a backup):
```bash
python3 agent_bridge.py install-hooks
python3 agent_bridge.py uninstall-hooks   # removes only the added entries
```

### 4. Config File — Mute · Brightness · Usage Polling · PC Commands (optional)

```bash
python3 agent_bridge.py init-config     # example → ~/.deskstream/agent_monitor.toml (mode 600)
```

```toml
[device]
mute = false            # true = turn off all alert and button sounds
brightness = 40         # OLED brightness % (1–100) — lower reduces burn-in (image retention)

[usage]
poll = false            # true = query Claude usage directly every 5 minutes

[[commands]]            # long button press — button = 1–4
button = 2
label = "npm test"
type = "shell"          # focus (bring app to front) | shell (run command) | keys (type text + key)
command = "npm test"
cwd = "~/work/web-app"
timeout = 300
```

- Mute and brightness in `[device]` are stored on the device and persist while the daemon is off (if a value is out of range, the device keeps the previous value — the error is recorded in the daemon log).
- `[usage] poll = true` (off by default): the daemon queries the usage endpoint Claude Code uses (`api.anthropic.com/api/oauth/usage`) with Claude Code's login token every 5 minutes.
  - Token location: the Keychain (`Claude Code-credentials`) on macOS, `~/.claude/.credentials.json` on Windows/Linux (or that folder if you use `CLAUDE_CONFIG_DIR`). The token exists only after you **log in to a subscription account once with the terminal `claude`** — logging in to the desktop app alone may not create it, in which case the log says `[usage] Claude Code 로그인 토큰 없음·만료` (login token missing/expired).
  - The token is **read only** — never refreshed, stored or logged. If it has expired, polling pauses and resumes once you use Claude Code again (which refreshes the token).
  - This is not an official API and may change with Claude Code updates, and it uses the subscription login token outside Claude Code, so it is off by default. If macOS asks for Keychain access on the first query, allow it.
  - Results are recorded in the daemon log as `[usage] 직접 조회 성공` (query succeeded) / `[usage] 조회 실패: …` (query failed) (the same message only once). Status line values are still received, and the most recent value is used.
- The device sends **only the button number**. What runs lives only in this file, which is re-read on every press (no daemon restart needed).
- If the file is writable by other users (group/other write), commands are not run.
- `keys` supports only ASCII text and `return · escape · tab · space · ctrl+c`, and needs the macOS **Accessibility** permission (for the terminal app that runs the daemon). If `app` is set, that app is brought to the front first.
  Without the permission the device shows `FAIL … no access` → allow the app that runs the daemon (Terminal, or Claude if it is the Claude app's terminal panel) in **System Settings > Privacy & Security > Accessibility**.
- Full example: [bridge_py/agent_monitor.example.toml](./bridge_py/agent_monitor.example.toml). For a config in another location, use `run --config <path>`.

### 5. Check the Screens (without Claude Code)
```bash
python3 demo_hooks.py        # puts 4 slots into running / awaiting approval / done / thinking
python3 demo_hooks.py --end
```

### Windows (not tested on real hardware)

The code is written to run on Windows too, but it has not been checked on actual Windows.

- The daemon, USB, BLE, tray and hook server are cross-platform code. The config file permission check is skipped on Windows (ACLs are used).
- The `SessionStart` hook is a `curl … || true` shell command, so **Git Bash** is required (same as Claude Code's own Windows requirement).
- Of the PC commands, `focus` and `keys` are **macOS only** (they fail with `macOS only` on Windows). `shell` works.

## Files

| File | Contents |
|:--|:--|
| `Agent_Monitor.ino` | USB/BLE line reception · buttons · alert sounds · 100ms render cycle |
| `agent_ble.h/.cpp` | BLE UART (NimBLE, Nordic UART UUID) |
| `agent_auth.h/.cpp` | Signing key (NVS) · nonce · HMAC-SHA256 signatures |
| `agent_state.h/.cpp` | Slot model and message parsing (pure C++, host tests) |
| `agent_ui.h/.cpp` | Slot · approval · detail · offline screens, PC command result strip, mute icon |
| `config.h` | Link timeout · frame period · blink period · approval misclick guard · detail screen return time, etc. |
| `bridge_py/agent_bridge.py` | Daemon (hook server · send loop · hook config commands) |
| `bridge_py/sessions.py` | Hook events → session state · slot assignment |
| `bridge_py/approvals.py` | Permission request queue (decide · next · cleanup when decided in the terminal) |
| `bridge_py/commands.py` | User config file · PC command execution |
| `bridge_py/usage_poll.py` | Direct Claude usage query (`[usage] poll`) |
| `bridge_py/agent_monitor.example.toml` | Config example |
| `bridge_py/protocol.py` | Serial message format (includes protocol description) |
| `bridge_py/transport.py` | USB serial · BLE (bleak) · USB-first auto selection |
| `bridge_py/security.py` | Signing key generation/registration · BLE signal signature and counter verification |
| `bridge_py/tray.py` | Menu bar tray icon |
| `bridge_py/hooks_config.py` | Hook config generation · install · removal |
| `bridge_py/demo_hooks.py` | Fake hooks for checking the screens |
| `bridge_py/agent_monitor.spec` | PC client executable build (PyInstaller) |
| `p0_spike/` | P0 hook behavior verification tools and results |

## Tests

```bash
cd Agent_Monitor/bridge_py && python3 -m unittest discover -s tests -v      # daemon, 51 tests

# Firmware state parsing (from the repository root)
L=libraries/DeskStreamCore/src
g++ -std=c++17 -IAgent_Monitor -I$L Agent_Monitor/test/test_agent_state.cpp Agent_Monitor/agent_state.cpp \
    $L/util/ds_json_lite.cpp -o /tmp/test_agent_state && /tmp/test_agent_state
```
