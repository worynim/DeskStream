# worynim@gmail.com
"""
@file test_bridge.py
@brief 데몬 단위 테스트 (표준 unittest, pyserial 불필요)
@details 실행: cd Agent_Monitor/bridge_py && python3 -m unittest discover -s tests -v
         fixtures/p0_run2.jsonl = P0 자동 시나리오 실제 훅 기록 (개인 경로 제거본)
"""
import json
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE.parent))

import hooks_config  # noqa: E402
import protocol  # noqa: E402
from agent_bridge import Bridge  # noqa: E402
from approvals import ApprovalQueue  # noqa: E402
from sessions import SessionTable, summarize_tool  # noqa: E402


def ev(sid="s1", **kw):
    return {"session_id": sid, "cwd": "/work/proj_" + sid, **kw}


class TestSessionTable(unittest.TestCase):
    def test_basic_flow(self):
        t = SessionTable()
        t.apply("SessionStart", ev(), 0)
        self.assertEqual(t.view(0)[0]["st"], "idle")
        self.assertEqual(t.view(0)[0]["name"], "proj_s1")
        t.apply("UserPromptSubmit", ev(prompt="hi"), 1)
        self.assertEqual(t.view(1)[0]["st"], "think")
        t.apply("PreToolUse", ev(tool_name="Bash", tool_input={"command": "npm test"}), 2)
        v = t.view(5)[0]
        self.assertEqual((v["st"], v["tool"], v["sum"], v["el"]), ("run", "Bash", "npm test", 3))
        t.apply("PostToolUse", ev(tool_name="Bash"), 6)
        self.assertEqual(t.view(6)[0]["st"], "think")
        alerts = t.apply("Stop", ev(last_assistant_message="All done"), 7)
        self.assertEqual(alerts, [("done", 0)])
        self.assertEqual(t.view(7)[0]["sum"], "All done")

    def test_first_event_without_session_start_registers(self):
        t = SessionTable()
        t.apply("UserPromptSubmit", ev(), 0)
        self.assertEqual(t.view(0)[0]["st"], "think")

    def test_permission_alert_once(self):
        t = SessionTable()
        self.assertEqual(t.apply("PermissionRequest", ev(tool_name="Bash", tool_input={"command": "rm x"}), 0),
                         [("approve", 0)])
        # 6초 뒤 오는 permission_prompt 알림은 같은 상태라 다시 울리지 않는다
        self.assertEqual(t.apply("Notification", ev(notification_type="permission_prompt"), 6), [])

    def test_idle_prompt_after_done_ignored(self):
        t = SessionTable()
        t.apply("Stop", ev(), 0)
        self.assertEqual(t.apply("Notification", ev(notification_type="idle_prompt"), 60), [])
        self.assertEqual(t.view(60)[0]["st"], "done")

    def test_internal_subagent_stop_ignored(self):
        t = SessionTable()
        t.apply("SubagentStop", ev(agent_type=""), 0)
        self.assertEqual(t.sessions, {})

    def test_errors_and_failure(self):
        t = SessionTable()
        t.apply("PostToolUseFailure", ev(), 0)
        self.assertEqual(t.view(0)[0]["err"], 1)
        self.assertEqual(t.apply("StopFailure", ev(), 1), [("error", 0)])

    def test_mark_read(self):
        t = SessionTable()
        t.apply("Stop", ev(), 0)
        self.assertTrue(t.mark_read(0, 1))
        self.assertEqual(t.view(1)[0]["st"], "idle")
        self.assertFalse(t.mark_read(0, 2))
        self.assertFalse(t.mark_read(3, 2))   # 빈 슬롯

    def test_session_end_frees_slot_and_fills_hidden(self):
        t = SessionTable(max_slots=2)
        t.apply("PermissionRequest", ev("a"), 0)
        t.apply("PermissionRequest", ev("b"), 1)
        t.apply("UserPromptSubmit", ev("c"), 2)      # 둘 다 승인 대기 → 자리를 빼앗지 못함
        self.assertEqual(t.sessions["c"].slot, -1)
        t.apply("SessionEnd", ev("a"), 3)
        self.assertEqual(t.sessions["c"].slot, 0)

    def test_full_slots_steal_least_recent_non_attention(self):
        t = SessionTable(max_slots=2)
        t.apply("UserPromptSubmit", ev("a"), 0)
        t.apply("PermissionRequest", ev("b"), 1)
        t.apply("UserPromptSubmit", ev("c"), 2)
        self.assertEqual((t.sessions["a"].slot, t.sessions["c"].slot), (-1, 0))

    def test_prune_stale(self):
        t = SessionTable()
        t.apply("UserPromptSubmit", ev(), 0)
        t.prune(3 * 3600)
        self.assertEqual(t.view(0)[0], None)

    def test_summarize_tool(self):
        self.assertEqual(summarize_tool({"file_path": "/a/b/main.cpp"}), "main.cpp")
        self.assertEqual(summarize_tool({"pattern": "TODO"}), "TODO")
        self.assertEqual(summarize_tool(None), "")

    def test_replay_p0_log(self):
        lines = (HERE / "fixtures" / "p0_run2.jsonl").read_text(encoding="utf-8").splitlines()
        recs = [json.loads(x) for x in lines]
        recs.sort(key=lambda r: r["ts"])
        t = SessionTable()
        alerts = []
        for r in recs:
            alerts += [a[0] for a in t.apply(r["event"], r["payload"], r["ts"])]
        # S1 승인(approve) → done, S2 approve → done, S3·S4 approve → (Esc) → done … 마지막 SessionEnd 로 정리
        self.assertEqual(alerts.count("approve"), 4)
        self.assertIn("done", alerts)
        self.assertEqual(t.sessions, {})


