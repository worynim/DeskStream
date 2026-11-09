# worynim@gmail.com
"""
@file sessions.py
@brief Claude Code 세션 테이블 — 훅 이벤트를 세션 상태와 장치 슬롯(최대 4개)으로 바꾼다
@details 상태: idle / think / run / approve / input / done / error
         이벤트 → 상태 규칙은 PLAN.md 2.1 과 p0_spike/RESULTS.md 의 관찰 결과를 따른다.
         스레드 안전하지 않다 — 호출하는 쪽(agent_bridge.Bridge)이 잠금을 잡는다.
"""
import os
from dataclasses import dataclass

MAX_SLOTS = 4
STALE_SEC = 2 * 3600            # 이벤트가 이만큼 없으면 (SessionEnd 없이 꺼진 세션) 정리
ALERT_STATES = ("approve", "input", "done", "error")   # 이 상태로 바뀌면 장치가 소리를 낸다
INPUT_NOTIFICATIONS = ("idle_prompt", "elicitation_dialog", "agent_needs_input")
SUMMARY_KEYS = ("command", "file_path", "pattern", "url", "description", "prompt", "query")


@dataclass
class Session:
    sid: str
    name: str
    since: float                 # 현재 상태가 시작된 시각
    last_seen: float
    state: str = "idle"
    tool: str = ""
    summary: str = ""
    errors: int = 0
    subagents: int = 0
    slot: int = -1               # -1 = 장치에 표시되지 않음


def summarize_tool(tool_input) -> str:
    """도구 입력에서 사람이 알아볼 대표 값 하나 (Bash 명령, 파일 이름 등)."""
    if not isinstance(tool_input, dict):
        return ""
    for key in SUMMARY_KEYS:
        value = tool_input.get(key)
        if isinstance(value, str) and value:
            return os.path.basename(value) if key == "file_path" else value
    return ""


class SessionTable:
    def __init__(self, max_slots: int = MAX_SLOTS):
        self.sessions: dict[str, Session] = {}
        self.slots: list = [None] * max_slots   # 슬롯 i 에 표시 중인 session_id

    def apply(self, event: str, p: dict, now: float) -> list:
        """훅 이벤트 하나를 반영하고, 새로 알림이 필요한 (상태, 슬롯) 목록을 돌려준다."""
        sid = p.get("session_id")
        if not sid:
            return []
        if event == "SessionEnd":
            self.remove(sid)
            return []
        if event == "SubagentStop" and not p.get("agent_type"):
            return []   # 매 턴 Stop 뒤에 따라오는 내부 작업 (P0 발견 4)

        s = self.sessions.get(sid)
        if s is None:
            name = os.path.basename(p.get("cwd", "").rstrip("/")) or sid[:8]
            s = self.sessions[sid] = Session(sid, name, since=now, last_seen=now)
        s.last_seen = now

        new = None
        if event == "SessionStart":
            new = "idle"
        elif event == "UserPromptSubmit":
            new, s.tool, s.summary = "think", "", ""
        elif event in ("PreToolUse", "PermissionRequest"):
            new = "run" if event == "PreToolUse" else "approve"
            s.tool = p.get("tool_name", "")
            s.summary = summarize_tool(p.get("tool_input"))
        elif event == "PostToolUse":
            new = "think"
        elif event == "PostToolUseFailure":
            new = "think"
            s.errors += 1
        elif event == "Notification":
            kind = p.get("notification_type")
            if kind == "permission_prompt":
                new = "approve"
            elif kind in INPUT_NOTIFICATIONS and not (kind == "idle_prompt" and s.state in ("done", "idle")):
                new = "input"   # idle_prompt 는 완료 뒤 60초에도 오므로 done/idle 이면 무시
        elif event == "Stop":
            new, s.tool = "done", ""
            s.summary = p.get("last_assistant_message", "") or ""
        elif event == "StopFailure":
            new = "error"
        elif event == "SubagentStart":
            s.subagents += 1
        elif event == "SubagentStop":
            s.subagents = max(0, s.subagents - 1)

        changed = new is not None and new != s.state
        if changed:
            s.state, s.since = new, now
        self._ensure_slot(s)
        if changed and new in ALERT_STATES and s.slot >= 0:
            return [(new, s.slot)]
        return []

    def mark_read(self, slot: int, now: float) -> bool:
        """장치에서 슬롯 버튼을 누름 → 완료(done) 표시를 확인 처리해 idle 로."""
        sid = self.slots[slot] if 0 <= slot < len(self.slots) else None
        s = self.sessions.get(sid)
        if s is None or s.state != "done":
            return False
        s.state, s.since = "idle", now
        return True

    def remove(self, sid: str):
        s = self.sessions.pop(sid, None)
        if s is not None and s.slot >= 0:
            self.slots[s.slot] = None
            self._fill_free_slots()

    def prune(self, now: float):
        for sid in [k for k, s in self.sessions.items() if now - s.last_seen > STALE_SEC]:
            self.remove(sid)

    def view(self, now: float) -> list:
        """슬롯별 표시 정보 (빈 슬롯은 None)."""
        out = []
        for sid in self.slots:
            s = self.sessions.get(sid)
            out.append(None if s is None else {
                "name": s.name, "st": s.state, "tool": s.tool, "sum": s.summary,
                "el": int(now - s.since), "err": s.errors, "sub": s.subagents})
        return out

    def _ensure_slot(self, s: Session):
        """표시되지 않은 세션에 슬롯을 준다. 꽉 찼으면 주의가 필요 없는 가장 오래된 세션 자리를 빼앗는다."""
        if s.slot >= 0:
            return
        if None not in self.slots:
            victims = [self.sessions[x] for x in self.slots if self.sessions[x].state not in ALERT_STATES[:2]]
            if not victims:
                return
            old = min(victims, key=lambda v: v.last_seen)
            self.slots[old.slot], old.slot = None, -1
        s.slot = self.slots.index(None)
        self.slots[s.slot] = s.sid

    def _fill_free_slots(self):
        """빈 슬롯이 생기면 숨겨진 세션 중 최근 것부터 채운다."""
        hidden = sorted((s for s in self.sessions.values() if s.slot < 0), key=lambda v: -v.last_seen)
        for s in hidden:
            if None not in self.slots:
                break
            self._ensure_slot(s)
