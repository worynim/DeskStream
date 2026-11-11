# worynim@gmail.com
"""
@file agent_bridge.py
@brief Agent_Monitor PC 데몬 — Claude Code 훅을 받아 세션 상태를 DeskStream 으로 보내고, 장치 버튼으로 권한을 결정한다
@details 구성: HTTP 훅 서버(127.0.0.1) → SessionTable / ApprovalQueue → SerialLink(USB 시리얼, JSON Lines)
         - 상태 훅: 즉시 빈 200 응답 → 데몬·장치가 꺼져 있어도 Claude Code 작업에 영향 없음.
         - PermissionRequest: 장치가 연결돼 있으면 최대 HOLD_SEC 동안 응답을 붙잡고 버튼 결정을 기다린다.
           장치 미연결·시간 초과·"나중에"·터미널 선결정이면 빈 응답 → 터미널 대화상자로 결정 (자동 허용 없음).
           터미널에서 승인하면 PostToolUse 로, 거절(Esc)하면 훅 연결이 끊기는 것으로 알고 장치 화면을 정리한다.
           터미널 대화상자는 기다리는 동안에도 함께 떠 있다 (P0 확인 — 먼저 답한 쪽이 이김).
         - 슬롯·승인·상세 화면이 바뀌면 바로, 그 밖에는 HEARTBEAT_SEC 마다 하트비트를 보낸다.
         - 상태줄(statusLine) 입력의 rate_limits(5시간·주간 사용률, 리셋 시각)를 받아 장치로 보낸다 (OLED 3·4 빈 슬롯 화면).
           데몬을 다시 켜면 다음 상태줄 갱신(Claude Code 세션 활동)이 올 때까지는 값을 모른다.
         - 장치 버튼: 짧게 = 그 슬롯 상세 보기(+완료 확인), 길게 = 설정 파일의 PC 명령 (commands.py),
           상세 화면 BTN2 = 세션 닫기(슬롯에서 지움). 설정 파일 [device] 의 음소거·밝기를 장치에 보낸다.
         - 연결: 기본은 BLE 만 (--transport ble). `--transport usb` = USB 만, `--transport auto` = USB 우선·없으면 BLE
           (transport.AutoLink). USB 로 연결되면 장치에 서명 키를 등록하고(처음 한 번),
           BLE 로 온 버튼·승인 신호는 서명·카운터가 맞을 때만 받는다 (security.py).

명령:
  python3 agent_bridge.py                              # 명령 없이 실행하면 tray (IDE 실행 버튼 등)
  python3 agent_bridge.py tray                         # 메뉴 막대 아이콘과 함께 데몬 실행
  python3 agent_bridge.py run [--transport ble|usb|auto] [--serial PORT]   # 데몬 실행 (기본 BLE)
  python3 agent_bridge.py settings > hooks.json        # claude --settings 용 훅 설정 출력 (전역 설정 변경 없음)
  python3 agent_bridge.py install-hooks                # ~/.claude/settings.json 에 훅·상태줄 설치 (백업 생성)
  python3 agent_bridge.py uninstall-hooks              # 설치한 항목만 제거
  python3 agent_bridge.py init-config                  # 예시 설정 → ~/.deskstream/agent_monitor.toml (권한 600)
"""
import argparse
import json
import select
import shutil
import socket
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import commands
import hooks_config
import protocol
from approvals import DECISIONS, ApprovalQueue
from security import DeviceAuth
from sessions import SessionTable, summarize_tool

