# worynim@gmail.com
"""
@file screen_streamer.py
@brief PC 화면 캡처 및 ESP32 전송용 Python 클라이언트
@details 실시간 화면 캡처, 각종 디더링 알고리즘(Atkinson, Floyd-Steinberg 등) 적용 및 UDP 데이터 전송 구현
"""

import io
import gc
import json
import logging
import socket
import time
import threading
import queue
import subprocess
import os
import hashlib
from pathlib import Path
import tkinter as tk
from tkinter import ttk
from PIL import Image, ImageEnhance, ImageTk, ImageFilter
import numpy as np
import mss
from typing import Optional, Tuple, List, Dict, Any

# [Step 5] 이미지 처리 로직을 image_processor.py로 분리
from image_processor import apply_threshold, apply_dithering, pack_bits, HAS_NUMBA
from overlay_manager import OverlayManager
from network_manager import NetworkManager



# =============================================
# [설정 상수]
# =============================================
CONFIG = {
    'UDP_PORT':               12345,
    'DISCOVERY_PORT':         12346,
    'DISCOVERY_MSG':          b'ESP32_OLED_OFFER',  # 펌웨어와 반드시 일치
    'WIDTH':                  512,           # 가상 캔버스 가로 (4 OLED × 128)
    'HEIGHT':                 64,            # 가상 캔버스 세로
    'CHUNK_SIZE':             1024,          # 청크 페이로드 크기 (bytes)
    'FRAME_BYTES':            4096,          # 512×64 / 8 = 4096
    'MSS_RESTART_INTERVAL':   3000,          # mss 세션 재시작 주기 (프레임, GC spike 방지용으로 늘림)
    'DEFAULT_IP':             '192.168.10.34',
    'REPLAYD_MEM_LIMIT_MB':   500,
}


def _load_user_config(defaults: dict) -> dict:
    """
    config.json 존재 시 기본값(defaults)을 사용자 설정으로 덮어씀.
    파일 없거나 파싱 실패 시 defaults 그대로 반환.

    Parameters:
        defaults: 기본 설정 딕셔너리 (CONFIG)

    Returns:
        최종 설정 딕셔너리
    """
    config_path = Path(__file__).parent / "config.json"
    if not config_path.exists():
        return defaults
    try:
        with open(config_path, encoding='utf-8') as f:
            overrides = json.load(f)
        cfg = dict(defaults)
        cfg.update({k: v for k, v in overrides.items() if k in defaults})
        print(f"[Config] config.json 반영: {list(overrides.keys())}")
        return cfg
    except Exception as e:
        print(f"[Config] config.json 로드 실패: {e} — 기본값 사용")
        return defaults

CONFIG = _load_user_config(CONFIG)

# =============================================
# [로깅 설정]
# =============================================
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S'
)
logger = logging.getLogger("ScreenStreamer")


# =============================================
# [커스텀 버튼 위젯]
# =============================================
class CustomButton(tk.Label):
    """호버/클릭 효과가 있는 커스텀 버튼"""

    def __init__(self, master, text: str, command=None,
                 bg: str = "#555555", fg: str = "white",
                 font=("Arial", 14, "bold"), **kwargs):
        self.command     = command
        self.default_bg  = bg
        self.default_fg  = fg
        self.is_disabled = False
        state = kwargs.pop('state', tk.NORMAL)
        super().__init__(master, text=text, bg=bg, fg=fg, font=font,
                         relief="solid", padx=10, pady=5,
                         borderwidth=1, cursor="arrow", **kwargs)
        self.bind("<Button-1>", self._on_click)
        self.bind("<Enter>",    self._on_enter)
        self.bind("<Leave>",    self._on_leave)
        self.config(state=state)

    def _on_click(self, event: Any) -> None:
        if not self.is_disabled and self.command:
            self.config(relief="sunken")
            self.after(100, lambda: self.config(relief="solid"))
            self.command()

    def _on_enter(self, event: Any) -> None:
        if not self.is_disabled:
            self.config(bg="#777777")

    def _on_leave(self, event: Any) -> None:
        if not self.is_disabled:
            self.config(bg=self.default_bg)

    def config(self, **kwargs) -> None:  # type: ignore[override]
        if 'state' in kwargs:
            state = kwargs.pop('state')
            if state in (tk.DISABLED, "disabled"):
                self.is_disabled = True
                super().config(bg="#444444", fg="#888888", cursor="arrow")
            elif state in (tk.NORMAL, "normal"):
                self.is_disabled = False
                super().config(bg=self.default_bg, fg=self.default_fg, cursor="arrow")
        if kwargs:
            super().config(**kwargs)




