# worynim@gmail.com
"""
@file test_bridge.py
@brief 데몬 단위 테스트 (표준 unittest, pyserial 불필요)
@details 실행: cd Agent_Monitor/bridge_py && python3 -m unittest discover -s tests -v
         fixtures/p0_run2.jsonl = P0 자동 시나리오 실제 훅 기록 (개인 경로 제거본)
"""
import json
import os
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE.parent))

import commands  # noqa: E402
import hooks_config  # noqa: E402
import protocol  # noqa: E402
from agent_bridge import Bridge  # noqa: E402
from approvals import ApprovalQueue  # noqa: E402
from security import DeviceAuth  # noqa: E402
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


NO_CONFIG = Path(tempfile.gettempdir()) / "agent_monitor_test_absent.toml"
TEST_KEY_DIR = tempfile.TemporaryDirectory()


def test_auth(name="key"):
    """사용자 홈의 실제 키를 건드리지 않는 테스트용 DeviceAuth"""
    return DeviceAuth(Path(TEST_KEY_DIR.name) / name)


class FakeLink:
    def __init__(self, connected=True, kind="usb"):
        self.connected = connected
        self.kind = kind if connected else None
        self.lines = []

    def start(self):
        pass

    def send(self, line):
        self.lines.append(line)


class TestBridgePermission(unittest.TestCase):
    def make(self, connected=True, hold=2.0):
        link = FakeLink(connected)
        return Bridge(lambda *_: link, hold_sec=hold, config_path=NO_CONFIG, auth=test_auth()), link

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


class TestDetail(unittest.TestCase):
    def test_history_and_detail(self):
        t = SessionTable()
        t.apply("UserPromptSubmit", ev(permission_mode="plan"), 0)
        for k in range(5):
            t.apply("PreToolUse", ev(tool_name="Bash", tool_input={"command": f"c{k}"}), k)
            t.apply("PostToolUse" if k != 3 else "PostToolUseFailure", ev(tool_name="Bash"), k)
        t.apply("PreToolUse", ev(tool_name="Edit", tool_input={"file_path": "/a/x.py"}), 9)
        d = t.detail(0, 10)
        self.assertEqual(d["h"], ["+Bash c2", "!Bash c3", "+Bash c4", ">Edit x.py"])   # 최근 4개
        self.assertEqual((d["mode"], d["err"], d["st"]), ("plan", 1, "run"))
        t.apply("Stop", ev(last_assistant_message="All good"), 11)
        self.assertEqual(t.detail(0, 11)["msg"], "All good")
        self.assertIsNone(t.detail(3, 11))
        self.assertIsNone(t.detail(-1, 11))

    def test_detail_msg(self):
        line = protocol.detail_msg(1, {"name": "p", "st": "done", "mode": "acceptEdits-long", "el": 3,
                                       "err": 0, "sub": 0, "msg": "x" * 200, "h": ["+Bash " + "y" * 40]})
        m = json.loads(line)
        self.assertEqual((m["mode"], len(m["msg"]), len(m["h0"]), m["h3"]), ("acceptEdits-", 120, 21, ""))
        self.assertEqual(protocol.detail_msg(2, None), '{"t":"detail","i":2,"on":0}')


