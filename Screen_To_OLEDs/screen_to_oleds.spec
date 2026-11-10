# -*- mode: python ; coding: utf-8 -*-
"""
Screen_To_OLEDs 화면 스트리머 PyInstaller 빌드 스펙
- tkinter 창 앱. 설정 파일은 ~/.deskstream/screen_to_oleds.json (screen_streamer._load_user_config)
- macOS: .app (로컬 네트워크 권한 문구 포함, 화면 기록 권한은 첫 캡처 때 시스템이 요청)
- Windows: 콘솔 창 없는 단일 .exe

빌드: Screen_To_OLEDs 폴더에서 `pyinstaller screen_to_oleds.spec`
"""
import sys

APP_NAME = "ScreenToOLEDs"
VERSION = "2.3.3"  # PC 클라이언트 버전 (펌웨어 버전과 별개) — RELEASE_NOTES 와 맞출 것

a = Analysis(
    ["screen_streamer.py"],
    pathex=[SPECPATH],
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
        bundle_identifier="com.deskstream.screentooleds",
        version=VERSION,
        icon=f"{SPECPATH}/icons/{APP_NAME}.icns",
        info_plist={
            "CFBundleShortVersionString": VERSION,
            "NSHighResolutionCapable": True,
            "NSLocalNetworkUsageDescription": "같은 네트워크의 DeskStream 기기를 찾고 화면을 UDP로 전송합니다.",
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
