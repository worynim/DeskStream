# -*- mode: python ; coding: utf-8 -*-
"""
Stats_Monitor PC 클라이언트 PyInstaller 빌드 스펙
- macOS: 트레이 전용 .app (Dock 아이콘 없음, BLE 권한 문구 포함)
- Windows: 콘솔 창 없는 단일 .exe

빌드: client_py 폴더에서 `pyinstaller stats_monitor.spec`
"""
import sys

APP_NAME = "StatsMonitor"
VERSION = "2.4.2"  # 클라이언트 버전 — 트레이 툴팁(main.py)의 버전과 맞출 것

a = Analysis(
    ["main.py"],
    pathex=[SPECPATH],
    hiddenimports=["bleak"],
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
        bundle_identifier="com.deskstream.statsmonitor",
        version=VERSION,
        icon=f"{SPECPATH}/icons/{APP_NAME}.icns",
        info_plist={
            "CFBundleShortVersionString": VERSION,
            "LSUIElement": True,  # 트레이(메뉴 막대) 전용 — Dock 아이콘 숨김
            "NSBluetoothAlwaysUsageDescription": "DeskStream 기기로 PC 상태 정보를 BLE로 전송합니다.",
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
