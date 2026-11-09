# worynim@gmail.com
"""
@file agent_bridge.py
@brief Agent_Monitor PC 데몬 — Claude Code 훅을 받아 세션 상태를 DeskStream 으로 보낸다
@details 구성: HTTP 훅 서버(127.0.0.1) → SessionTable → SerialLink(USB 시리얼, JSON Lines)
         - 모든 훅에 즉시 빈 200 으로 응답한다. PermissionRequest 도 P1 에서는 결정하지 않으므로(pass)
           평소처럼 터미널 대화상자로 결정된다.
         - 슬롯 내용이 바뀌면 바로, 그 밖에는 HEARTBEAT_SEC 마다 하트비트를 보낸다 (장치의 연결 표시용).

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
from sessions import SessionTable

DEFAULT_HTTP_PORT = 9230
HEARTBEAT_SEC = 2


def log(msg: str):
    print(f"{time.strftime('%H:%M:%S')} {msg}", flush=True)


class Bridge:
    def __init__(self, link_factory, verbose=False):
        self.table = SessionTable()
        self.lock = threading.Lock()
        self.dirty = threading.Event()
        self.sent = [None] * len(self.table.slots)   # 슬롯별 마지막으로 보낸 줄 (같으면 생략)
        self.alerts = []                              # 보낼 알림 줄 — 시리얼 쓰기는 송신 스레드만 한다
        self.verbose = verbose
        self.link = link_factory(self.on_device_line, self.on_connect)

    def start(self):
        self.link.start()
        threading.Thread(target=self._send_loop, daemon=True).start()

    # --- 훅 → 상태 ---
    def on_hook(self, event: str, payload: dict):
        with self.lock:
            for kind, slot in self.table.apply(event, payload, time.time()):
                self.alerts.append(protocol.alert_msg(kind, slot))
        if self.verbose:
            log(f"[hook] {event:<18} {str(payload.get('session_id', ''))[:8]} {payload.get('tool_name', '')}")
        self.dirty.set()

    # --- 장치 → 데몬 ---
    def on_connect(self):
        self.sent = [None] * len(self.sent)   # 새 연결이면 전체 재전송
        self.dirty.set()

    def on_device_line(self, line: str):
        msg = protocol.parse_device(line)
        if msg is None:
            return
        kind = msg.get("t")
        if kind == "hello":
            log(f"[device] hello fw={msg.get('fw')}")
            self.on_connect()
        elif kind == "btn":
            b, press = msg.get("b"), msg.get("press")
            log(f"[device] 버튼 {b} {press}")
            if press == "short" and isinstance(b, int):
                with self.lock:
                    if self.table.mark_read(b, time.time()):
                        self.dirty.set()
            # press == "long": PC 명령 실행은 P3 에서 결정 (현재는 기록만)

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
            for line in alerts:
                self.link.send(line)
            for i, v in enumerate(views):
                # el(경과 초)은 매번 달라지므로 비교에서 빼고, 장치가 직접 센다
                key = None if v is None else {**v, "el": None}
                if key != self.sent[i]:
                    self.sent[i] = key
                    self.link.send(protocol.slot_msg(i, v))
            if now - last_hb >= HEARTBEAT_SEC:
                last_hb = now
                self.link.send(protocol.HEARTBEAT)


def make_handler(bridge: Bridge):
    class HookHandler(BaseHTTPRequestHandler):
        def do_POST(self):
            event = self.path.rsplit("/", 1)[-1]
            body = self.rfile.read(int(self.headers.get("Content-Length", 0) or 0))
            self.send_response(200)          # 먼저 응답해 Claude Code 를 기다리게 하지 않는다
            self.send_header("Content-Length", "0")
            self.end_headers()
            try:
                payload = json.loads(body or b"{}")
            except json.JSONDecodeError:
                return
            if isinstance(payload, dict):
                bridge.on_hook(event, payload)

        def log_message(self, *_):
            pass
    return HookHandler


def cmd_run(args):
    from transport import SerialLink   # pyserial 은 run 에서만 필요
    bridge = Bridge(lambda on_line, on_connect: SerialLink(on_line, on_connect, port=args.serial, log=log),
                    verbose=args.verbose)
    bridge.start()
    server = ThreadingHTTPServer(("127.0.0.1", args.http_port), make_handler(bridge))
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
