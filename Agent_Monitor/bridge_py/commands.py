# worynim@gmail.com
"""
@file commands.py
@brief 사용자 설정 파일(~/.deskstream/agent_monitor.toml) — 버튼 길게 누르기 PC 명령과 무음 시간대
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
from datetime import datetime, time as dtime
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
    commands: dict       # button(1~4) → Command
    quiet: tuple | None  # (시작, 끝) datetime.time — 자정을 넘을 수 있다


def _parse_hhmm(value, where) -> dtime:
    try:
        h, m = str(value).split(":")
        return dtime(int(h), int(m))
    except (ValueError, TypeError):
        raise ConfigError(f"{where}: 시간은 \"HH:MM\" 형식이어야 합니다 ({value!r})")


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
    quiet = None
    if "quiet" in data:
        q = data["quiet"]
        quiet = (_parse_hhmm(q.get("start"), "quiet.start"), _parse_hhmm(q.get("end"), "quiet.end"))
    return Config(commands, quiet)


def load_config(path: Path = CONFIG_PATH) -> Config:
    """설정 파일을 읽는다. 파일이 없으면 빈 설정. 문법·값 오류는 ConfigError."""
    if not path.exists():
        return Config({}, None)
    # Windows 는 권한을 ACL 로 관리해 st_mode 가 늘 0o666 으로 보이므로 검사하지 않는다 (사용자 프로필 폴더 보호에 맡김)
    if os.name != "nt" and path.stat().st_mode & (stat.S_IWGRP | stat.S_IWOTH):
        raise ConfigError(f"{path} 를 다른 사용자도 쓸 수 있습니다 — chmod 600 후 다시 시도하세요")
    try:
        data = tomllib.loads(path.read_text(encoding="utf-8"))
    except tomllib.TOMLDecodeError as e:
        raise ConfigError(f"{path.name} 문법 오류: {e}")
    return parse_config(data)


def in_quiet(now: datetime, quiet) -> bool:
    if not quiet:
        return False
    start, end, t = quiet[0], quiet[1], now.time()
    return start <= t < end if start <= end else (t >= start or t < end)


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


def execute(cmd: Command) -> tuple:
    """명령을 실행하고 (성공 여부, 짧은 결과 문구) 를 돌려준다."""
    if cmd.type in ("focus", "keys") and sys.platform != "darwin":
        return False, "macOS only"
    try:
        if cmd.type == "shell":
            r = subprocess.run(cmd.command, shell=True, cwd=os.path.expanduser(cmd.cwd) if cmd.cwd else None,
                               capture_output=True, timeout=cmd.timeout)
        else:
            r = subprocess.run(build_argv(cmd), capture_output=True, timeout=10)
    except subprocess.TimeoutExpired:
        return False, "timeout"
    except OSError as e:
        return False, type(e).__name__
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