DEFAULT_HTTP_PORT = 9230
HEARTBEAT_SEC = 2
HOLD_SEC = hooks_config.PERMISSION_TIMEOUT_SEC - 10   # 훅 timeout 전에 스스로 pass 로 끝낸다
CLEAR_SESSION_EVENTS = ("UserPromptSubmit", "Stop", "StopFailure", "SessionEnd")
CLEAR_TOOL_EVENTS = ("PostToolUse", "PostToolUseFailure")
SIGNED_TYPES = ("decide", "btn")   # BLE 로 올 때 서명이 필요한 장치 메시지
USAGE_WINDOWS = ("five_hour", "seven_day")   # 상태줄 rate_limits 의 구간 — 5시간 · 주간 (protocol.usage_msg 순서)
CLOSED_POLL_SEC = 0.5              # 권한 요청을 붙잡고 있는 동안 훅 연결이 끊겼는지 확인하는 간격
KEY_RETRY_SEC = 30                 # BLE 모드에서 USB 키 등록에 실패하면 이 간격으로 다시 시도 (케이블을 나중에 꽂아도 되게)


def log(msg: str):
    line = f"{time.strftime('%H:%M:%S')} {msg}"
    try:
        print(line, flush=True)
    except UnicodeEncodeError:   # Windows 콘솔(cp949 등)에 없는 글자 — 로그 때문에 장치 처리가 멈추지 않게
        enc = getattr(sys.stdout, "encoding", None) or "ascii"
        print(line.encode(enc, "replace").decode(enc), flush=True)