class TestProtocol(unittest.TestCase):
    def test_clean(self):
        self.assertEqual(protocol.clean("한글 a\nb\x1b", 20), "?? a b")
        self.assertEqual(protocol.clean("x" * 30, 5), "xxxxx")

    def test_slot_msg_only_simple_escapes(self):
        line = protocol.slot_msg(0, {"name": "p", "st": "run", "tool": "Bash",
                                     "sum": 'echo "hi" \\ 완료', "el": 3, "err": 0, "sub": 0})
        self.assertNotIn("\\u", line)
        self.assertEqual(json.loads(line)["sum"], 'echo "hi" \\ ??')
        self.assertEqual(protocol.slot_msg(2, None), '{"t":"slot","i":2,"on":0}')

    def test_parse_device(self):
        self.assertEqual(protocol.parse_device('{"t":"btn","b":1,"press":"short"}\r\n')["b"], 1)
        self.assertIsNone(protocol.parse_device("ESP-ROM:esp32c3"))
        self.assertIsNone(protocol.parse_device("{broken"))


class TestApprovalQueue(unittest.TestCase):
    def setUp(self):
        self.q = ApprovalQueue()
        self.a = self.q.add("s1", 0, "p", "Bash", "rm a", {"command": "rm a"})
        self.b = self.q.add("s1", 0, "p", "Bash", "rm b", {"command": "rm b"})
        self.c = self.q.add("s2", 1, "q", "Edit", "x.py", {"file_path": "x.py"})

    def test_resolve_and_cursor(self):
        self.assertEqual(self.q.position(), (1, 3))
        self.assertTrue(self.q.next(self.a.id))
        self.assertFalse(self.q.next(self.a.id))          # 보이는 요청이 아니면 무시
        self.assertIs(self.q.current(), self.b)
        self.q.resolve(self.a.id, "deny")                  # 앞쪽 항목이 빠져도 보던 요청 유지
        self.assertIs(self.q.current(), self.b)
        self.assertEqual(self.a.decision, "deny")
        self.assertTrue(self.a.done.is_set())
        self.assertIsNone(self.q.resolve(self.a.id, "allow"))   # 이미 정리된 id
        self.q.resolve(self.c.id, "allow")
        self.q.resolve(self.b.id, "allow")
        self.assertEqual((self.q.current(), self.q.position()), (None, (0, 0)))

    def test_cursor_wraps_when_last_removed(self):
        self.q.next(self.a.id)
        self.q.next(self.b.id)
        self.q.resolve(self.c.id, "pass")
        self.assertIs(self.q.current(), self.a)

    def test_clear_tool_and_session(self):
        cleared = self.q.clear_tool("s1", "Bash", {"command": "rm b"})
        self.assertEqual([p.id for p in cleared], [self.b.id])
        self.assertEqual(self.b.decision, "pass")
        self.assertEqual(self.q.clear_tool("s1", "Bash", {"command": "other"}), [])
        self.assertEqual(len(self.q.clear_session("s1")), 1)
        self.assertEqual([p.id for p in self.q.items], [self.c.id])


class FakeLink:
    def __init__(self, connected=True):
        self.connected = connected
        self.lines = []

    def start(self):
        pass

    def send(self, line):
        self.lines.append(line)


