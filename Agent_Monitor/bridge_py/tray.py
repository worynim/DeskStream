# worynim@gmail.com
"""
@file tray.py
@brief 메뉴 막대(트레이) 아이콘으로 데몬 실행 — `python3 agent_bridge.py tray`
@details pystray + Pillow (Stats_Monitor 클라이언트와 같은 방식). 데몬(훅 서버·장치 연결)은 백그라운드 스레드,
         아이콘은 메인 스레드(macOS 요구사항)에서 돈다.
         아이콘 색: 회색 = 장치 없음 · 초록 = USB · 파랑 = BLE · 주황 = 승인 대기 중
         메뉴: 상태(연결·세션 수·승인 대기) · 설정 파일 열기 · 종료
"""
import os
import subprocess
import sys
import threading
import time

import pystray
from PIL import Image, ImageDraw

import commands

COLORS = {None: (140, 140, 140), "usb": (40, 170, 80), "ble": (40, 110, 220), "approve": (240, 140, 20)}
REFRESH_SEC = 2


def make_icon(color) -> Image.Image:
    img = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for k in range(4):                     # OLED 4장을 닮은 막대 4개
        d.rectangle((4 + k * 15, 18, 15 + k * 15, 46), fill=color)
    return img


def open_file(path):
    """설정 파일을 기본 편집기로 연다 (없으면 예시로 만든다)."""
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(commands.EXAMPLE_PATH.read_text(encoding="utf-8"), encoding="utf-8")
        path.chmod(0o600)
    if sys.platform == "darwin":
        subprocess.run(["open", "-t", str(path)])
    elif os.name == "nt":
        os.startfile(str(path))            # noqa: S606 — 사용자 자신의 설정 파일
    else:
        subprocess.run(["xdg-open", str(path)])


def status_text(bridge) -> str:
    kind = bridge.link.kind
    with bridge.lock:
        n = sum(1 for s in bridge.table.slots if s)
        q = len(bridge.approvals.items)
    link = {"usb": "USB 연결", "ble": "BLE 연결", None: "장치 없음"}[kind]
    return f"{link} · 세션 {n}" + (f" · 승인 대기 {q}" if q else "")


def main(args, start_daemon):
    bridge, server = start_daemon(args)
    threading.Thread(target=server.serve_forever, daemon=True).start()

    def state_color():
        return COLORS["approve"] if bridge.approvals.items else COLORS[bridge.link.kind]

    icon = pystray.Icon("agent_monitor", make_icon(state_color()), "Agent Monitor", menu=pystray.Menu(
        pystray.MenuItem(lambda _item: status_text(bridge), None, enabled=False),
        pystray.MenuItem("설정 파일 열기", lambda: open_file(args.config)),
        pystray.MenuItem("종료", lambda: icon.stop()),
    ))

    def refresh():
        last = None
        while True:
            color = state_color()
            if color != last:
                icon.icon = make_icon(color)
                last = color
            icon.title = f"Agent Monitor — {status_text(bridge)}"
            icon.update_menu()
            time.sleep(REFRESH_SEC)

    threading.Thread(target=refresh, daemon=True).start()
    icon.run()
    server.shutdown()
