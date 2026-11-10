# worynim@gmail.com
"""
@file commands.py
@brief 사용자 설정 파일(~/.deskstream/agent_monitor.toml) — 버튼 길게 누르기 PC 명령 · 장치 음소거·밝기
@details 보안 원칙
         - 장치는 버튼 번호만 보낸다. 실행할 내용은 오직 이 설정 파일에서 온다 (장치·네트워크에서 명령 문자열을 받지 않음).
         - 설정 파일이 다른 사용자도 쓸 수 있는 권한(group/other write)이면 명령을 실행하지 않는다 (macOS·Linux).
         - 버튼을 누를 때마다 파일을 다시 읽는다 → 데몬 재시작 없이 수정 반영.
         명령 종류
         - focus : 앱을 앞으로 (`open -a`, macOS)
         - shell : 셸 명령 실행 → 종료 코드를 장치에 표시 (timeout 지원)
         - keys  : 앞에 있는 창(또는 app 지정 시 그 앱)에 텍스트·키 입력 (osascript, macOS 손쉬운 사용 권한 필요)
"""
import os
import stat
import subprocess
import sys
import threading
import tomllib
from dataclasses import dataclass
from pathlib import Path

CONFIG_PATH = Path.home() / ".deskstream" / "agent_monitor.toml"
EXAMPLE_PATH = Path(__file__).with_name("agent_monitor.example.toml")
TYPES = ("focus", "shell", "keys")
KEYS = {   # key 이름 → AppleScript System Events 구문
    "return": "key code 36", "escape": "key code 53", "tab": "key code 48",
    "space": "key code 49", "ctrl+c": 'keystroke "c" using control down',
}
DEFAULT_SHELL_TIMEOUT = 300


class ConfigError(Exception):
    pass


@dataclass
class Command:
    button: int          # 1~4 (장치 BTN 번호)
    label: str
    type: str
    app: str = ""
    command: str = ""
    cwd: str = ""
    timeout: int = DEFAULT_SHELL_TIMEOUT
    text: str = ""
    key: str = ""


@dataclass
class Config:
    commands: dict           # button(1~4) → Command
    mute: bool = False       # [device] mute — 장치 알림음·버튼음 모두 끔
    brightness: int = 100    # [device] brightness — OLED 밝기 % (1~100, 낮출수록 번인 감소)


def parse_config(data: dict) -> Config:
    commands = {}
    for k, c in enumerate(data.get("commands", [])):
        where = f"commands[{k}]"
        b = c.get("button")
        if b not in (1, 2, 3, 4):
            raise ConfigError(f"{where}: button 은 1~4")
        if b in commands:
            raise ConfigError(f"{where}: button {b} 이 중복됩니다")
        kind = c.get("type")
        if kind not in TYPES:
            raise ConfigError(f"{where}: type 은 {'/'.join(TYPES)}")
        cmd = Command(button=b, label=str(c.get("label", ""))[:20] or kind, type=kind,
                      app=str(c.get("app", "")), command=str(c.get("command", "")), cwd=str(c.get("cwd", "")),
                      timeout=int(c.get("timeout", DEFAULT_SHELL_TIMEOUT)), text=str(c.get("text", "")),
                      key=str(c.get("key", "")))
        if kind == "focus" and not cmd.app:
            raise ConfigError(f"{where}: focus 는 app 이 필요합니다")
        if kind == "shell" and not cmd.command:
            raise ConfigError(f"{where}: shell 은 command 가 필요합니다")
        if kind == "keys":
            if not cmd.text and not cmd.key:
                raise ConfigError(f"{where}: keys 는 text 또는 key 가 필요합니다")
            if cmd.key and cmd.key not in KEYS:
                raise ConfigError(f"{where}: key 는 {'/'.join(KEYS)}")
            if not all(32 <= ord(ch) < 127 for ch in cmd.text):
                raise ConfigError(f"{where}: text 는 영문·숫자·기호(ASCII)만 됩니다")
        commands[b] = cmd
    dev = data.get("device", {})
    if not isinstance(dev, dict):
        raise ConfigError("[device] 는 표(table)여야 합니다")
    mute = dev.get("mute", False)
    if not isinstance(mute, bool):
        raise ConfigError("device.mute 는 true / false")
    brightness = dev.get("brightness", 100)
    if isinstance(brightness, bool) or not isinstance(brightness, int) or not 1 <= brightness <= 100:
        raise ConfigError("device.brightness 는 1~100 (%)")
    return Config(commands, mute, brightness)


