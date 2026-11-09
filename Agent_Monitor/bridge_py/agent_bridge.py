# worynim@gmail.com
"""
@file agent_bridge.py
@brief Agent_Monitor PC 데몬 — Claude Code 훅을 받아 세션 상태를 DeskStream 으로 보내고, 장치 버튼으로 권한을 결정한다
@details 구성: HTTP 훅 서버(127.0.0.1) → SessionTable / ApprovalQueue → SerialLink(USB 시리얼, JSON Lines)
         - 상태 훅: 즉시 빈 200 응답 → 데몬·장치가 꺼져 있어도 Claude Code 작업에 영향 없음.
         - PermissionRequest: 장치가 연결돼 있으면 최대 HOLD_SEC 동안 응답을 붙잡고 버튼 결정을 기다린다.
           장치 미연결·시간 초과·"나중에"·터미널 선결정이면 빈 응답 → 터미널 대화상자로 결정 (자동 허용 없음).
           터미널 대화상자는 기다리는 동안에도 함께 떠 있다 (P0 확인 — 먼저 답한 쪽이 이김).
         - 슬롯·승인 화면이 바뀌면 바로, 그 밖에는 HEARTBEAT_SEC 마다 하트비트를 보낸다.

명령:
  python3 agent_bridge.py run [--serial PORT]          # 데몬 실행
  python3 agent_bridge.py settings > hooks.json        # claude --settings 용 훅 설정 출력 (전역 설정 변경 없음)
  python3 agent_bridge.py install-hooks                # ~/.claude/settings.json 에 설치 (백업 생성)
  python3 agent_bridge.py uninstall-hooks              # 설치한 항목만 제거
"""
import argparse
import json
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import hooks_config
import protocol
from approvals import DECISIONS, ApprovalQueue
from sessions import SessionTable, summarize_tool

DEFAULT_HTTP_PORT = 9230
HEARTBEAT_SEC = 2
HOLD_SEC = hooks_config.PERMISSION_TIMEOUT_SEC - 10   # 훅 timeout 전에 스스로 pass 로 끝낸다
CLEAR_SESSION_EVENTS = ("UserPromptSubmit", "Stop", "StopFailure", "SessionEnd")
CLEAR_TOOL_EVENTS = ("PostToolUse", "PostToolUseFailure")


def log(msg: str):
    print(f"{time.strftime('%H:%M:%S')} {msg}", flush=True)


class Bridge:
    def __init__(self, link_factory, verbose=False, hold_sec=HOLD_SEC):
        self.table = SessionTable()
        self.approvals = ApprovalQueue()
        self.lock = threading.Lock()
        self.dirty = threading.Event()
        self.sent = [None] * len(self.table.slots)   # 슬롯별 마지막으로 보낸 내용 (같으면 생략)
        self.sent_perm = None                         # 마지막으로 보낸 승인 화면 줄
        self.alerts = []                              # 보낼 알림 줄 — 시리얼 쓰기는 송신 스레드만 한다
        self.verbose = verbose
        self.hold_sec = hold_sec
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
                self.alerts.append(protocol.alert_msg(kind, slot))
        if self.verbose:
            log(f"[hook] {event:<18} {sid[:8]} {payload.get('tool_name', '')}")
        self.dirty.set()

    def on_permission(self, payload: dict) -> bytes:
        """PermissionRequest — 장치 결정을 기다려 훅 응답 본문을 돌려준다 (호출한 HTTP 스레드를 붙잡는다)."""
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
        if not p.done.wait(self.hold_sec):
            with self.lock:
                self.approvals.resolve(p.id, "pass")   # 시간 초과 → 터미널에서 결정
            self.dirty.set()
        log(f"[perm] #{p.id} {p.tool} {p.summary[:40]!r} → {p.decision}")
        return protocol.permission_response(p.decision)

    def _log_cleared(self, cleared):
        for p in cleared:
            log(f"[perm] #{p.id} 터미널 등에서 먼저 결정됨 → 장치 화면 정리")

    # --- 장치 → 데몬 ---
    def on_connect(self):
        self.sent = [None] * len(self.sent)   # 새 연결이면 전체 재전송
        self.sent_perm = None
        self.dirty.set()

    def on_device_line(self, line: str):
        msg = protocol.parse_device(line)
        if msg is None:
            return
        kind = msg.get("t")
        if kind == "hello":
            log(f"[device] hello fw={msg.get('fw')}")
            self.on_connect()
        elif kind == "decide":
            self._on_decide(msg.get("id"), msg.get("act"))
        elif kind == "btn":
            b, press = msg.get("b"), msg.get("press")
            log(f"[device] 버튼 {b} {press}")
            if press == "short" and isinstance(b, int):
                with self.lock:
                    if self.table.mark_read(b, time.time()):
                        self.dirty.set()
            # press == "long": PC 명령 실행은 P3 에서 결정 (현재는 기록만)

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
                alerts, self.alerts = self.alerts, []
                perm_line = protocol.perm_msg(self.approvals.current(), *self.approvals.position())
            for line in alerts:
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
            if now - last_hb >= HEARTBEAT_SEC:
                last_hb = now
                self.link.send(protocol.HEARTBEAT)


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
                return self._reply(bridge.on_permission(payload))
            self._reply(b"")                 # 상태 훅은 먼저 응답해 Claude Code 를 기다리게 하지 않는다
            bridge.on_hook(event, payload)

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


def cmd_run(args):
    from transport import SerialLink   # pyserial 은 run 에서만 필요
    bridge = Bridge(lambda on_line, on_connect: SerialLink(on_line, on_connect, port=args.serial, log=log),
                    verbose=args.verbose)
    bridge.start()
    server = ThreadingHTTPServer(("127.0.0.1", args.http_port), make_handler(bridge))
    server.daemon_threads = True          # 붙잡힌 권한 요청이 종료를 막지 않게
    log(f"agent_bridge: 훅 수신 127.0.0.1:{args.http_port} · 시리얼 {args.serial or '자동 탐색'}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


def main():
    ap = argparse.ArgumentParser(description="Agent_Monitor PC 데몬")
    ap.add_argument("--http-port", type=int, default=DEFAULT_HTTP_PORT)
    sub = ap.add_subparsers(dest="cmd", required=True)
    run = sub.add_parser("run", help="데몬 실행")
    run.add_argument("--serial", help="시리얼 포트 (기본: Espressif USB 자동 탐색)")
    run.add_argument("-v", "--verbose", action="store_true", help="훅 이벤트 출력")
    sub.add_parser("settings", help="claude --settings 용 훅 설정 JSON 출력")
    for name in ("install-hooks", "uninstall-hooks"):
        p = sub.add_parser(name)
        p.add_argument("--file", type=Path, default=hooks_config.DEFAULT_SETTINGS)
    args = ap.parse_args()

    if args.cmd == "run":
        cmd_run(args)
    elif args.cmd == "settings":
        print(json.dumps({"hooks": hooks_config.build_hooks(args.http_port)}, indent=2))
    elif args.cmd == "install-hooks":
        backup = hooks_config.install(args.file, args.http_port)
        print(f"설치 완료: {args.file}" + (f" (백업: {backup})" if backup else ""))
    elif args.cmd == "uninstall-hooks":
        removed, backup = hooks_config.uninstall(args.file, args.http_port)
        print(f"제거한 훅 {removed}개: {args.file}" + (f" (백업: {backup})" if backup else ""))


if __name__ == "__main__":
    sys.exit(main())