class Bridge:
    def __init__(self, link_factory, verbose=False, hold_sec=HOLD_SEC, config_path=commands.CONFIG_PATH,
                 executor=commands.execute, auth=None):
        self.table = SessionTable()
        self.approvals = ApprovalQueue()
        self.lock = threading.Lock()
        self.dirty = threading.Event()
        self.sent = [None] * len(self.table.slots)   # 슬롯별 마지막으로 보낸 내용 (같으면 생략)
        self.sent_perm = None                         # 마지막으로 보낸 승인 화면 줄
        self.sent_detail = None                       # 마지막으로 보낸 상세 화면 내용
        self.sent_cfg = None                          # 마지막으로 보낸 (cmds 줄, dev 줄)
        self.usage = [None] * len(USAGE_WINDOWS)      # 구간별 (사용률 %, 리셋 시각 epoch 초) — 받기 전에는 None
        self.sent_usage = None                        # 마지막으로 보낸 사용량 (구간별 값)
        self._cfg = (None, None)                      # (설정 파일 수정 시각, (cmds 줄, dev 줄)) — 파일이 바뀔 때만 다시 읽음
        self.watch = -1                               # 장치가 상세 화면으로 보고 있는 슬롯
        self.outbox = []                              # 바로 보낼 줄(알림·명령 진행) — 시리얼 쓰기는 송신 스레드만 한다
        self.verbose = verbose
        self.hold_sec = hold_sec
        self.config_path = config_path
        self.runner = commands.CommandRunner(self._on_cmd_status, log, config_path, executor)
        self.auth = auth or DeviceAuth()
        self.key_mismatch = False                     # BLE 로 연결된 장치의 키가 이 PC 키와 다름 (USB 로 등록 필요)
        self.key_busy = False                         # USB 키 등록 시도 중
        self.key_try_at = float("-inf")               # 마지막 USB 키 등록 시도 시각
        self.link = link_factory(self.on_device_line, self.on_connect)

    def start(self):
        self.link.start()
        threading.Thread(target=self._send_loop, daemon=True).start()

    # --- 훅 → 상태 ---
    def on_hook(self, event: str, payload: dict):
        sid = payload.get("session_id", "")
        with self.lock:
            if event in CLEAR_SESSION_EVENTS:
                self._log_cleared(self.approvals.clear_session(sid))
            elif event in CLEAR_TOOL_EVENTS:
                self._log_cleared(self.approvals.clear_tool(sid, payload.get("tool_name", ""),
                                                            payload.get("tool_input")))
            for kind, slot in self.table.apply(event, payload, time.time()):
                self.outbox.append(protocol.alert_msg(kind, slot))
        if self.verbose:
            log(f"[hook] {event:<18} {sid[:8]} {payload.get('tool_name', '')}")
        self.dirty.set()

    def on_permission(self, payload: dict, closed=lambda: False) -> bytes:
        """PermissionRequest — 장치 결정을 기다려 훅 응답 본문을 돌려준다 (호출한 HTTP 스레드를 붙잡는다).
        closed() 가 참이 되면(Claude Code 가 훅 연결을 끊음 = 터미널에서 거절) 기다리지 않고 정리한다."""
        self.on_hook("PermissionRequest", payload)
        if not self.link.connected:
            return protocol.permission_response("pass")
        sid = payload.get("session_id", "")
        with self.lock:
            s = self.table.sessions.get(sid)
            p = self.approvals.add(sid, s.slot if s else -1, s.name if s else sid[:8],
                                   payload.get("tool_name", ""), summarize_tool(payload.get("tool_input")),
                                   payload.get("tool_input"))
        self.dirty.set()
        deadline = time.time() + self.hold_sec
        while not p.done.wait(CLOSED_POLL_SEC):
            gone = closed()
            if gone or time.time() >= deadline:
                with self.lock:
                    self.approvals.resolve(p.id, "pass")   # 시간 초과 → 터미널에서 결정 / 터미널에서 이미 거절
                if gone:
                    log(f"[perm] #{p.id} 훅 연결이 끊김 (터미널에서 거절) → 장치 화면 정리")
                self.dirty.set()
        log(f"[perm] #{p.id} {p.tool} {p.summary[:40]!r} → {p.decision}")
        return protocol.permission_response(p.decision)

    def on_status(self, payload: dict):
        """상태줄 입력 — rate_limits 가 있으면 사용량을 갱신한다 (구독 계정이 아니거나 첫 응답 전에는 없음)."""
        limits = payload.get("rate_limits")
        if not isinstance(limits, dict):
            return
        with self.lock:
            for k, name in enumerate(USAGE_WINDOWS):
                w = limits.get(name)
                pct = w.get("used_percentage") if isinstance(w, dict) else None
                if not isinstance(pct, (int, float)):
                    continue                          # 이 구간 값이 없으면 마지막 값을 유지
                reset_at = w.get("resets_at")
                self.usage[k] = (max(0, min(100, round(pct))),
                                 int(reset_at) if isinstance(reset_at, (int, float)) else 0)
        self.dirty.set()

    def usage_windows(self, now: float) -> tuple:
        """보낼 사용량 (구간별 값). 리셋 시각이 지난 구간은 0 % 로 본다 (새 값이 올 때까지)."""
        return tuple((0, 0) if w and w[1] and now >= w[1] else w for w in self.usage)

    def _on_cmd_status(self, button: int, state: str, label: str, result: str):
        with self.lock:
            self.outbox.append(protocol.cmd_msg(button - 1, state, label, result))
        self.dirty.set()

    def _log_cleared(self, cleared):
        for p in cleared:
            log(f"[perm] #{p.id} 터미널 등에서 먼저 결정됨 → 장치 화면 정리")

    # --- 장치 → 데몬 ---
    def on_connect(self):
        with self.lock:
            if self.link.kind == "ble":       # 연결마다 새 nonce — 서명 확인의 기준 (장치가 hello 로 답한다)
                self.outbox.insert(0, self.auth.new_nonce())
            else:                             # 이미 켜져 있던 장치도 hello(키 id)를 보내게
                self.outbox.insert(0, protocol.WHO)
        self.sent = [None] * len(self.sent)   # 새 연결이면 전체 재전송
        self.sent_perm = None
        self.sent_detail = None
        self.sent_cfg = None
        self.sent_usage = None
        self.dirty.set()

    def _register_key_via_usb(self, now: float):
        """BLE 로 연결된 장치의 키가 다름 → USB 가 꽂혀 있으면 포트를 잠깐 열어 키를 등록한다.
        실패하면 KEY_RETRY_SEC 뒤 다시 (나중에 케이블을 꽂아도 등록되게). 성공하면 장치가 BLE 로도 새 kid 의 hello 를
        보내 key_mismatch 가 풀린다. 송신 루프를 막지 않게 별도 스레드에서."""
        register = getattr(self.link, "register_key", None)
        with self.lock:
            if register is None or self.key_busy or now - self.key_try_at < KEY_RETRY_SEC:
                return
            self.key_busy, self.key_try_at = True, now

        def work():
            try:
                if register(self.auth.setkey_msg(), self.auth.kid()):
                    log("[device] 서명 키 등록 (USB 잠깐 연결) — 이제 BLE 버튼·승인을 받습니다")
            finally:
                with self.lock:
                    self.key_busy = False
        threading.Thread(target=work, daemon=True).start()

    def config_lines(self) -> tuple:
        """설정 파일에서 만든 (cmds 줄, dev 줄) — 파일 수정 시각이 바뀌었을 때만 다시 읽는다.
        설정 오류면 cmds 는 "config error", dev 는 None(장치가 마지막 값을 유지)."""
        try:
            mtime = self.config_path.stat().st_mtime
        except OSError:
            mtime = None
        if self._cfg[1] is None or mtime != self._cfg[0]:
            try:
                cfg = commands.load_config(self.config_path)
                lines = (protocol.cmds_msg(commands.labels_of(cfg)), protocol.dev_msg(cfg.mute, cfg.brightness))
            except commands.ConfigError as e:
                log(f"[config] {e}")
                lines = (protocol.cmds_msg(commands.CONFIG_ERROR_LABELS), None)
            self._cfg = (mtime, lines)
        return self._cfg[1]

    def on_device_line(self, line: str):
        msg = protocol.parse_device(line)
        if msg is None:
            return
        kind = msg.get("t")
        if kind in SIGNED_TYPES and self.link.kind == "ble" and not self.auth.verify(line):
            log(f"[device] BLE 신호 거부 (서명 없음·불일치·재전송): {line.strip()[:60]}")
            return
        if kind == "hello":
            log(f"[device] hello fw={msg.get('fw')} kid={msg.get('kid') or '-'} via {self.link.kind}")
            mismatch = msg.get("kid") != self.auth.kid()
            with self.lock:
                self.key_mismatch = mismatch and self.link.kind == "ble"
            if mismatch:
                if self.link.kind == "usb":
                    log("[device] 서명 키 등록 (USB)")
                    with self.lock:
                        self.outbox.append(self.auth.setkey_msg())
                else:
                    log("[device] 장치 키가 없거나 다름 — BLE 버튼·승인은 거부됨. USB 가 꽂혀 있으면 자동으로 등록합니다")
                    self._register_key_via_usb(time.time())
            self.sent = [None] * len(self.sent)
            self.sent_perm = None
            self.sent_detail = None
            self.sent_cfg = None
            self.sent_usage = None
            self.dirty.set()
        elif kind == "decide":
            self._on_decide(msg.get("id"), msg.get("act"))
        elif kind == "btn":
            b, press = msg.get("b"), msg.get("press")
            log(f"[device] 버튼 {b} {press}")
            if not isinstance(b, int) or not 0 <= b < len(self.table.slots):
                return
            if press == "short":            # 슬롯 b 상세 보기 + 완료 확인
                with self.lock:
                    self.table.mark_read(b, time.time())
                    self.watch = b
                    self.sent_detail = None
                self.dirty.set()
            elif press == "close":          # 상세 화면 BTN2 — 슬롯 b 세션을 장치에서 지운다 (다시 이벤트가 오면 다시 생김)
                with self.lock:
                    sid = self.table.slots[b]
                    if sid:
                        self._log_cleared(self.approvals.clear_session(sid))
                        self.table.remove(sid)
                    self.watch = -1
                self.dirty.set()
            elif press == "long":           # 설정 파일의 PC 명령 (버튼 번호 1~4)
                self.runner.run(b + 1)

    def _on_decide(self, pid, act):
        if not isinstance(pid, int):
            return
        with self.lock:
            if act == "next":
                self.approvals.next(pid)
            elif act in DECISIONS:
                p = self.approvals.resolve(pid, act)
                if p is not None and act != "pass":
                    # 장치 결정 직후 훅이 오기 전까지 슬롯이 승인 대기로 남지 않게
                    self.table.set_state(p.sid, "run" if act == "allow" else "think", time.time())
        self.dirty.set()

    # --- 송신 루프 ---
    def _send_loop(self):
        last_hb = 0.0
        while True:
            self.dirty.wait(timeout=HEARTBEAT_SEC)
            self.dirty.clear()
            now = time.time()
            with self.lock:
                self.table.prune(now)
                views = self.table.view(now)
                outbox, self.outbox = self.outbox, []
                perm_line = protocol.perm_msg(self.approvals.current(), *self.approvals.position())
                detail = self.table.detail(self.watch, now) if self.watch >= 0 else None
                usage = self.usage_windows(now)
            for line in outbox:
                self.link.send(line)
            for i, v in enumerate(views):
                # el(경과 초)은 매번 달라지므로 비교에서 빼고, 장치가 직접 센다
                key = None if v is None else {**v, "el": None}
                if key != self.sent[i]:
                    self.sent[i] = key
                    self.link.send(protocol.slot_msg(i, v))
            if perm_line != self.sent_perm:
                self.sent_perm = perm_line
                self.link.send(perm_line)
            cfg_lines = self.config_lines()
            if cfg_lines != self.sent_cfg:
                self.sent_cfg = cfg_lines
                for line in cfg_lines:
                    if line:
                        self.link.send(line)
            if any(usage) and usage != self.sent_usage:
                self.sent_usage = usage
                self.link.send(protocol.usage_msg(usage, now))
            if self.watch >= 0:
                key = None if detail is None else {**detail, "el": None}
                if key != self.sent_detail:
                    self.sent_detail = key
                    self.link.send(protocol.detail_msg(self.watch, detail))
            if now - last_hb >= HEARTBEAT_SEC:
                last_hb = now
                self.link.send(protocol.HEARTBEAT)
            if self.key_mismatch and self.link.kind == "ble":
                self._register_key_via_usb(now)


