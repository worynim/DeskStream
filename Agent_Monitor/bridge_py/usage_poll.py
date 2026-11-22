# worynim@gmail.com
"""
@file usage_poll.py
@brief Claude 구독 사용량(5시간·주간) 직접 조회 — 설정 [usage] poll = true 일 때만 데몬이 쓴다
@details Claude Code 가 쓰는 비공개 주소 GET /api/oauth/usage 를 Claude Code 의 로그인 토큰으로 호출한다.
         토큰 위치: macOS = 키체인 "Claude Code-credentials", Windows·Linux = ~/.claude/.credentials.json
         (CLAUDE_CONFIG_DIR 가 있으면 그 폴더). 둘 다 claudeAiOauth.accessToken · expiresAt(ms).
         - 공식 API 가 아니라 Claude Code 업데이트로 바뀔 수 있다.
         - 토큰은 읽기만 한다: 갱신·저장·로그 출력하지 않는다. 만료됐으면 조회하지 않는다
           (그동안은 Claude Code 를 쓰지 않아 사용량도 늘지 않고, 다시 쓰면 Claude Code 가 토큰을 갱신한다).
         - 터미널 Claude Code 로 구독 계정에 로그인해야 토큰이 생긴다 (데스크톱 앱 로그인만으로는 없을 수 있음).
"""
import getpass
import json
import os
import ssl
import subprocess
import sys
import urllib.request
from datetime import datetime
from pathlib import Path

USAGE_URL = "https://api.anthropic.com/api/oauth/usage"
KEYCHAIN_SERVICE = "Claude Code-credentials"
TIMEOUT_SEC = 10


def credentials_path() -> Path:
    """Windows·Linux 의 Claude Code 로그인 정보 파일"""
    base = os.environ.get("CLAUDE_CONFIG_DIR")
    return (Path(base) if base else Path.home() / ".claude") / ".credentials.json"


def _keychain_text(run):
    try:
        r = run(["security", "find-generic-password", "-s", KEYCHAIN_SERVICE, "-a", getpass.getuser(), "-w"],
                capture_output=True, text=True, timeout=5)
    except (OSError, subprocess.SubprocessError):
        return None
    return r.stdout if r.returncode == 0 else None


def _file_text(path: Path):
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError):
        return None


def read_token(now: float, run=subprocess.run, platform=None, cred_path=None):
    """Claude Code 로그인 토큰 (macOS 키체인 · 그 밖 ~/.claude/.credentials.json). 없거나 만료됐으면 None."""
    if (platform or sys.platform) == "darwin":
        text = _keychain_text(run)
    else:
        text = _file_text(cred_path or credentials_path())
    if not text:
        return None
    try:
        oauth = json.loads(text).get("claudeAiOauth") or {}
    except (ValueError, AttributeError):
        return None
    token, expires_ms = oauth.get("accessToken"), oauth.get("expiresAt")
    if not isinstance(token, str) or not token:
        return None
    if isinstance(expires_ms, (int, float)) and expires_ms / 1000 <= now:
        return None
    return token


def _ssl_context():
    """python.org 판 Python·실행 파일은 시스템 인증서를 못 찾을 수 있어 certifi 가 있으면 쓴다."""
    try:
        import certifi
        return ssl.create_default_context(cafile=certifi.where())
    except ImportError:
        return ssl.create_default_context()


def fetch(token: str) -> dict:
    """사용량 응답 JSON. 네트워크·HTTP 오류는 OSError(urllib.error.URLError 포함), 형식 오류는 ValueError."""
    req = urllib.request.Request(USAGE_URL, headers={
        "Authorization": f"Bearer {token}", "anthropic-beta": "oauth-2025-04-20"})
    with urllib.request.urlopen(req, timeout=TIMEOUT_SEC, context=_ssl_context()) as resp:
        return json.load(resp)


def parse_usage(data: dict, windows: tuple) -> list:
    """구간별 (사용률 %, 리셋 시각 epoch 초). 값이 없는 구간은 None.
    응답 예: {"five_hour": {"utilization": 42.0, "resets_at": "2026-11-18T00:19:59.52+00:00"}, ...}"""
    out = []
    for name in windows:
        w = data.get(name) if isinstance(data, dict) else None
        pct = w.get("utilization") if isinstance(w, dict) else None
        if isinstance(pct, bool) or not isinstance(pct, (int, float)):
            out.append(None)
            continue
        try:
            reset_at = int(datetime.fromisoformat(w.get("resets_at")).timestamp())
        except (TypeError, ValueError):
            reset_at = 0
        out.append((max(0, min(100, round(pct))), reset_at))
    return out
