# worynim@gmail.com
"""
@file protocol.py
@brief 데몬 ↔ DeskStream 시리얼 메시지 (JSON Lines, 한 줄 = 한 메시지)
@details 펌웨어 파서(ds::jsonGetString)는 \\" \\\\ \\/ 이외의 이스케이프를 거부하고 화면 글꼴은 ASCII 뿐이므로,
         문자열은 출력 가능한 ASCII 로 정리해 보낸다 (한글 등은 '?').
         길이 제한은 펌웨어 agent_state.h 의 버퍼 크기와 맞춘다.

PC → 장치
  {"t":"slot","i":0,"on":1,"name":"DeskStream","st":"run","tool":"Bash","sum":"npm test","el":12,"err":0,"sub":0}
  {"t":"slot","i":1,"on":0}
  {"t":"alert","kind":"done","i":0}
  {"t":"perm","id":7,"slot":0,"name":"web-app","tool":"Bash","sum":"rm -rf build","n":1,"q":2}   승인 화면
  {"t":"perm","id":0}                                                                             승인 화면 닫기
  {"t":"detail","i":0,"on":1,"name":"web-app","st":"run","mode":"default","el":12,"err":0,"sub":0,
   "msg":"마지막 답변","h0":"+Bash npm test","h1":">Edit app.py","h2":"","h3":""}             상세 화면 (h: '>' 실행 중 '+' 성공 '!' 실패)
  {"t":"cmd","b":1,"st":"run","label":"npm test","msg":""}      PC 명령 진행 (st = run | ok | fail, b = 버튼 0~3)
  {"t":"hb","quiet":1}                                          quiet = 무음 시간대 (없으면 0)
장치 → PC
  {"t":"hello","fw":"0.3.0"}
  {"t":"btn","b":0,"press":"short"}      short = 슬롯 b 상세 보기(+완료 확인), long = PC 명령 b+1 (승인 화면이 아닐 때)
  {"t":"decide","id":7,"act":"allow"}    act = allow | deny | pass(나중에) | next
"""
import json

NAME_MAX = 20        # 펌웨어 Slot::name[21] / Perm::name[21]
TOOL_MAX = 20        # 펌웨어 Slot::tool[21] / Perm::tool[21]
SUM_MAX = 63         # 펌웨어 Slot::sum[64]
PERM_SUM_MAX = 120   # 펌웨어 Perm::sum[121] — 승인 화면은 4화면 × 2줄에 나눠 보여준다
MODE_MAX = 12        # 펌웨어 Detail::mode[13]
MSG_MAX = 120        # 펌웨어 Detail::msg[121]
HIST_MAX = 21        # 펌웨어 Detail::h[4][22] — 상세 화면 한 줄
CMD_MSG_MAX = 12     # 펌웨어 CmdToast::msg[13]
PERM_NONE = '{"t":"perm","id":0}'


def clean(text, limit: int) -> str:
    """출력 가능한 ASCII 만 남기고(제어 문자는 공백, 그 밖은 '?'), 공백을 하나로 줄인 뒤 limit 글자로 자른다."""
    chars = "".join(" " if ord(c) < 32 else "?" if ord(c) > 126 else c for c in str(text or ""))
    out = " ".join(chars.split())
    return out[:limit]


def encode(msg: dict) -> str:
    return json.dumps(msg, separators=(",", ":"))


def slot_msg(i: int, view) -> str:
    if view is None:
        return encode({"t": "slot", "i": i, "on": 0})
    return encode({"t": "slot", "i": i, "on": 1,
                   "name": clean(view["name"], NAME_MAX), "st": view["st"],
                   "tool": clean(view["tool"], TOOL_MAX), "sum": clean(view["sum"], SUM_MAX),
                   "el": view["el"], "err": view["err"], "sub": view["sub"]})


def perm_msg(p, n: int, q: int) -> str:
    """승인 화면 메시지. p = approvals.Pending (None 이면 화면 닫기)."""
    if p is None:
        return PERM_NONE
    return encode({"t": "perm", "id": p.id, "slot": p.slot, "name": clean(p.name, NAME_MAX),
                   "tool": clean(p.tool, TOOL_MAX), "sum": clean(p.summary, PERM_SUM_MAX), "n": n, "q": q})


def permission_response(decision: str) -> bytes:
    """PermissionRequest 훅 응답 본문. pass 는 빈 본문(결정 안 함 → 터미널 대화상자)."""
    if decision not in ("allow", "deny"):
        return b""
    body = {"behavior": decision}
    if decision == "deny":
        body["message"] = "Denied from the DeskStream Agent_Monitor device."
    return json.dumps({"hookSpecificOutput": {"hookEventName": "PermissionRequest", "decision": body}}).encode()


def detail_msg(i: int, d) -> str:
    """상세 화면 메시지. d = SessionTable.detail() (None 이면 빈 슬롯)."""
    if d is None:
        return encode({"t": "detail", "i": i, "on": 0})
    msg = {"t": "detail", "i": i, "on": 1, "name": clean(d["name"], NAME_MAX), "st": d["st"],
           "mode": clean(d["mode"], MODE_MAX), "el": d["el"], "err": d["err"], "sub": d["sub"],
           "msg": clean(d["msg"], MSG_MAX)}
    hist = list(d["h"])[-4:]
    for k in range(4):
        msg[f"h{k}"] = clean(hist[k], HIST_MAX) if k < len(hist) else ""
    return encode(msg)


def cmd_msg(b: int, state: str, label: str, result: str) -> str:
    return encode({"t": "cmd", "b": b, "st": state, "label": clean(label, NAME_MAX),
                   "msg": clean(result, CMD_MSG_MAX)})


def heartbeat_msg(quiet: bool) -> str:
    return encode({"t": "hb", "quiet": 1 if quiet else 0})


def alert_msg(kind: str, i: int) -> str:
    return encode({"t": "alert", "kind": kind, "i": i})


def parse_device(line: str):
    """장치가 보낸 한 줄 → dict. JSON 이 아니면(부팅 로그 등) None."""
    line = line.strip()
    if not line.startswith("{"):
        return None
    try:
        msg = json.loads(line)
    except json.JSONDecodeError:
        return None
    return msg if isinstance(msg, dict) else None
