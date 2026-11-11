# worynim@gmail.com
"""
@file approvals.py
@brief 권한 요청(PermissionRequest) 대기열 — 장치 버튼 결정을 기다리는 요청들
@details - 결정: allow / deny / pass. pass = 장치에서 결정하지 않음 → 빈 응답 → 터미널 대화상자로 결정.
         - 장치에는 cursor 가 가리키는 요청 하나만 보인다. "다음" 버튼이 cursor 를 돌린다.
         - 터미널에서 먼저 결정되면(P0: 훅과 터미널 동시 대기) 같은 세션의 후속 이벤트로 정리한다
           → clear_tool / clear_session. 터미널에서 거절(Esc)하면 후속 이벤트 없이 훅 연결만 끊기므로
           그 경우는 agent_bridge 가 연결 끊김을 보고 정리한다.
         스레드 안전하지 않다 — 호출하는 쪽(agent_bridge.Bridge)이 잠금을 잡는다.
"""
import threading
from dataclasses import dataclass, field

DECISIONS = ("allow", "deny", "pass")


@dataclass
class Pending:
    id: int
    sid: str
    slot: int
    name: str
    tool: str
    summary: str
    tool_input: dict
    decision: str | None = None
    done: threading.Event = field(default_factory=threading.Event)


def _same_call(asked, ran) -> bool:
    """권한 요청 때의 입력(asked)이 실행된 입력(ran)과 같은 호출인가 — dict 면 asked 의 항목이 ran 에 모두 같은 값으로 있으면 된다."""
    if isinstance(asked, dict) and isinstance(ran, dict):
        return all(k in ran and ran[k] == v for k, v in asked.items())
    return asked == ran


class ApprovalQueue:
    def __init__(self):
        self.items: list[Pending] = []
        self.cursor = 0
        self._next_id = 1

    def add(self, sid, slot, name, tool, summary, tool_input) -> Pending:
        p = Pending(self._next_id, sid, slot, name, tool, summary, tool_input)
        self._next_id += 1
        self.items.append(p)
        return p

    def current(self) -> Pending | None:
        return self.items[self.cursor] if self.items else None

    def resolve(self, pid: int, decision: str) -> Pending | None:
        """요청 pid 를 결정하고 대기열에서 뺀다. 없는 id(이미 정리됨)면 None."""
        for k, p in enumerate(self.items):
            if p.id == pid:
                p.decision = decision
                del self.items[k]
                if k < self.cursor:
                    self.cursor -= 1
                if self.cursor >= len(self.items):
                    self.cursor = 0
                p.done.set()
                return p
        return None

    def next(self, pid: int) -> bool:
        """보이는 요청이 pid 일 때만 다음 요청으로 넘긴다 (늦게 온 버튼이 엉뚱한 요청을 넘기지 않게)."""
        cur = self.current()
        if cur is None or cur.id != pid or len(self.items) < 2:
            return False
        self.cursor = (self.cursor + 1) % len(self.items)
        return True

    def clear_tool(self, sid: str, tool: str, tool_input) -> list:
        """터미널에서 승인돼 도구가 실행됨(PostToolUse) → 같은 도구·입력의 요청을 pass 로 정리.
        PostToolUse 의 입력에는 항목이 덧붙을 수 있어(AskUserQuestion 의 answers 등) 요청 때의 항목이 모두 같으면 같은 요청으로 본다."""
        ids = [p.id for p in self.items if p.sid == sid and p.tool == tool and _same_call(p.tool_input, tool_input)]
        return [self.resolve(i, "pass") for i in ids]

    def clear_session(self, sid: str) -> list:
        """턴이 끝나거나 새 프롬프트·세션 종료 → 그 세션의 남은 요청을 모두 pass 로 정리."""
        ids = [p.id for p in self.items if p.sid == sid]
        return [self.resolve(i, "pass") for i in ids]

    def position(self) -> tuple:
        """(보이는 요청 번호 1부터, 전체 개수)"""
        return (self.cursor + 1, len(self.items)) if self.items else (0, 0)