class TestCommands(unittest.TestCase):
    def test_parse_valid(self):
        c = commands.parse_config({
            "commands": [{"button": 1, "type": "focus", "app": "Terminal"},
                         {"button": 2, "type": "shell", "command": "true", "label": "T"},
                         {"button": 3, "type": "keys", "text": "continue", "key": "return"}]})
        self.assertEqual(sorted(c.commands), [1, 2, 3])
        self.assertEqual(c.commands[1].label, "focus")     # label 없으면 type

    def test_parse_device(self):
        c = commands.parse_config({"device": {"mute": True, "brightness": 25}})
        self.assertEqual((c.mute, c.brightness), (True, 25))
        c = commands.parse_config({})
        self.assertEqual((c.mute, c.brightness), (False, 100))
        self.assertEqual(protocol.dev_msg(False, 1), '{"t":"dev","mute":0,"bright":3}')
        self.assertEqual(protocol.dev_msg(True, 100), '{"t":"dev","mute":1,"bright":255}')

    def test_parse_errors(self):
        bad = [
            {"device": {"mute": "yes"}},
            {"device": {"brightness": 0}},
            {"device": {"brightness": 101}},
            {"device": {"brightness": True}},
            {"device": "loud"},
            {"commands": [{"button": 5, "type": "focus", "app": "a"}]},
            {"commands": [{"button": 1, "type": "rm"}]},
            {"commands": [{"button": 1, "type": "focus"}]},
            {"commands": [{"button": 1, "type": "shell"}]},
            {"commands": [{"button": 1, "type": "keys"}]},
            {"commands": [{"button": 1, "type": "keys", "key": "f13"}]},
            {"commands": [{"button": 1, "type": "keys", "text": "계속"}]},
            {"commands": [{"button": 1, "type": "focus", "app": "a"}, {"button": 1, "type": "focus", "app": "b"}]},
        ]
        for data in bad:
            with self.assertRaises(commands.ConfigError, msg=str(data)):
                commands.parse_config(data)

    def test_build_argv(self):
        self.assertEqual(commands.build_argv(commands.Command(1, "t", "focus", app="Ghostty")),
                         ["open", "-a", "Ghostty"])
        script = commands.build_argv(commands.Command(1, "c", "keys", app="Terminal", text='say "hi"',
                                                      key="return"))[2]
        self.assertIn('tell application "Terminal" to activate', script)
        self.assertIn('keystroke "say \\"hi\\""', script)
        self.assertIn("key code 36", script)
        one = commands.build_argv(commands.Command(1, "e", "keys", key="escape"))[2]
        self.assertEqual(one, 'tell application "System Events" to key code 53')

    def test_shell_execute(self):
        ok, msg = commands.execute(commands.Command(2, "t", "shell", command="exit 3"))
        self.assertEqual((ok, msg), (False, "exit 3"))
        ok, msg = commands.execute(commands.Command(2, "t", "shell", command="sleep 2", timeout=0.2))
        self.assertEqual((ok, msg), (False, "timeout"))

    @unittest.skipIf(os.name == "nt", "Windows 는 st_mode 권한 검사를 하지 않음")
    def test_load_rejects_group_writable(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "c.toml"
            p.write_text('[[commands]]\nbutton = 1\ntype = "shell"\ncommand = "true"\n')
            p.chmod(0o600)
            self.assertIn(1, commands.load_config(p).commands)
            p.chmod(0o620)
            with self.assertRaises(commands.ConfigError):
                commands.load_config(p)
            p.write_text("not = [toml")
            p.chmod(0o600)
            with self.assertRaises(commands.ConfigError):
                commands.load_config(p)
        self.assertEqual(commands.load_config(NO_CONFIG).commands, {})

    def test_no_accessibility_detected(self):
        ko = "36:47: execution error: System Events에 오류 발생: osascript에서 키스트로크를 보내도록 허용되지 않습니다. (1002)"
        self.assertTrue(commands._no_accessibility(ko.encode()))
        self.assertTrue(commands._no_accessibility(b"osascript is not allowed to send keystrokes."))
        self.assertFalse(commands._no_accessibility(b"syntax error"))

    def test_example_config_is_valid(self):
        c = commands.parse_config(__import__("tomllib").loads(commands.EXAMPLE_PATH.read_text(encoding="utf-8")))
        self.assertEqual(sorted(c.commands), [1, 2, 3, 4])


class TestBridgeButtons(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.cfg = Path(self.dir.name) / "c.toml"
        self.cfg.write_text('[[commands]]\nbutton = 2\nlabel = "Test"\ntype = "shell"\ncommand = "true"\n')
        self.cfg.chmod(0o600)
        self.ran = []
        self.link = FakeLink()
        self.bridge = Bridge(lambda *_: self.link, config_path=self.cfg, auth=test_auth(),
                             executor=lambda c: (self.ran.append(c.label), (True, "exit 0"))[1])

    def tearDown(self):
        self.dir.cleanup()

    def wait_outbox(self, n):
        for _ in range(100):
            if len(self.bridge.outbox) >= n:
                return
            time.sleep(0.01)

    def test_long_press_runs_configured_command(self):
        self.bridge.on_device_line('{"t":"btn","b":1,"press":"long"}')
        self.wait_outbox(2)
        msgs = [json.loads(x) for x in self.bridge.outbox]
        self.assertEqual(self.ran, ["Test"])
        self.assertEqual([(m["b"], m["st"]) for m in msgs], [(1, "run"), (1, "ok")])

    def test_long_press_unset_button(self):
        self.bridge.on_device_line('{"t":"btn","b":0,"press":"long"}')
        m = json.loads(self.bridge.outbox[-1])
        self.assertEqual((m["b"], m["st"], m["msg"]), (0, "fail", "not set"))
        self.assertEqual(self.ran, [])

    def test_short_press_watches_slot(self):
        self.bridge.on_hook("Stop", ev())
        self.bridge.on_device_line('{"t":"btn","b":0,"press":"short"}')
        self.assertEqual(self.bridge.watch, 0)
        self.assertEqual(self.bridge.table.view(0)[0]["st"], "idle")   # 완료 확인
        self.bridge.on_device_line('{"t":"btn","b":9,"press":"short"}')  # 범위 밖 무시
        self.assertEqual(self.bridge.watch, 0)

    def test_button_labels_and_cmds_msg(self):
        self.assertEqual(commands.button_labels(self.cfg), [("", ""), ("Test", "shell"), ("", ""), ("", "")])
        m = json.loads(protocol.cmds_msg(commands.button_labels(self.cfg)))
        self.assertEqual((m["l1"], m["k1"], m["l0"], m["k3"]), ("Test", "shell", "", ""))
        self.assertEqual(commands.button_labels(NO_CONFIG), [("", "")] * 4)
        self.cfg.write_text("broken = [")
        self.assertEqual(commands.button_labels(self.cfg)[0], ("config error", "error"))

    def test_cmds_line_reloads_on_change(self):
        first = json.loads(self.bridge.config_lines()[0])
        self.assertEqual(first["l1"], "Test")
        self.cfg.write_text('[[commands]]\nbutton = 1\nlabel = "Finder"\ntype = "focus"\napp = "Finder"\n')
        os.utime(self.cfg, (time.time() + 5, time.time() + 5))   # 수정 시각이 확실히 바뀌게
        second = json.loads(self.bridge.config_lines()[0])
        self.assertEqual((second["l0"], second["k0"], second["l1"]), ("Finder", "focus", ""))

    def test_device_settings_line(self):
        self.assertEqual(json.loads(self.bridge.config_lines()[1]), {"t": "dev", "mute": 0, "bright": 255})
        self.cfg.write_text('[device]\nmute = true\nbrightness = 40\n')
        os.utime(self.cfg, (time.time() + 5, time.time() + 5))
        self.assertEqual(json.loads(self.bridge.config_lines()[1]), {"t": "dev", "mute": 1, "bright": 102})
        self.cfg.write_text('[device]\nbrightness = 0\n')                       # 범위 밖 → 설정 오류
        os.utime(self.cfg, (time.time() + 10, time.time() + 10))
        cmds, dev = self.bridge.config_lines()
        self.assertIsNone(dev)                                                   # 장치는 마지막 값 유지
        self.assertEqual(json.loads(cmds)["l0"], "config error")

    def test_close_removes_session(self):
        self.bridge.on_hook("UserPromptSubmit", ev())
        self.bridge.on_device_line('{"t":"btn","b":0,"press":"short"}')
        self.bridge.on_device_line('{"t":"btn","b":0,"press":"close"}')
        self.assertEqual((self.bridge.table.view(0)[0], self.bridge.watch), (None, -1))
        self.bridge.on_device_line('{"t":"btn","b":2,"press":"close"}')         # 빈 슬롯 — 아무 일 없음
        self.bridge.on_hook("PreToolUse", ev(tool_name="Bash"))                  # 살아 있는 세션이면 다시 생김
        self.assertEqual(self.bridge.table.view(0)[0]["st"], "run")

    def test_alert_sent_on_done(self):
        self.bridge.on_hook("Stop", ev())
        self.assertEqual(json.loads(self.bridge.outbox[-1]), {"t": "alert", "kind": "done", "i": 0})


def sign(auth: DeviceAuth, body: str) -> str:
    """펌웨어 authSignLine 과 같은 방식으로 서명 (본문은 "c" 포함)"""
    import hashlib
    import hmac
    sig = hmac.new(auth.key, (auth.nonce + body).encode(), hashlib.sha256).hexdigest()[:32]
    return body[:-1] + f',"sig":"{sig}"}}'


class TestSecurity(unittest.TestCase):
    def test_key_created_private_and_stable(self):
        a = test_auth("k1")
        kid = a.kid()
        if os.name != "nt":   # Windows 는 st_mode 로 권한을 볼 수 없음
            self.assertEqual(oct(a.key_path.stat().st_mode & 0o777), "0o600")
        self.assertEqual(DeviceAuth(a.key_path).kid(), kid)     # 다시 읽어도 같은 키
        self.assertEqual(json.loads(a.setkey_msg())["key"], a.key.hex())

    def test_verify(self):
        a = test_auth("k2")
        self.assertFalse(a.verify('{"t":"btn","b":0,"press":"long","c":1}'))      # nonce 없음·서명 없음
        a.new_nonce()
        good = sign(a, '{"t":"decide","id":7,"act":"allow","c":1}')
        self.assertTrue(a.verify(good))
        self.assertFalse(a.verify(good))                                           # 같은 카운터 재전송
        self.assertTrue(a.verify(sign(a, '{"t":"btn","b":1,"press":"long","c":5}')))
        self.assertFalse(a.verify(sign(a, '{"t":"btn","b":1,"press":"long","c":3}')))   # 카운터 감소
        tampered = sign(a, '{"t":"decide","id":7,"act":"deny","c":9}').replace("deny", "allow")
        self.assertFalse(a.verify(tampered))
        other = test_auth("k3")                                                    # 다른 키 (가짜 장치)
        other.nonce = a.nonce
        self.assertFalse(a.verify(sign(other, '{"t":"decide","id":7,"act":"allow","c":20}')))
        old = sign(a, '{"t":"decide","id":7,"act":"allow","c":30}')
        a.new_nonce()                                                              # 새 연결
        self.assertFalse(a.verify(old))

    def test_bridge_rejects_unsigned_ble(self):
        link = FakeLink(kind="ble")
        auth = test_auth("k4")
        b = Bridge(lambda *_: link, config_path=NO_CONFIG, auth=auth)
        b.on_connect()
        self.assertEqual(json.loads(b.outbox[0])["t"], "nonce")
        b.on_hook("Stop", ev())
        b.on_device_line('{"t":"btn","b":0,"press":"short","c":1}')
        self.assertEqual(b.watch, -1)                                              # 서명 없음 → 무시
        b.on_device_line(sign(auth, '{"t":"btn","b":0,"press":"short","c":2}'))
        self.assertEqual(b.watch, 0)

    def test_bridge_registers_key_over_usb_only(self):
        for kind, expect in (("usb", True), ("ble", False)):
            link = FakeLink(kind=kind)
            b = Bridge(lambda *_: link, config_path=NO_CONFIG, auth=test_auth("k5"))
            b.on_device_line('{"t":"hello","fw":"0.4.0","kid":""}')
            self.assertEqual(any('"setkey"' in x for x in b.outbox), expect, kind)
        b.outbox.clear()
        b.on_device_line(json.dumps({"t": "hello", "kid": b.auth.kid()}))
        self.assertFalse(any('"setkey"' in x for x in b.outbox))


class TestMainDefault(unittest.TestCase):
    """명령 없이 실행(IDE 실행 버튼 등)하면 tray, 트레이 패키지가 없으면 run 으로"""

    def run_main(self, tray_available):
        import types
        import agent_bridge
        seen = {}
        saved = (sys.argv, agent_bridge.cmd_run, sys.modules.get("tray"))
        sys.argv = ["agent_bridge.py"]
        agent_bridge.cmd_run = lambda a: seen.setdefault("run", a.transport)
        if tray_available:
            fake = types.ModuleType("tray")
            fake.main = lambda a, _start: seen.setdefault("tray", a.transport)
            sys.modules["tray"] = fake
        else:
            sys.modules["tray"] = None          # import 하면 ImportError
        try:
            agent_bridge.main()
        finally:
            sys.argv, agent_bridge.cmd_run = saved[0], saved[1]
            if saved[2] is None:
                sys.modules.pop("tray", None)
            else:
                sys.modules["tray"] = saved[2]
        return seen

    def test_no_command_runs_tray(self):
        self.assertEqual(self.run_main(True), {"tray": "auto"})

    def test_tray_missing_falls_back_to_run(self):
        self.assertEqual(self.run_main(False), {"run": "auto"})


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