class TestBridgePermission(unittest.TestCase):
    def make(self, connected=True, hold=2.0):
        link = FakeLink(connected)
        return Bridge(lambda *_: link, hold_sec=hold), link

    def ask(self, bridge, cmd="rm -rf build", sid="s1"):
        """권한 요청을 별도 스레드로 보내고 (스레드, 결과 dict) 를 돌려준다."""
        result = {}
        payload = ev(sid, tool_name="Bash", tool_input={"command": cmd})
        t = threading.Thread(target=lambda: result.setdefault("body", bridge.on_permission(payload)))
        t.start()
        for _ in range(100):                       # 대기열에 들어갈 때까지
            if any(p.tool_input == {"command": cmd} for p in bridge.approvals.items):
                break
            time.sleep(0.01)
        return t, result

    def decision_of(self, body):
        return json.loads(body)["hookSpecificOutput"]["decision"]["behavior"] if body else "pass"

    def test_device_allow(self):
        bridge, _ = self.make()
        t, r = self.ask(bridge)
        pid = bridge.approvals.current().id
        bridge.on_device_line(json.dumps({"t": "decide", "id": pid, "act": "allow"}))
        t.join(2)
        self.assertEqual(self.decision_of(r["body"]), "allow")
        self.assertEqual(bridge.table.view(0)[0]["st"], "run")

    def test_device_deny_has_message(self):
        bridge, _ = self.make()
        t, r = self.ask(bridge)
        bridge.on_device_line(json.dumps({"t": "decide", "id": bridge.approvals.current().id, "act": "deny"}))
        t.join(2)
        d = json.loads(r["body"])["hookSpecificOutput"]["decision"]
        self.assertEqual(d["behavior"], "deny")
        self.assertIn("DeskStream", d["message"])

    def test_later_and_unknown_id(self):
        bridge, _ = self.make()
        t, r = self.ask(bridge)
        bridge.on_device_line('{"t":"decide","id":999,"act":"allow"}')     # 없는 id 는 무시
        bridge.on_device_line('{"t":"decide","id":"1","act":"allow"}')     # 숫자가 아니면 무시
        self.assertTrue(t.is_alive())
        bridge.on_device_line(json.dumps({"t": "decide", "id": bridge.approvals.current().id, "act": "pass"}))
        t.join(2)
        self.assertEqual(r["body"], b"")

    def test_terminal_decided_first_clears(self):
        bridge, _ = self.make()
        t, r = self.ask(bridge, "npm test")
        bridge.on_hook("PostToolUse", ev(tool_name="Bash", tool_input={"command": "npm test"}))
        t.join(2)
        self.assertEqual(r["body"], b"")
        self.assertEqual(bridge.approvals.items, [])

    def test_stop_clears_session(self):
        bridge, _ = self.make()
        t, r = self.ask(bridge)
        bridge.on_hook("Stop", ev())
        t.join(2)
        self.assertEqual(r["body"], b"")

    def test_timeout_passes(self):
        bridge, _ = self.make(hold=0.2)
        t, r = self.ask(bridge)
        t.join(2)
        self.assertEqual(r["body"], b"")
        self.assertEqual(bridge.approvals.items, [])

    def test_disconnected_passes_immediately(self):
        bridge, _ = self.make(connected=False)
        self.assertEqual(bridge.on_permission(ev(tool_name="Bash", tool_input={"command": "x"})), b"")
        self.assertEqual(bridge.approvals.items, [])

    def test_perm_message(self):
        bridge, _ = self.make()
        t, _r = self.ask(bridge)
        p = bridge.approvals.current()
        msg = json.loads(protocol.perm_msg(p, *bridge.approvals.position()))
        self.assertEqual((msg["id"], msg["slot"], msg["name"], msg["sum"], msg["n"], msg["q"]),
                         (p.id, 0, "proj_s1", "rm -rf build", 1, 1))
        self.assertEqual(protocol.perm_msg(None, 0, 0), '{"t":"perm","id":0}')
        bridge.on_hook("Stop", ev())
        t.join(2)


class TestHooksConfig(unittest.TestCase):
    def test_install_preserves_user_hooks_and_uninstall(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "settings.json"
            user_hook = {"type": "command", "command": "echo mine"}
            path.write_text(json.dumps({"model": "x", "hooks": {"Stop": [{"hooks": [user_hook]}]}}))
            hooks_config.install(path, 9230)
            hooks_config.install(path, 9230)                 # 두 번 설치해도 중복 없음
            s = json.loads(path.read_text())
            self.assertEqual(len(s["hooks"]["Stop"]), 2)
            self.assertEqual(s["hooks"]["SessionStart"][0]["hooks"][0]["type"], "command")
            removed, backup = hooks_config.uninstall(path, 9230)
            self.assertEqual(removed, 12)
            self.assertTrue(backup.exists())
            self.assertEqual(json.loads(path.read_text()), {"model": "x", "hooks": {"Stop": [{"hooks": [user_hook]}]}})


if __name__ == "__main__":
    unittest.main()