# =============================================
# [메인 애플리케이션]
# =============================================
class VibeStreamerApp:
    """PC 화면을 512×64 흑백으로 변환하여 DeskStream OLED에 UDP 전송"""

    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        _jit_status = "🚀 Numba JIT" if HAS_NUMBA else "⚠️ Python Mode (느림 — pip install numba 권장)"
        self.root.title(f"Screen To OLEDs — DeskStream Screen Streamer  [{_jit_status}]")
        self.root.geometry("520x920")
        self.root.configure(bg="#2b2b2b")

        self.streaming_active: bool = False
        self.target_ip_str: str     = CONFIG['DEFAULT_IP']
        self.monitors: list         = []
        self.manage_replayd_var     = tk.BooleanVar(value=False)

        # ── 프로듀서 스레드 안전 캐시 ──
        # Tkinter IntVar/StringVar.get()을 백그라운드 스레드에서 호출하면
        # 내부 Tcl 뮤텍스 대기로 50~200ms 블로킹 발생 → 여기에 캐시
        # 메인 스레드에서 50ms마다 _refresh_settings_cache()가 동기화
        self._c_bw_mode:   str   = "threshold"
        self._c_threshold: int   = 128
        self._c_contrast:  float = 2.0
        self._c_layout:    str   = "Landscape (512x64)"
        self._c_preview:   bool  = True
        self._c_fps:       int   = 30
        self._c_replayd:   bool  = False
        self._c_skip_static: bool = True

        self.net_queue   = queue.Queue(maxsize=3)
        self.frame_id: int = 0
        self._send_error_count: int = 0  # [Step 3] 연속 UDP 전송 실패 카운터

        # 캡처 영역 오버레이: 4개 선 윈도우 + 핸들 방식 (투명 창 캡처 버그 없음)
        self.overlays: list = []          # [Top, Bottom, Left, Right, Handle]
        self.capture_area: Optional[dict] = None  # 현재 캡처 영역 dict (드래그 시 실시간 갱신)
        self.overlay_ox: int = 0          # 오버레이 origin x
        self.overlay_oy: int = 0          # 오버레이 origin y
        self.is_dragging: bool = False    # 드래그 중 캡처 일시 정지 플래그
        self.drag_start_x: int = 0
        self.drag_start_y: int = 0
        self.win_start_x: int = 0
        self.win_start_y: int = 0

        self._check_os_for_reset()
        self._setup_ui()
        self._get_monitors()
        # 매니저 초기화 (Step 6)
        self.overlay_mgr = OverlayManager(self.root, self)
        self.net_mgr     = NetworkManager(self, CONFIG)
        self.root.after(500, self.scan_ip)
        self.root.after(100, self.overlay_mgr.show_overlay)  # 시작 시 오버레이 표시
        self.root.after(200, self._refresh_settings_cache)   # 설정 캐시 동기화 시작

    # ──────────────────────────────────────────
    # 설정값 캐시 동기화 (메인 스레드 전용)
    # ──────────────────────────────────────────
    def _refresh_settings_cache(self) -> None:
        """
        Tkinter 변수 → 순수 Python 캐시 변수 동기화.
        메인 스레드에서만 호출 (root.after 스케줄).
        프로듀서 스레드는 self._c_* 캐시만 읽어 Tcl 뮤텍스 블로킹 방지.
        """
        self._c_bw_mode   = self.bw_mode_var.get()
        self._c_threshold = self.threshold_var.get()
        self._c_contrast  = self.contrast_var.get()
        self._c_layout    = self.layout_var.get()
        self._c_preview   = self.preview_on.get()
        self._c_fps       = self.fps_var.get()
        self._c_replayd   = self.manage_replayd_var.get()
        self._c_skip_static= self.skip_static_var.get()
        self.root.after(50, self._refresh_settings_cache)  # 50ms마다 반복

    # ──────────────────────────────────────────
    # macOS replayd 메모리 관리
    # ──────────────────────────────────────────
    def _check_os_for_reset(self) -> None:
        if os.name == 'posix':
            self._manage_replayd_memory(force=True)

    def _manage_replayd_memory(self, force: bool = False) -> None:
        if os.name != 'posix':
            return
        if not self.manage_replayd_var.get() and not force:
            return
        try:
            cmd    = "ps -ax -o rss,comm | grep '[r]eplayd'"
            output = subprocess.check_output(cmd, shell=True).decode()
            for line in output.strip().split('\n'):
                parts = line.split()
                if len(parts) >= 2:
                    mem_mb = int(parts[0]) / 1024
                    if force or mem_mb > CONFIG['REPLAYD_MEM_LIMIT_MB']:
                        subprocess.run(["killall", "-9", "replayd"], capture_output=True)
                        time.sleep(0.5)
                        break
        except Exception as e:
            logger.debug(f"replayd 메모리 관리 중 오류: {e}")

    # ──────────────────────────────────────────
    # UI 구성
    # ──────────────────────────────────────────
    def _setup_ui(self) -> None:
        style = {"bg": "#2b2b2b", "fg": "white"}

        # ── 설정 영역 ──
        ip_frame = tk.LabelFrame(self.root, text="Settings", padx=10, pady=8,
                                 bg="#3c3f41", fg="white")
        ip_frame.pack(fill="x", padx=10, pady=5)

        # 전송 모드 선택 (단일/전체)
        self.target_mode_var = tk.StringVar(value="direct")
        self.last_unicast_ip = CONFIG['DEFAULT_IP']
        
        mode_row = tk.Frame(ip_frame, bg="#3c3f41")
        mode_row.pack(fill="x", pady=(0, 5))
        
        self.target_radios = []
        for text, val in [("Direct (Single)", "direct"), ("Broadcast (Multi)", "broadcast")]:
            rb = tk.Radiobutton(mode_row, text=text, variable=self.target_mode_var,
                                value=val, command=self._on_target_mode_change,
                                bg="#3c3f41", fg="white", selectcolor="#555555")
            rb.pack(side=tk.LEFT, padx=5)
            self.target_radios.append(rb)

        ip_row = tk.Frame(ip_frame, bg="#3c3f41")
        ip_row.pack(fill="x")
        tk.Label(ip_row, text="IP:", **style).pack(side=tk.LEFT)
        self.ip_entry = tk.Entry(ip_row, width=16)
        
        # 초기 전송 모드(Direct) 및 IP 설정
        initial_ip = CONFIG['DEFAULT_IP']
        if self.target_mode_var.get() == "broadcast" and "." in initial_ip:
            parts = initial_ip.split(".")
            if len(parts) == 4:
                parts[3] = "255"
                initial_ip = ".".join(parts)
        
        self.ip_entry.insert(0, initial_ip)
        self.ip_entry.pack(side=tk.LEFT, padx=5)

        self.btn_scan = CustomButton(ip_row, text="Scan", command=self.scan_ip, bg="#555555")
        self.btn_scan.pack(side=tk.LEFT)
        self.scan_status_var = tk.StringVar(value="Ready")
        tk.Label(ip_row, textvariable=self.scan_status_var,
                 fg="#aaffaa", font=("Arial", 12, "bold"), bg="#3c3f41").pack(side=tk.LEFT, padx=8)

        # ── 소스 설정 영역 ──
        screen_frame = tk.LabelFrame(self.root, text="Source Config", padx=10, pady=8,
                                     bg="#3c3f41", fg="white")
        screen_frame.pack(fill="x", padx=10, pady=5)

        # 모니터 선택 UI 제거됨 (좌표 표시 기능을 통한 수동 배치 유도)

        # OLED 레이아웃
        layout_row = tk.Frame(screen_frame, bg="#3c3f41")
        layout_row.pack(fill="x", pady=2)
        tk.Label(layout_row, text="OLED Layout:", bg="#3c3f41", fg="white").pack(side=tk.LEFT, padx=(0, 5))
        self.layout_var = tk.StringVar(value="Landscape (512x64)")
        self.layout_radios: list = []
        for val in ["Landscape (512x64)", "Portrait (64x512)"]:
            rb = tk.Radiobutton(layout_row, text=val, variable=self.layout_var,
                                value=val, command=self._update_overlay,
                                bg="#3c3f41", fg="white", selectcolor="#555555")
            rb.pack(side=tk.LEFT, padx=5)
            self.layout_radios.append(rb)

        # 캡처 영역 크기
        cap_row = tk.Frame(screen_frame, bg="#3c3f41")
        cap_row.pack(fill="x", pady=2)
        tk.Label(cap_row, text="Capture Width:", bg="#3c3f41", fg="white").pack(side=tk.LEFT, padx=(0, 5))
        self.cap_size_var    = tk.StringVar(value="512")
        self.cap_radios: list = []
        for val in ["512", "768", "1024", "1440", "Full"]:
            rb = tk.Radiobutton(cap_row, text=val, variable=self.cap_size_var,
                                value=val, command=self._update_overlay,
                                bg="#3c3f41", fg="white", selectcolor="#555555")
            rb.pack(side=tk.LEFT)
            self.cap_radios.append(rb)

        # Center Crop UI 제거됨 (사용자 요청)

        # 캡처 영역 오버레이 토글
        self.show_overlay_var = tk.BooleanVar(value=True)
        self.chk_overlay = tk.Checkbutton(screen_frame, text="Show Capture Area Overlay",
                                          variable=self.show_overlay_var,
                                          bg="#3c3f41", fg="white", selectcolor="#555555",
                                          command=self._update_overlay)
        self.chk_overlay.pack(anchor="w")

        # ── 이진화 설정 영역 ──
        bw_frame = tk.LabelFrame(self.root, text="B&W Conversion", padx=10, pady=8,
                                 bg="#3c3f41", fg="white")
        bw_frame.pack(fill="x", padx=10, pady=5)

        self.bw_mode_var = tk.StringVar(value="Atkinson")
        mode_grid = tk.Frame(bw_frame, bg="#3c3f41")
        mode_grid.pack(fill="x", pady=2)
        
        self.bw_radios: list = []
        modes = [
            ("Threshold (Fast)", "threshold"),
            ("Ordered (Bayer)", "Ordered (Bayer)"),
            ("Floyd-Steinberg", "Floyd-Steinberg"),
            ("Atkinson", "Atkinson"),
            ("Stucki", "Stucki"),
            ("Sierra", "Sierra")
        ]
        for i, (text, val) in enumerate(modes):
            rb = tk.Radiobutton(mode_grid, text=text, variable=self.bw_mode_var,
                                value=val, command=self._update_bw_ui,
                                bg="#3c3f41", fg="white", selectcolor="#555555",
                                font=("Arial", 12))
            rb.grid(row=i//2, column=i%2, sticky="w", padx=10, pady=2)
            self.bw_radios.append(rb)

        # Threshold 슬라이더
        self.threshold_var = tk.IntVar(value=128)
        self.threshold_frame = tk.Frame(bw_frame, bg="#3c3f41")
        self.threshold_frame.pack(fill="x", pady=2)
        tk.Label(self.threshold_frame, text="Threshold:", bg="#3c3f41", fg="white").pack(side=tk.LEFT)
        self.threshold_scale = tk.Scale(self.threshold_frame, from_=0, to=255,
                                        orient=tk.HORIZONTAL, variable=self.threshold_var,
                                        bg="#3c3f41", fg="white", troughcolor="#555555",
                                        length=300)
        self.threshold_scale.pack(side=tk.LEFT, fill="x", expand=True)

        # Contrast 슬라이더 (디더링 전용)
        self.contrast_var = tk.DoubleVar(value=2.0)
        self.contrast_frame = tk.Frame(bw_frame, bg="#3c3f41")
        self.contrast_frame.pack(fill="x", pady=2)
        tk.Label(self.contrast_frame, text="Contrast:", bg="#3c3f41", fg="white").pack(side=tk.LEFT)
        self.contrast_scale = tk.Scale(self.contrast_frame, from_=0.5, to=3.0,
                                       resolution=0.1, orient=tk.HORIZONTAL,
                                       variable=self.contrast_var,
                                       bg="#3c3f41", fg="white", troughcolor="#555555",
                                       length=300)
        self.contrast_scale.pack(side=tk.LEFT, fill="x", expand=True)
        # 초기에는 Threshold 모드이므로 Contrast 프레임 숨김
        self.contrast_frame.pack_forget()

        # ── 성능 설정 영역 ──
        perf_frame = tk.LabelFrame(self.root, text="Performance", padx=10, pady=8,
                                   bg="#3c3f41", fg="white")
        perf_frame.pack(fill="x", padx=10, pady=5)

        self.fps_var = tk.IntVar(value=30)
        tk.Scale(perf_frame, from_=5, to=60, orient=tk.HORIZONTAL,
                 variable=self.fps_var, label="Target FPS",
                 bg="#3c3f41", fg="white", troughcolor="#555555").pack(fill="x")

        self.preview_on = tk.BooleanVar(value=True)
        self.chk_preview = tk.Checkbutton(perf_frame, text="Live Preview",
                                          variable=self.preview_on,
                                          bg="#3c3f41", fg="white", selectcolor="#555555")
        self.chk_preview.pack(anchor="w")

        self.chk_replayd = tk.Checkbutton(perf_frame, text="Manage Replayd (Mac)",
                                          variable=self.manage_replayd_var,
                                          bg="#3c3f41", fg="white", selectcolor="#555555")
        self.chk_replayd.pack(anchor="w")

        self.skip_static_var = tk.BooleanVar(value=True)
        self.chk_skip_static = tk.Checkbutton(perf_frame, text="Skip Static Frames",
                                              variable=self.skip_static_var,
                                              bg="#3c3f41", fg="white", selectcolor="#555555")
        self.chk_skip_static.pack(anchor="w")

        # ── 시작/정지 버튼 ──
        btn_frame = tk.Frame(self.root, pady=10, bg="#2b2b2b")
        btn_frame.pack(fill="x", padx=10)
        self.btn_start = CustomButton(btn_frame, text="▶  START STREAMING",
                                      bg="#007acc", command=self.start_streaming)
        self.btn_start.pack(side=tk.LEFT, fill="x", expand=True, padx=5)
        self.btn_stop  = CustomButton(btn_frame, text="■  STOP",
                                      bg="#c0392b", command=self.stop_streaming,
                                      state=tk.DISABLED)
        self.btn_stop.pack(side=tk.LEFT, fill="x", expand=True, padx=5)

        # ── 상태 표시 ──
        self.stats_frame = tk.Frame(self.root, bg="black", pady=8)
        self.stats_frame.pack(fill="x", padx=10, pady=5)
        self.lbl_stats = tk.Label(self.stats_frame, text="Stopped",
                                  font=("Courier", 14, "bold"), fg="#aaffaa", bg="black")
        self.lbl_stats.pack()

        # ── 프리뷰 (512:64 비율 유지) ──
        self.lbl_preview = tk.Label(self.root, bg="black")
        self.lbl_preview.pack(fill="both", expand=True, padx=10, pady=5)

        # 초기 UI 상태 동기화 (Atkinson 선택에 따른 Contrast 슬라이더 표시)
        self._update_bw_ui()

    def _update_bw_ui(self) -> None:
        """이진화 모드에 따라 슬라이더 표시/숨김"""
        if self.bw_mode_var.get() == "threshold":
            self.contrast_frame.pack_forget()
            self.threshold_frame.pack(fill="x", pady=2)
        else:
            self.threshold_frame.pack_forget()
            self.contrast_frame.pack(fill="x", pady=2)
            # 디더링 계열 모드 진입 시 가독성을 위해 Contrast를 2.0으로 기본 설정
            self.contrast_var.set(2.0)

    def _update_overlay(self, event: Any = None) -> None:
        """오버레이 체크박스 상태에 따라 표시/숨김"""
        if self.show_overlay_var.get():
            self.overlay_mgr.show_overlay()
        else:
            self.overlay_mgr.hide_overlay()

    # _get_selected_monitor_index() 제거됨 (UI 제거에 따라 Index 1 기본 사용)

    def _calc_capture_area(self, mon: dict) -> Tuple[int, int, int, int]:
        """
        캡처 영역 좌표 반환. (핸들 표시를 위한 안전 여백 포함)
        """
        is_portrait = getattr(self, 'layout_var', None) and "Portrait" in self.layout_var.get()
        cap_str = self.cap_size_var.get()

        # 핸들 높이(24px)와 패딩을 고려한 안전 마진
        SAFE_W = 10
        SAFE_H = 40

        if cap_str == "Full":
            if is_portrait:
                cap_h = mon['height'] - SAFE_H
                cap_w = cap_h // 8
                if cap_w > mon['width'] - SAFE_W:
                    cap_w = mon['width'] - SAFE_W
                    cap_h = cap_w * 8
            else:
                cap_w = mon['width'] - SAFE_W
                cap_h = cap_w // 8
                if cap_h > mon['height'] - SAFE_H:
                    cap_h = mon['height'] - SAFE_H
                    cap_w = cap_h * 8
            
            left = mon['left'] + (mon['width'] - cap_w) // 2
            # 핸들이 모니터 상단을 벗어나지 않도록 top 좌표 조정
            top  = mon['top'] + (mon['height'] - cap_h) // 2
            if top < mon['top'] + SAFE_H:
                top = mon['top'] + SAFE_H
            return left, top, cap_w, cap_h
        else:
            max_dim = int(cap_str)
            if is_portrait:
                cap_h = min(max_dim, mon['height'] - SAFE_H)
                cap_w = cap_h // 8
            else:
                cap_w = min(max_dim, mon['width'] - SAFE_W)
                cap_h = cap_w // 8
            
            # 최종 크기 재검증
            if cap_w > mon['width'] - SAFE_W:
                cap_w = mon['width'] - SAFE_W
                cap_h = cap_w // 8
            if cap_h > mon['height'] - SAFE_H:
                cap_h = mon['height'] - SAFE_H
                cap_w = cap_h * 8

            left = mon['left'] + (mon['width']  - cap_w) // 2
            top  = mon['top']  + (mon['height'] - cap_h) // 2
            # 핸들 가시성 확보를 위한 최소 Top 좌표 보정
            if top < mon['top'] + SAFE_H:
                top = mon['top'] + SAFE_H
                
            return int(left), int(top), int(cap_w), int(cap_h)

    # ──────────────────────────────────────────
    # 모니터 목록
    # ──────────────────────────────────────────
    def _get_monitors(self) -> None:
        with mss.mss() as sct:
            self.monitors = sct.monitors

    # ──────────────────────────────────────────
    # IP 자동 검색
    # ──────────────────────────────────────────
    def scan_ip(self) -> None:
        """IP 자동 검색 — 비동기 실행으로 UI 블로킹 없음"""
        self.scan_status_var.set("Scanning...")
        self.btn_scan.config(state="disabled")
        self.net_mgr.scan_ip_async(self._on_scan_done)


    def _on_scan_done(self, found: Optional[str]) -> None:
        """IP 검색 완료 후 메인 스레드에서 UI 업데이트"""
        self.btn_scan.config(state="normal")
        if found:
            self.last_unicast_ip = found
            if self.target_mode_var.get() == "direct":
                self.ip_entry.delete(0, tk.END)
                self.ip_entry.insert(0, found)
            self.scan_status_var.set(f"Found: {found}")
            self.target_ip_str = found
        else:
            self.scan_status_var.set("Not found")

    def _on_target_mode_change(self) -> None:
        """모드 변경 시 IP 주소 자동 보정"""
        current_ip = self.ip_entry.get()
        if self.target_mode_var.get() == "broadcast":
            # 현재 IP 기반으로 .255 생성
            if current_ip and "." in current_ip:
                parts = current_ip.split(".")
                if len(parts) == 4:
                    if parts[3] != "255":
                        self.last_unicast_ip = current_ip
                    parts[3] = "255"
                    bcast_ip = ".".join(parts)
                    self.ip_entry.delete(0, tk.END)
                    self.ip_entry.insert(0, bcast_ip)
        else:
            # 이전 유니캐스트 IP 복구
            self.ip_entry.delete(0, tk.END)
            self.ip_entry.insert(0, self.last_unicast_ip)


    # ──────────────────────────────────────────
    # 스트리밍 시작/정지
    # ──────────────────────────────────────────
    def start_streaming(self) -> None:
        self.target_ip_str = self.ip_entry.get()
        if not self.streaming_active:
            self.streaming_active = True
            # 스트리밍 중 고정: IP / 실시간 허용: B&W 모드, Contrast, FPS, Preview, Layout, Width, Overlay
            all_widgets = ([self.ip_entry, self.btn_scan, self.chk_replayd, self.chk_skip_static]
                           + self.target_radios)
            for w in all_widgets:
                w.config(state="disabled")

            # 스트리밍 시작 시 오버레이가 꺼져있었다면 내부적으로라도 위치 데이터를 확보해야 함
            if not self.capture_area:
                with mss.mss() as sct:
                    mon_idx = 1 if len(sct.monitors) > 1 else 0
                    mon = sct.monitors[mon_idx]
                    left, top, cap_w, cap_h = self._calc_capture_area(mon)
                self.capture_area = {
                    'left': left, 'top': top,
                    'width': cap_w, 'height': cap_h,
                    'mon': mon_idx
                }

            # 오버레이 표시 여부와 상관없이 위치를 동기화하기 위해 호출
            if self.show_overlay_var.get():
                self.overlay_mgr.show_overlay()

            while not self.net_queue.empty():
                self.net_queue.get_nowait()
            threading.Thread(target=self._producer_loop, daemon=True).start()
            threading.Thread(target=self.net_mgr.udp_sender_loop, daemon=True).start()
            self.btn_start.config(state=tk.DISABLED)
            self.btn_stop.config(state=tk.NORMAL)

    def stop_streaming(self) -> None:
        self.streaming_active = False
        # self._hide_overlay()  <-- 제거하여 중지 시에도 오버레이 유지
        all_widgets = ([self.ip_entry, self.btn_scan, self.chk_overlay, self.chk_replayd, self.chk_skip_static]
                       + self.cap_radios + self.layout_radios + self.target_radios)
        for w in all_widgets:
            w.config(state="normal")
        self.btn_start.config(state=tk.NORMAL)
        self.btn_stop.config(state=tk.DISABLED)
        self.lbl_stats.config(text="Stopped")

    # ──────────────────────────────────────────
    # 이미지 처리: 캡처 이미지 → 4096 bytes
    # ──────────────────────────────────────────
    def process_image_from_bgra(self, bgra_buf: bytes, cap_w: int, cap_h: int,
                                mode: str, threshold: int, contrast: float,
                                layout: str, want_preview: bool
                                ) -> Tuple[bytes, Optional[Image.Image], float, float, float, float]:
        """
        BGRA 캡처 바이트를 DeskStream 전송용 4096 bytes로 변환.

        Returns:
            (raw_bytes, preview, cnv_ms, rsz_ms, bin_ms, pak_ms)
        """
        is_portrait = ("Portrait" in layout)
        target_w = 64 if is_portrait else 512
        target_h = 512 if is_portrait else 64

        # 1. BGRA → Grayscale (PIL C 구현 — Retina 고해상도에서 numpy float보다 빠름)
        _t0 = time.perf_counter()
        img_rgb = Image.frombuffer("RGB", (cap_w, cap_h), bgra_buf, "raw", "BGRX")

        # Retina 2x 보정: 물리 픽셀이 목표 해상도의 2배 이상일 때 사전 다운샘플
        # (macOS Retina: mss.grab()이 논리 해상도의 2배 물리 픽셀 반환)
        # → 전처리 비용 크게 감소: cap_w=1024 → 512로 줄이면 처리량 4배 감소
        if cap_w > target_w * 2:
            img_rgb = img_rgb.resize((cap_w // 2, cap_h // 2), Image.BOX)
            cap_w = cap_w // 2
            cap_h = cap_h // 2

        img_gray_full = img_rgb.convert("L")  # PIL 내부 C 코드로 그레이스케일 변환
        cnv_ms = (time.perf_counter() - _t0) * 1000

        # 2. 비율 유지 리사이즈 + 중앙 크롭 (1채널 Grayscale에서 수행 → 3배 빠름)
        _t0 = time.perf_counter()
        src_ratio = cap_w / cap_h
        tgt_ratio = target_w / target_h
        if src_ratio > tgt_ratio:
            new_h = target_h
            new_w = int(new_h * src_ratio)
        else:
            new_w = target_w
            new_h = int(new_w / src_ratio)
        img_gray = img_gray_full.resize((new_w, new_h), Image.LANCZOS)
        left = (new_w - target_w) // 2
        top  = (new_h - target_h) // 2
        img_gray = img_gray.crop((left, top, left + target_w, top + target_h))
        if is_portrait:
            img_gray = img_gray.transpose(Image.Transpose.ROTATE_90)
        rsz_ms = (time.perf_counter() - _t0) * 1000

        # 3. 이진화 (Threshold or Dithering)
        _t0 = time.perf_counter()
        if mode == "threshold":
            bw_arr = apply_threshold(img_gray, threshold)
        else:
            bw_arr = apply_dithering(img_gray, mode, contrast)
        bin_ms = (time.perf_counter() - _t0) * 1000

        # 4. 비트 패킹 → 4096 bytes
        _t0 = time.perf_counter()
        raw_bytes = pack_bits(bw_arr)
        pak_ms = (time.perf_counter() - _t0) * 1000

        # 5. 프리뷰 이미지 생성 (필요할 때만)
        preview = None
        if want_preview:
            preview = Image.fromarray(bw_arr).convert("RGB")
            if is_portrait:
                preview = preview.transpose(Image.Transpose.ROTATE_270)

        return raw_bytes, preview, cnv_ms, rsz_ms, bin_ms, pak_ms

    # ──────────────────────────────────────────
    # 프로듀서 루프 (캡처 스레드)
    # ──────────────────────────────────────────
    def _producer_loop(self) -> None:
        """
        캡처 & 이미지 처리 루프.

        [성능 최적화]
        - GC 비활성화: 핫루프 내 임시 객체에 의한 GC 스파이크 제거
        - BGRA→Gray 직변환: PIL RGB 중간 단계 완전 생략
        - mss 세션 명시적 교체: with 블록 탈출 끊김 제거
        - 프리뷰 10fps 제한: 불필요 시 PIL 객체 생성 안 함
        - 상세 타이밍 표시: Cap(캡처) / Proc(처리) / Total(총) ms
        """
        logger.info("[Producer] 스레드 시작")

        self._last_frame_hash = b""
        self._last_sent_time = time.perf_counter()

        # ── GC 비활성화: 핫루프 내 임시 numpy/PIL 객체에 의한 주기적 GC 스파이크 방지 ──
        gc.disable()

        frames = 0
        acc_cap = 0.0
        max_cap = 0.0   # 1초 내 최대 캡처 시간 추적
        acc_cnv = acc_rsz = acc_bin = acc_pak = 0.0
        last_fps_time     = time.time()
        last_preview_time = time.time()

        try:
            with mss.mss() as sct:
                mon_idx = 1 if len(sct.monitors) > 1 else 0
        except Exception:
            mon_idx = 0

        target_fps = self.fps_var.get()

        # ── mss 세션 한 번만 생성 ──
        sct = mss.mss()
        loop_idx = 0

        # 초기 캡처 영역 계산
        try:
            mon  = sct.monitors[mon_idx]
            left, top, w, h = self._calc_capture_area(mon)
            area: dict = {"top": top, "left": left, "width": w, "height": h, "mon": mon_idx}
        except Exception:
            area = {}

        # ── _manage_replayd_memory 별도 스레드로 분리 (subprocess 호출 연쇄 루프 블로킹 제거) ──
        def _replayd_worker() -> None:
            self._manage_replayd_memory()
        _replayd_thread: Optional[threading.Thread] = None

        # ── Deadline Clock 초기화 (macOS sleep 지터 제거용) ──
        frame_deadline = time.perf_counter()

        try:
            while self.streaming_active:
                # [Step 4 개선] GC 세대별 분리: 짧은 주기(젊은 세대) / 긴 주기(전체 + mss 재시작)
                if loop_idx > 0:
                    if loop_idx % CONFIG['MSS_RESTART_INTERVAL'] == 0:
                        gc.collect()  # 전체 GC
                        try:
                            sct.close()
                        except Exception:
                            pass
                        sct = mss.mss()
                    elif loop_idx % 300 == 0:
                        gc.collect(0)  # 젊은 세대만 수집 (< 1ms)

                # replayd 메모리 관리: 별도 스레드로 실행 (핸루프 블로킹 제거)
                if self._c_replayd and loop_idx % 100 == 0:
                    if _replayd_thread is None or not _replayd_thread.is_alive():
                        _replayd_thread = threading.Thread(
                            target=_replayd_worker, daemon=True)
                        _replayd_thread.start()
                loop_idx += 1

                loop_start = time.perf_counter()

                # 드래그 중에는 캡처 일시 중단 (macOS 드래그 끊김 방지)
                if self.is_dragging:
                    time.sleep(0.01)
                    continue

                # 캡처 영역 결정 (드래그로 이동된 경우 우선 적용)
                if self.capture_area:
                    area = dict(self.capture_area)
                    area.setdefault('mon', mon_idx)

                # ── Phase 1: 화면 캡처 (BGRA raw) ──
                t_cap = time.perf_counter()
                try:
                    sct_img = sct.grab(area)
                except Exception:
                    time.sleep(0.01)
                    continue
                cap_time = time.perf_counter() - t_cap
                acc_cap += cap_time
                if cap_time > max_cap:
                    max_cap = cap_time

                # ── Step 8: 정적 프레임 스킵 (화면 변화 없을 때 처리/전송 생략) ──
                # mss의 .bgra 프로퍼티는 호출 시마다 bytes를 새로 할당하므로 변수에 캐싱합니다.
                bgra_buf = sct_img.bgra
                
                if self._c_skip_static:
                    # Python 내장 hash()는 C레벨 SipHash를 사용하여 전체 배열(수 MB)도 1~3ms 만에
                    # 초고속으로 검사합니다. 100% 모든 픽셀의 변화를 감지할 수 있습니다.
                    frame_hash = hash(bgra_buf)
                    
                    # 1초 지났으면 KeepAlive용으로 프레임을 한 번 강제 전송함
                    now_perf = time.perf_counter()
                    if frame_hash == self._last_frame_hash and (now_perf - self._last_sent_time) < 1.0:
                        # 화면 변화 없음 + 1초 이내 -> 이번 프레임 생략
                        frames += 1
                        frame_deadline += 1.0 / target_fps
                        sleep_until = frame_deadline - time.perf_counter()
                        if sleep_until > 0.002:
                            time.sleep(sleep_until - 0.001)
                        elif sleep_until < -0.05:
                            frame_deadline = time.perf_counter()
                        continue
                        
                    self._last_frame_hash = frame_hash
                    self._last_sent_time = now_perf

                # 설정값 읽기 — 캐시된 Python 변수 사용 (Tkinter .get() 호출 금지)
                # 이유: Tkinter StringVar/IntVar.get()을 백그라운드 스레드에서 호출하면
                #       Tcl 뮤텍스 대기로 50~200ms 블로킹 발생 (FPS 6fps 급락의 원인)
                bw_mode   = self._c_bw_mode
                threshold = self._c_threshold
                contrast  = self._c_contrast
                layout    = self._c_layout
                now       = time.time()
                want_preview = (self._c_preview and
                                (now - last_preview_time) >= 0.1)

                # ── Phase 2~4: 처리 + 전송 (배열 내 예외를 잘라서 프로듀서 전체 충돌 방지) ──
                try:
                    raw_bytes, preview, cnv_ms, rsz_ms, bin_ms, pak_ms = self.process_image_from_bgra(
                        bgra_buf, sct_img.width, sct_img.height,
                        bw_mode, threshold, contrast, layout, want_preview
                    )

                    acc_cnv  += cnv_ms / 1000
                    acc_rsz  += rsz_ms / 1000
                    acc_bin  += bin_ms / 1000
                    acc_pak  += pak_ms / 1000

                    # ── Phase 3: 네트워크 큐에 전달 ──
                    if self.net_queue.full():
                        try:
                            self.net_queue.get_nowait()
                        except Exception:
                            pass
                    self.net_queue.put((self.frame_id, raw_bytes))
                    self.frame_id = (self.frame_id + 1) % 256

                    # ── Phase 4: 프리뷰 갱신 (메인 스레드 위임) ──
                    if want_preview and preview:
                        self.root.after(0, self._update_preview_main_thread, preview)
                        last_preview_time = now

                except Exception as _proc_err:
                    # 프레임 처리 실패시 루프를 죽이지 않고 다음 프레임으로 계속
                    logger.warning(f"[Producer] 프레임 처리 오류: {_proc_err}")

                # ── Deadline Clock FPS 제한 ──
                # sleep(남은시간) 대신 절대 마감 시각 기준으로 대기
                # → 매 프레임 ±10ms sleep 오차가 다음 프레임에서 자동 보정됨
                frame_deadline += 1.0 / target_fps
                sleep_until = frame_deadline - time.perf_counter()
                if sleep_until > 0.002:           # 2ms 이상 남을 때만 sleep
                    time.sleep(sleep_until - 0.001)  # 1ms 마진 남기고 sleep
                elif sleep_until < -0.05:          # 50ms 이상 지연 시 리셋 (드래그 등)
                    frame_deadline = time.perf_counter()

                # ── 통계 갱신 (1초 주기) ──
                frames += 1
                elapsed = time.time() - last_fps_time
                if elapsed >= 1.0:
                    real_fps = frames / elapsed
                    n = frames if frames else 1
                    cap_ms     = acc_cap * 1000 / n
                    cap_max_ms = max_cap * 1000
                    cnv_ms = acc_cnv * 1000 / n
                    rsz_ms = acc_rsz * 1000 / n
                    bin_ms = acc_bin * 1000 / n
                    pak_ms = acc_pak * 1000 / n
                    # Retina 감지: 물리 픽셀이 논리 픽셀의 2배 이상이면 2x 표시
                    log_w = area.get('width', sct_img.width)
                    log_h = area.get('height', sct_img.height)
                    is_retina = sct_img.width >= log_w * 2
                    res_tag = f"{log_w}x{log_h}{'(Retina2x)' if is_retina else ''}"
                    msg = (f"FPS:{real_fps:.0f} "
                           f"| {res_tag} "
                           f"Cap:{cap_ms:.0f}(mx:{cap_max_ms:.0f}) "
                           f"Cnv:{cnv_ms:.0f} "
                           f"Rsz:{rsz_ms:.0f} "
                           f"Bin:{bin_ms:.0f} "
                           f"Pak:{pak_ms:.0f}ms "
                           f"| {bw_mode.upper()}")
                    self.root.after(0, lambda m=msg: self.lbl_stats.config(text=m))
                    frames = 0
                    acc_cap = acc_cnv = acc_rsz = acc_bin = acc_pak = 0.0
                    max_cap = 0.0
                    last_fps_time = time.time()
                    target_fps = self._c_fps

        finally:
            # GC 복원 + mss 세션 해제
            gc.enable()
            gc.collect()
            try:
                sct.close()
            except Exception:
                pass
            logger.info("[Producer] 스레드 종료")

    # ──────────────────────────────────────────
    # 프리뷰 갱신 (메인 스레드에서 호출)
    # ──────────────────────────────────────────
    def _update_preview_main_thread(self, img: Image.Image) -> None:
        try:
            w = self.lbl_preview.winfo_width()
            h = self.lbl_preview.winfo_height()
            if w <= 1 or h <= 1:
                w, h = 512, 64

            # 원본 이미지 비율에 맞춰 스케일 업
            img_w, img_h = img.width, img.height
            scale   = min(w / img_w, h / img_h)
            nw, nh  = int(img_w * scale), int(img_h * scale)
            final   = Image.new("RGB", (w, h), (0, 0, 0))
            resized = img.resize((nw, nh), Image.NEAREST)
            final.paste(resized, ((w - nw) // 2, (h - nh) // 2))
            self.tk_img = ImageTk.PhotoImage(final)
            self.lbl_preview.config(image=self.tk_img)
        except Exception:
            pass


# =============================================
# [엔트리 포인트]
# =============================================
if __name__ == "__main__":
    root = tk.Tk()
    app  = VibeStreamerApp(root)
    root.mainloop()