def make_handler(bridge: Bridge):
    class HookHandler(BaseHTTPRequestHandler):
        def do_POST(self):
            event = self.path.rsplit("/", 1)[-1]
            body = self.rfile.read(int(self.headers.get("Content-Length", 0) or 0))
            try:
                payload = json.loads(body or b"{}")
            except json.JSONDecodeError:
                payload = None
            if not isinstance(payload, dict):
                return self._reply(b"")
            if event == "PermissionRequest":
                return self._reply(bridge.on_permission(payload, self._client_closed))
            if event == "StatusLine":        # 훅이 아니라 상태줄 입력 — 사용량만 읽는다
                self._reply(b"")
                return bridge.on_status(payload)
            self._reply(b"")                 # 상태 훅은 먼저 응답해 Claude Code 를 기다리게 하지 않는다
            bridge.on_hook(event, payload)

        def _client_closed(self) -> bool:
            """요청 본문을 다 받은 뒤 소켓이 읽기 가능 = 상대가 연결을 닫음 (추가로 보낼 데이터는 없다)"""
            try:
                readable, _, _ = select.select([self.connection], [], [], 0)
                return bool(readable) and self.connection.recv(1, socket.MSG_PEEK) == b""
            except OSError:
                return True

        def _reply(self, out: bytes):
            try:
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(out)))
                self.end_headers()
                self.wfile.write(out)
            except (BrokenPipeError, ConnectionResetError):
                pass                          # Claude Code 가 먼저 연결을 끊음 (timeout 등)

        def log_message(self, *_):
            pass
    return HookHandler