def load_config(path: Path = CONFIG_PATH) -> Config:
    """설정 파일을 읽는다. 파일이 없으면 빈 설정. 문법·값 오류는 ConfigError."""
    if not path.exists():
        return Config({})
    # Windows 는 권한을 ACL 로 관리해 st_mode 가 늘 0o666 으로 보이므로 검사하지 않는다 (사용자 프로필 폴더 보호에 맡김)
    if os.name != "nt" and path.stat().st_mode & (stat.S_IWGRP | stat.S_IWOTH):
        raise ConfigError(f"{path} 를 다른 사용자도 쓸 수 있습니다 — chmod 600 후 다시 시도하세요")
    try:
        data = tomllib.loads(path.read_text(encoding="utf-8"))
    except tomllib.TOMLDecodeError as e:
        raise ConfigError(f"{path.name} 문법 오류: {e}")
    return parse_config(data)


CONFIG_ERROR_LABELS = [("config error", "error")] * 4


def labels_of(cfg: Config) -> list:
    """BTN1~4 의 (이름, 종류) — 장치 빈 슬롯 화면에 "길게 누르면 하는 일"로 표시. 설정 없음 = ("", "")."""
    return [(cfg.commands[b].label, cfg.commands[b].type) if b in cfg.commands else ("", "") for b in (1, 2, 3, 4)]


def button_labels(path: Path = CONFIG_PATH) -> list:
    """labels_of(설정 파일). 설정 오류면 네 버튼 모두 ("config error", "error")."""
    try:
        return labels_of(load_config(path))
    except ConfigError:
        return CONFIG_ERROR_LABELS


def _applescript_str(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def build_argv(cmd: Command) -> list:
    """실행할 프로세스 인자 (shell 제외). 테스트에서 실제 실행 없이 확인할 수 있게 분리."""
    if cmd.type == "focus":
        return ["open", "-a", cmd.app]
    lines = []
    if cmd.app:
        lines += [f"tell application {_applescript_str(cmd.app)} to activate", "delay 0.3"]
    events = []
    if cmd.text:
        events.append(f"keystroke {_applescript_str(cmd.text)}")
    if cmd.key:
        events.append(KEYS[cmd.key])
    if len(events) == 1:
        lines.append('tell application "System Events" to ' + events[0])
    else:
        lines += ['tell application "System Events"', *events, "end tell"]
    return ["osascript", "-e", "\n".join(lines)]


def _no_accessibility(stderr: bytes) -> bool:
    """osascript 가 손쉬운 사용 권한이 없어 키 입력을 거부당했는지 (오류 1002 / 'not allowed')"""
    text = (stderr or b"").decode("utf-8", errors="replace")
    return "1002" in text or "not allowed" in text.lower()


def execute(cmd: Command) -> tuple:
    """명령을 실행하고 (성공 여부, 짧은 결과 문구) 를 돌려준다."""
    if cmd.type in ("focus", "keys") and sys.platform != "darwin":
        return False, "macOS only"
    try:
        if cmd.type == "shell":
            # Windows 콘솔 없는 실행 파일(.exe)에서 명령마다 콘솔 창이 뜨지 않게 한다
            r = subprocess.run(cmd.command, shell=True, cwd=os.path.expanduser(cmd.cwd) if cmd.cwd else None,
                               capture_output=True, timeout=cmd.timeout,
                               creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        else:
            r = subprocess.run(build_argv(cmd), capture_output=True, timeout=10)
    except subprocess.TimeoutExpired:
        return False, "timeout"
    except OSError as e:
        return False, type(e).__name__
    if cmd.type == "keys" and r.returncode != 0 and _no_accessibility(r.stderr):
        return False, "no access"   # macOS 손쉬운 사용 권한 없음 (데몬을 실행한 앱에 허용 필요)
    return r.returncode == 0, f"exit {r.returncode}"


class CommandRunner:
    """버튼별로 한 번에 하나만 실행. notify(button, state, label, msg) 로 진행 상황을 알린다."""

    def __init__(self, notify, log, config_path: Path = CONFIG_PATH, executor=execute):
        self.notify, self.log, self.path, self.executor = notify, log, config_path, executor
        self.busy = set()
        self.lock = threading.Lock()

    def run(self, button: int):
        try:
            cmd = load_config(self.path).commands.get(button)
        except ConfigError as e:
            self.log(f"[cmd] 설정 오류: {e}")
            return self.notify(button, "fail", "config", "error")
        if cmd is None:
            return self.notify(button, "fail", f"BTN{button}", "not set")
        with self.lock:
            if button in self.busy:
                return          # 같은 버튼 명령이 아직 실행 중
            self.busy.add(button)
        self.notify(button, "run", cmd.label, "")
        threading.Thread(target=self._run, args=(cmd,), daemon=True).start()

    def _run(self, cmd: Command):
        ok, msg = self.executor(cmd)
        self.log(f"[cmd] BTN{cmd.button} {cmd.type} {cmd.label!r} → {msg}")
        with self.lock:
            self.busy.discard(cmd.button)
        self.notify(cmd.button, "ok" if ok else "fail", cmd.label, msg)
