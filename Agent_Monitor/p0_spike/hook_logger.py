# worynim@gmail.com
"""
@file hook_logger.py
@brief P0 스파이크 — Claude Code HTTP 훅 수신·기록 서버
@details POST /hooks/<Event> 로 들어온 훅 페이로드를 logs/<날짜>.jsonl 에 기록하고 한 줄 요약을 출력한다.
         PermissionRequest 는 --perm 옵션에 따라 보류 후 allow / deny / pass(빈 응답) 로 응답한다.
         표준 라이브러리만 사용한다.

사용 예:
  python3 hook_logger.py                       # 권한 요청은 즉시 pass (터미널 대화상자로 넘김)
  python3 hook_logger.py --perm allow --hold 5 # 5초 보류 후 허용
  python3 hook_logger.py --perm ask            # 콘솔에서 a/d/p 입력으로 결정 (버튼 흉내)
  python3 hook_logger.py --seq allow,deny,pass,timeout  # 요청마다 순서대로 결정 (자동 시나리오)
    timeout = --timeout-hold 초만큼 보류한 뒤 pass (훅 timeout 초과 상황 재현)
"""
import argparse
import json
import sys
import threading
import time
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

LOG_DIR = Path(__file__).parent / "logs"
ask_lock = threading.Lock()  # ask 모드에서 콘솔 입력을 한 번에 하나만 받기 위함


def summarize(event: str, p: dict) -> str:
    """이벤트별로 눈에 띄는 필드만 한 줄로 요약한다."""
    sid = str(p.get("session_id", ""))[:8]
    proj = Path(p.get("cwd", "")).name
    extra = ""
    if "tool_name" in p:
        ti = p.get("tool_input") or {}
        extra = f"{p['tool_name']}: {ti.get('command') or ti.get('file_path') or ''}"[:80]
    elif event == "Notification":
        extra = f"[{p.get('notification_type')}] {p.get('message', '')}"[:80]
    elif event == "UserPromptSubmit":
        extra = str(p.get("prompt", ""))[:60]
    elif event == "SessionStart":
        extra = f"source={p.get('source')}"
    return f"{datetime.now():%H:%M:%S} {sid} {proj:<14} {event:<20} {extra}"


def permission_response(behavior: str) -> bytes:
    """PermissionRequest 결정 JSON. pass 는 빈 본문(결정 안 함)."""
    if behavior == "pass":
        return b""
    decision = {"behavior": behavior}
    if behavior == "deny":
        decision["message"] = "P0 hook_logger 에서 거절"
    return json.dumps({"hookSpecificOutput": {
        "hookEventName": "PermissionRequest", "decision": decision}}).encode()


class HookHandler(BaseHTTPRequestHandler):
    args = None  # main()에서 설정

    def do_POST(self):
        event = self.path.rsplit("/", 1)[-1]
        body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        try:
            payload = json.loads(body or b"{}")
        except json.JSONDecodeError:
            payload = {"_raw": body.decode(errors="replace")}

        received = time.time()
        print(summarize(event, payload), flush=True)
        out = b""
        decision = None
        if event == "PermissionRequest":
            decision = self.decide()
            out = permission_response(decision)
        self.log_record(event, payload, received, decision)

        try:
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(out)))
            self.end_headers()
            self.wfile.write(out)
        except (BrokenPipeError, ConnectionResetError):
            print(f"    ↳ 응답 전송 실패: Claude Code 가 연결을 끊음 (훅 timeout 초과 추정)", flush=True)

    def decide(self) -> str:
        a = self.args
        if a.seq:
            with ask_lock:
                step = a.seq.pop(0) if a.seq else "pass"
            if step == "timeout":
                print(f"    ↳ [seq] {a.timeout_hold}초 보류 후 pass (timeout 재현)", flush=True)
                time.sleep(a.timeout_hold)
                return "pass"
            print(f"    ↳ [seq] {step}", flush=True)
            return step
        if a.perm == "ask":
            with ask_lock:
                ans = input("    ↳ 결정 [a]llow / [d]eny / [p]ass: ").strip().lower()[:1]
            return {"a": "allow", "d": "deny"}.get(ans, "pass")
        if a.hold:
            print(f"    ↳ {a.hold}초 보류 후 {a.perm}", flush=True)
            time.sleep(a.hold)
        return a.perm

    def log_record(self, event, payload, received, decision):
        LOG_DIR.mkdir(exist_ok=True)
        rec = {"ts": received, "event": event, "payload": payload}
        if decision:
            rec["decision"] = decision
            rec["held_sec"] = round(time.time() - received, 2)
        with open(LOG_DIR / f"{datetime.now():%Y%m%d}.jsonl", "a", encoding="utf-8") as f:
            f.write(json.dumps(rec, ensure_ascii=False) + "\n")

    def log_message(self, *_):  # 기본 접근 로그 끄기
        pass


def main():
    ap = argparse.ArgumentParser(description="Claude Code HTTP 훅 수신·기록 (P0 스파이크)")
    ap.add_argument("--port", type=int, default=9230)
    ap.add_argument("--perm", choices=["pass", "allow", "deny", "ask"], default="pass",
                    help="PermissionRequest 응답 방식 (기본 pass = 빈 응답)")
    ap.add_argument("--hold", type=float, default=0, help="응답 전 보류 시간(초)")
    ap.add_argument("--seq", type=lambda s: s.split(","), default=None,
                    help="요청마다 순서대로 쓸 결정 목록 (allow,deny,pass,timeout). 지정 시 --perm 무시")
    ap.add_argument("--timeout-hold", type=float, default=45, help="seq 의 timeout 단계 보류 시간(초)")
    HookHandler.args = ap.parse_args()

    server = ThreadingHTTPServer(("127.0.0.1", HookHandler.args.port), HookHandler)
    print(f"hook_logger: 127.0.0.1:{HookHandler.args.port}  perm={HookHandler.args.perm} "
          f"hold={HookHandler.args.hold}s  log={LOG_DIR}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        sys.exit(0)


if __name__ == "__main__":
    main()
