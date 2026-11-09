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
  {"t":"hb"}
장치 → PC
  {"t":"hello","fw":"0.1.0"}
  {"t":"btn","b":0,"press":"short"}      press = short | long
"""
import json

NAME_MAX = 20   # 펌웨어 Slot::name[21]
TOOL_MAX = 20   # 펌웨어 Slot::tool[21]
SUM_MAX = 63    # 펌웨어 Slot::sum[64]
HEARTBEAT = '{"t":"hb"}'


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
