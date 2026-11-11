# worynim@gmail.com
"""
@file protocol.py
@brief 데몬 ↔ DeskStream 시리얼 메시지 (JSON Lines, 한 줄 = 한 메시지)
@details 펌웨어 파서(ds::jsonGetString)는 \\" \\\\ \\/ 이외의 이스케이프(\\uXXXX 포함)를 거부하므로 JSON 은
         ensure_ascii=False 로 만들어 UTF-8 그대로 보낸다. 화면 글꼴(갈무리7)에 없는 글자는 펌웨어가 그리지 않는다.
         길이 제한은 펌웨어 agent_state.h 의 버퍼 크기(바이트)와 맞춘다 — 한글은 글자당 3바이트.

PC → 장치
  {"t":"slot","i":0,"on":1,"name":"DeskStream","st":"run","tool":"Bash","sum":"npm test","el":12,"err":0,"sub":0}
  {"t":"slot","i":1,"on":0}
  {"t":"alert","kind":"done","i":0}
  {"t":"perm","id":7,"slot":0,"name":"web-app","tool":"Bash","sum":"rm -rf build","n":1,"q":2}   승인 화면
  {"t":"perm","id":0}                                                                             승인 화면 닫기
  {"t":"detail","i":0,"on":1,"name":"web-app","st":"run","mode":"default","el":12,"err":0,"sub":0,
   "msg":"마지막 답변","h0":"+Bash npm test","h1":">Edit app.py","h2":"","h3":""}             상세 화면 (h: '>' 실행 중 '+' 성공 '!' 실패)
  {"t":"cmd","b":1,"st":"run","label":"npm test","msg":""}      PC 명령 진행 (st = run | ok | fail, b = 버튼 0~3)
  {"t":"cmds","l0":"Terminal","k0":"focus","l1":"","k1":"",…}   버튼별 길게 누르기 PC 명령 (펌웨어 0.6.3 이하의 빈 슬롯 화면용 — 이후 펌웨어는 무시)
  {"t":"usage","p5":23,"r5":5400,"p7":41,"r7":300000}           Claude Code 사용량 — p = 사용률 %(-1 = 모름), r = 리셋까지 남은 초(0 = 모름),
                                                                5 = 5시간 · 7 = 주간 (OLED 3·4 빈 슬롯 화면)
  {"t":"dev","mute":0,"bright":102}                             장치 설정 — 음소거 · OLED 밝기(contrast 1~255)
  {"t":"hb"}                                                    하트비트 (2초)
  {"t":"who"}                                                   USB 연결 시 — 장치가 hello 로 답한다
  {"t":"setkey","key":"<64 hex>"} / {"t":"nonce","n":"<hex>"}    서명 키 등록(USB 만) / BLE 연결마다 nonce (security.py)
장치 → PC
  {"t":"hello","fw":"0.4.0","kid":"a1b2c3d4"}   kid = 등록된 서명 키 id (없으면 빈 문자열)
  {"t":"btn","b":0,"press":"short"}      short = 슬롯 b 상세 보기(+완료 확인), long = PC 명령 b+1,
                                         close = 슬롯 b 세션 닫기 (상세 화면 BTN2) — 승인 화면이 아닐 때
  {"t":"decide","id":7,"act":"allow"}    act = allow | deny | pass(나중에) | next
"""
import json

NAME_MAX = 20        # (바이트) 펌웨어 Slot::name[21] / Perm::name[21]
TOOL_MAX = 20        # 펌웨어 Slot::tool[21] / Perm::tool[21]
SUM_MAX = 96         # 펌웨어 Slot::sum[97]
PERM_SUM_MAX = 240   # 펌웨어 Perm::sum[241] — 승인 화면은 4화면 × 2줄에 나눠 보여준다
MODE_MAX = 12        # 펌웨어 Detail::mode[13]
MSG_MAX = 1728       # 펌웨어 Detail::msg[1729] — 상세 화면 4페이지 (한글 576자)
HIST_MAX = 21        # 펌웨어 Detail::h[4][22] — 상세 화면 한 줄
CMD_MSG_MAX = 12     # 펌웨어 CmdToast::msg[13]
KIND_MAX = 5         # 펌웨어 CmdInfo::kind[6] — focus / shell / keys / error
PERM_NONE = '{"t":"perm","id":0}'
HEARTBEAT = '{"t":"hb"}'
WHO = '{"t":"who"}'


def clean(text, limit: int) -> str:
    """제어 문자는 공백으로 바꾸고 공백을 하나로 줄인 뒤, UTF-8 로 limit 바이트 안에서 글자 단위로 자른다."""
    chars = "".join(" " if ord(c) < 32 or ord(c) == 127 else c for c in str(text or ""))
    out = " ".join(chars.split())
    return out.encode("utf-8")[:limit].decode("utf-8", "ignore")


def encode(msg: dict) -> str:
    return json.dumps(msg, separators=(",", ":"), ensure_ascii=False)


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


def cmds_msg(items) -> str:
    """items = commands.button_labels() — 4개 (이름, 종류)."""
    msg = {"t": "cmds"}
    for k, (label, kind) in enumerate(items):
        msg[f"l{k}"] = clean(label, NAME_MAX)
        msg[f"k{k}"] = clean(kind, KIND_MAX)
    return encode(msg)


def usage_msg(windows, now: float) -> str:
    """windows = (5시간, 주간) 각각 (사용률 %, 리셋 시각 epoch 초) 또는 None(모름). 남은 시간은 장치가 직접 센다."""
    msg = {"t": "usage"}
    for key, w in zip("57", windows):
        pct, reset_at = w if w else (-1, 0)
        msg[f"p{key}"] = pct
        msg[f"r{key}"] = max(0, int(reset_at - now)) if reset_at else 0
    return encode(msg)


def dev_msg(mute: bool, brightness_pct: int) -> str:
    """장치 설정. 밝기 %(1~100) → SSD1306 contrast(1~255)."""
    contrast = max(1, min(255, round(brightness_pct * 255 / 100)))
    return encode({"t": "dev", "mute": 1 if mute else 0, "bright": contrast})


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