def start_daemon(args) -> tuple:
    """Bridge 와 훅 서버를 만들어 시작한다 (서버 루프는 호출한 쪽이 돌린다). (bridge, server)"""
    from transport import AutoLink   # pyserial·bleak 은 실행할 때만 필요
    bridge = Bridge(lambda on_line, on_connect: AutoLink(on_line, on_connect, serial_port=args.serial, log=log,
                                                         mode=args.transport),
                    verbose=args.verbose, config_path=args.config)
    # 포트를 먼저 연다 — 이미 다른 데몬이 쓰고 있으면(OSError) 장치 연결을 시작하지 않고 실패
    server = ThreadingHTTPServer(("127.0.0.1", args.http_port), make_handler(bridge))
    server.daemon_threads = True          # 붙잡힌 권한 요청이 종료를 막지 않게
    bridge.start()
    via = args.serial or {"auto": "USB 우선 · 없으면 BLE", "usb": "USB 만", "ble": "BLE 만"}[args.transport]
    log(f"agent_bridge: 훅 수신 127.0.0.1:{args.http_port} · 연결 {via}")
    return bridge, server


def cmd_run(args):
    _bridge, server = start_daemon(args)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


def main():
    ap = argparse.ArgumentParser(description="Agent_Monitor PC 데몬")
    ap.add_argument("--http-port", type=int, default=DEFAULT_HTTP_PORT)
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name, help_text in (("run", "데몬 실행"), ("tray", "메뉴 막대 아이콘과 함께 데몬 실행")):
        run = sub.add_parser(name, help=help_text)
        run.add_argument("--serial", help="시리얼 포트 지정 (지정하면 USB 만)")
        run.add_argument("--transport", choices=("auto", "usb", "ble"), default="ble",
                         help="ble = BLE 만 (기본) · usb = USB 만 · auto = USB 가 보이면 USB, 아니면 BLE")
        run.add_argument("-v", "--verbose", action="store_true", help="훅 이벤트 출력")
        run.add_argument("--config", type=Path, default=commands.CONFIG_PATH, help="설정 파일 (기본: %(default)s)")
    sub.add_parser("settings", help="claude --settings 용 훅 설정 JSON 출력")
    for name in ("install-hooks", "uninstall-hooks"):
        p = sub.add_parser(name)
        p.add_argument("--file", type=Path, default=hooks_config.DEFAULT_SETTINGS)
    sub.add_parser("init-config", help=f"예시 설정을 {commands.CONFIG_PATH} 로 복사 (이미 있으면 그대로 둠)")
    args = ap.parse_args(sys.argv[1:] or ["tray"])   # 명령 없이 실행하면 tray

    if args.cmd == "run":
        cmd_run(args)
    elif args.cmd == "tray":
        try:
            import tray  # pystray·Pillow 는 트레이에서만 필요
        except ImportError as e:
            log(f"트레이를 열 수 없어 아이콘 없이 실행합니다 ({e.name} 미설치 — pip install -r requirements.txt)")
            return cmd_run(args)
        tray.main(args, start_daemon)
    elif args.cmd == "settings":
        print(json.dumps({"hooks": hooks_config.build_hooks(args.http_port),
                          "statusLine": hooks_config.build_statusline(args.http_port)}, indent=2))
    elif args.cmd == "install-hooks":
        backup = hooks_config.install(args.file, args.http_port)
        print(f"설치 완료: {args.file}" + (f" (백업: {backup})" if backup else ""))
        if hooks_config.statusline_kept(args.file, args.http_port):
            print("기존 statusLine 을 그대로 두었습니다 — 사용량(5시간·주간)은 표시되지 않습니다")
    elif args.cmd == "init-config":
        dst = commands.CONFIG_PATH
        if dst.exists():
            print(f"이미 있습니다: {dst}")
        else:
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(commands.EXAMPLE_PATH, dst)
            dst.chmod(0o600)
            print(f"생성: {dst} — 버튼 명령을 편집하세요")
    elif args.cmd == "uninstall-hooks":
        removed, backup = hooks_config.uninstall(args.file, args.http_port)
        print(f"제거한 훅 {removed}개: {args.file}" + (f" (백업: {backup})" if backup else ""))


if __name__ == "__main__":
    sys.exit(main())
