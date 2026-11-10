# -*- mode: python ; coding: utf-8 -*-
"""
Agent_Monitor PC 데몬 PyInstaller 빌드 스펙
- 더블클릭(인자 없음) = 트레이 모드. 하위 명령(run·install-hooks 등)은 실행 파일에 인자로 넘기면 그대로 동작
- macOS: 트레이 전용 .app (Dock 아이콘 없음, BLE·자동화 권한 문구 포함)
- Windows: 콘솔 창 없는 단일 .exe

빌드: bridge_py 폴더에서 `pyinstaller agent_monitor.spec`
"""
import sys

APP_NAME = "AgentMonitor"
VERSION = "0.6.3"  # PC 클라이언트 버전 (펌웨어 버전과 별개) — RELEASE_NOTES 와 맞출 것

a = Analysis(
    ["agent_bridge.py"],
    pathex=[SPECPATH],
    datas=[("agent_monitor.example.toml", ".")],  # commands.EXAMPLE_PATH (모듈 옆에서 찾음)
    hiddenimports=["bleak", "tray"],                # tray 는 함수 안에서 import
    excludes=["tkinter", "numpy"],
    noarchive=False,
)
pyz = PYZ(a.pure)

if sys.platform == "darwin":
    exe = EXE(
        pyz,
        a.scripts,
        [],
        exclude_binaries=True,
        name=APP_NAME,
        console=False,
        target_arch="arm64",
    )
    coll = COLLECT(exe, a.binaries, a.datas, name=APP_NAME)
    app = BUNDLE(
        coll,
        name=f"{APP_NAME}.app",
        bundle_identifier="com.deskstream.agentmonitor",
        version=VERSION,
        icon=f"{SPECPATH}/icons/{APP_NAME}.icns",
        info_plist={
            "CFBundleShortVersionString": VERSION,
            "LSUIElement": True,  # 트레이(메뉴 막대) 전용 — Dock 아이콘 숨김
            "NSBluetoothAlwaysUsageDescription": "DeskStream Agent 기기와 BLE로 세션 상태·버튼 신호를 주고받습니다.",
            "NSAppleEventsUsageDescription": "장치 버튼에 지정한 PC 명령(앱 앞으로 가져오기·키 입력)을 실행합니다.",
        },
    )
else:
    exe = EXE(
        pyz,
        a.scripts,
        a.binaries,
        a.datas,
        [],
        name=APP_NAME,
        console=False,
        upx=False,
        icon=f"{SPECPATH}/icons/{APP_NAME}.ico",
    )
