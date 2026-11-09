# worynim@gmail.com
"""
@file security.py
@brief BLE 버튼·승인 신호 서명 검증 — 펌웨어 agent_auth 와 짝
@details 위협: 같은 이름으로 광고하는 가짜 장치에 데몬이 연결해 "승인"·"PC 명령" 신호를 받는 것.
         - 키(32바이트)는 이 데몬이 만들어 ~/.deskstream/device_key (권한 600) 에 두고, **USB 로만** 장치에 보낸다.
         - BLE 연결마다 새 nonce 를 보내고, 장치가 보낸 줄의 "sig" = HMAC-SHA256(키, nonce + 본문) 앞 16바이트,
           "c"(카운터) 는 연결 안에서 계속 커져야 한다 → 가짜 장치·재전송 차단.
         - kid = HMAC(키, "kid") 앞 4바이트 hex — 장치 hello 의 kid 와 비교해 키 등록 여부를 안다.
"""
import hashlib
import hmac
import json
import os
import re
import secrets
from pathlib import Path

KEY_PATH = Path.home() / ".deskstream" / "device_key"
SIG_RE = re.compile(r',"sig":"([0-9a-f]{32})"\}\s*$')


class DeviceAuth:
    def __init__(self, key_path: Path = KEY_PATH):
        self.key_path = key_path
        self._key = None
        self.nonce = ""
        self.last_counter = 0

    @property
    def key(self) -> bytes:
        """키를 읽고, 없으면 만들어 저장한다."""
        if self._key is None:
            if self.key_path.exists():
                self._key = bytes.fromhex(self.key_path.read_text().strip())
            else:
                self._key = secrets.token_bytes(32)
                self.key_path.parent.mkdir(parents=True, exist_ok=True)
                fd = os.open(self.key_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
                with os.fdopen(fd, "w") as f:
                    f.write(self._key.hex())
        return self._key

    def kid(self) -> str:
        return hmac.new(self.key, b"kid", hashlib.sha256).hexdigest()[:8]

    def setkey_msg(self) -> str:
        """USB 로만 보내는 키 등록 메시지."""
        return json.dumps({"t": "setkey", "key": self.key.hex()}, separators=(",", ":"))

    def new_nonce(self) -> str:
        """BLE 연결마다 — 새 nonce 를 정하고 그 메시지를 돌려준다."""
        self.nonce = secrets.token_hex(8)
        self.last_counter = 0
        return json.dumps({"t": "nonce", "n": self.nonce}, separators=(",", ":"))

    def verify(self, line: str) -> bool:
        """BLE 로 온 줄의 서명·카운터 확인. 맞으면 카운터를 갱신한다."""
        line = line.strip()
        m = SIG_RE.search(line)
        if not m or not self.nonce:
            return False
        body = line[:m.start()] + "}"
        expected = hmac.new(self.key, (self.nonce + body).encode(), hashlib.sha256).hexdigest()[:32]
        if not hmac.compare_digest(expected, m.group(1)):
            return False
        try:
            c = json.loads(body).get("c")
        except json.JSONDecodeError:
            return False
        if not isinstance(c, int) or c <= self.last_counter:
            return False
        self.last_counter = c
        return True
