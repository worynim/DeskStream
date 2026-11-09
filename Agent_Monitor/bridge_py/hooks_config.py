# worynim@gmail.com
"""
@file hooks_config.py
@brief Claude Code 훅 설정 생성 · 설치 · 제거
@details - SessionStart 는 http 훅을 실행하지 않으므로(P0 발견 3) curl command 훅으로 보낸다.
         - 나머지는 http 훅, 짧은 timeout → 데몬이 꺼져 있어도 Claude Code 작업에 영향 없음.
         - 설치는 기존 사용자 훅을 보존하고 우리 항목만 추가한다. 쓰기 전 백업 파일을 만든다.
         - 우리 항목 판별: url/command 에 "127.0.0.1:<port>/hooks/" 가 들어 있는지.
"""
import json
import shutil
import time
from pathlib import Path

DEFAULT_SETTINGS = Path.home() / ".claude" / "settings.json"
HTTP_EVENTS = ("SessionEnd", "UserPromptSubmit", "PreToolUse", "PostToolUse", "PostToolUseFailure",
               "PermissionRequest", "Notification", "Stop", "StopFailure", "SubagentStart", "SubagentStop")
HTTP_TIMEOUT_SEC = 5


def marker(port: int) -> str:
    return f"127.0.0.1:{port}/hooks/"


def build_hooks(port: int) -> dict:
    base = f"http://{marker(port)}"
    hooks = {"SessionStart": [{"hooks": [{
        "type": "command",
        "command": f"curl -s -m 2 -o /dev/null -H 'Content-Type: application/json' "
                   f"--data-binary @- {base}SessionStart || true"}]}]}
    for event in HTTP_EVENTS:
        hooks[event] = [{"hooks": [{"type": "http", "url": base + event, "timeout": HTTP_TIMEOUT_SEC}]}]
    return hooks


def _is_ours(hook: dict, port: int) -> bool:
    m = marker(port)
    return m in str(hook.get("url", "")) or m in str(hook.get("command", ""))


def _strip_ours(settings: dict, port: int) -> int:
    """settings 의 hooks 에서 우리 항목을 지우고 지운 개수를 돌려준다. 비게 된 그룹·이벤트도 정리."""
    removed = 0
    hooks = settings.get("hooks", {})
    for event in list(hooks):
        groups = []
        for group in hooks[event]:
            kept = [h for h in group.get("hooks", []) if not _is_ours(h, port)]
            removed += len(group.get("hooks", [])) - len(kept)
            if kept:
                groups.append({**group, "hooks": kept})
        if groups:
            hooks[event] = groups
        else:
            del hooks[event]
    if not hooks:
        settings.pop("hooks", None)
    return removed


def _load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8")) if path.exists() else {}


def _save(path: Path, settings: dict) -> Path | None:
    backup = None
    if path.exists():
        backup = path.with_name(f"{path.name}.bak-{time.strftime('%Y%m%d-%H%M%S')}")
        shutil.copy2(path, backup)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(settings, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return backup


def install(path: Path, port: int) -> Path | None:
    settings = _load(path)
    _strip_ours(settings, port)             # 다시 설치해도 중복되지 않게
    hooks = settings.setdefault("hooks", {})
    for event, groups in build_hooks(port).items():
        hooks.setdefault(event, []).extend(groups)
    return _save(path, settings)


def uninstall(path: Path, port: int) -> tuple:
    settings = _load(path)
    removed = _strip_ours(settings, port)
    backup = _save(path, settings) if removed else None
    return removed, backup
