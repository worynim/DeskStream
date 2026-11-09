# worynim@gmail.com
"""
@file demo_hooks.py
@brief 화면 확인용 데모 — 실행 중인 agent_bridge 에 가짜 훅을 보내 4개 슬롯을 여러 상태로 만든다
@details Claude Code 없이 장치 화면·알림음을 확인할 때 쓴다. 표준 라이브러리만 사용.
  python3 demo_hooks.py            # 슬롯 4개: 실행 중 / 승인 대기 / 완료 / 생각 중
  python3 demo_hooks.py --end      # 데모 세션 정리
"""
import argparse
import json
import time
import urllib.request

SESSIONS = ("demo-run", "demo-approve", "demo-done", "demo-think")


def post(port, event, sid, name, **payload):
    body = json.dumps({"session_id": sid, "cwd": f"/work/{name}", **payload}).encode()
    req = urllib.request.Request(f"http://127.0.0.1:{port}/hooks/{event}", body,
                                 {"Content-Type": "application/json"})
    urllib.request.urlopen(req, timeout=3).read()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--http-port", type=int, default=9230)
    ap.add_argument("--end", action="store_true", help="데모 세션 정리")
    a = ap.parse_args()
    p = a.http_port

    if a.end:
        for sid in SESSIONS:
            post(p, "SessionEnd", sid, sid)
        return

    post(p, "UserPromptSubmit", SESSIONS[0], "DeskStream")
    post(p, "PreToolUse", SESSIONS[0], "DeskStream", tool_name="Bash",
         tool_input={"command": "arduino-cli compile --fqbn esp32:esp32 Agent_Monitor"})
    time.sleep(1)
    post(p, "UserPromptSubmit", SESSIONS[1], "web-app")
    post(p, "PermissionRequest", SESSIONS[1], "web-app", tool_name="Bash",
         tool_input={"command": "rm -rf node_modules && npm install"})
    time.sleep(1)
    post(p, "UserPromptSubmit", SESSIONS[2], "api-server")
    post(p, "PostToolUseFailure", SESSIONS[2], "api-server", tool_name="Bash")
    post(p, "Stop", SESSIONS[2], "api-server",
         last_assistant_message="Fixed the login bug and added 3 tests. All tests pass.")
    time.sleep(1)
    post(p, "UserPromptSubmit", SESSIONS[3], "docs")
    post(p, "PreToolUse", SESSIONS[3], "docs", tool_name="Edit",
         tool_input={"file_path": "/work/docs/README.md"})
    post(p, "PostToolUse", SESSIONS[3], "docs", tool_name="Edit")


if __name__ == "__main__":
    main()
