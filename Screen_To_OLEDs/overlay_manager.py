# worynim@gmail.com
"""
@file overlay_manager.py
@brief 캡처 영역 오버레이 창(4선 + 드래그 핸들) 생성 및 제어 모듈
@details screen_streamer.py에서 분리 (Step 6). 로직 1:1 이동, 변경 없음.
"""

import tkinter as tk
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    from screen_streamer import VibeStreamerApp


class OverlayManager:
    """
    캡처 영역을 시각화하는 오버레이 창 관리자.

    Parameters:
        root : tk.Tk           — 메인 윈도우
        app  : VibeStreamerApp  — 앱 인스턴스 (capture_area, overlays 등 공유 상태)
    """

    def __init__(self, root: tk.Tk, app: "VibeStreamerApp") -> None:
        self.root = root
        self.app  = app

    # ──────────────────────────────────────────
    # 오버레이 표시
    # ──────────────────────────────────────────
    def show_overlay(self) -> None:
        """캡처 영역 오버레이(4선 + 핸들 + 좌표창)를 화면에 표시."""
        import mss
        app = self.app

        # 기존 오버레이 제거
        self.hide_overlay()

        # 1. 캡처 대상 모니터 결정
        with mss.mss() as sct:
            mon_idx = 1 if len(sct.monitors) > 1 else 0
            target_mon = sct.monitors[mon_idx]

        # 2. 크기 계산
        cap_str = app.cap_size_var.get()
        is_portrait = "Portrait" in app.layout_var.get()

        if cap_str == "Full":
            if is_portrait:
                height = target_mon['height'] - 40  # SAFE_H
                width  = height // 8
            else:
                width  = target_mon['width'] - 10   # SAFE_W
                height = width // 8
        else:
            _, _, width, height = app._calc_capture_area(target_mon)

        # 3. 위치 결정 (핸들 고정 원칙, 경계 보정 없음)
        if app.capture_area:
            left, top = app.capture_area['left'], app.capture_area['top']
        else:
            left, top, _, _ = app._calc_capture_area(target_mon)

        mon_idx = 0  # 캡처 엔진 전달용

        # capture_area 갱신/유지
        app.capture_area = {
            'left': left, 'top': top,
            'width': width, 'height': height,
            'mon': mon_idx
        }

        pad      = 2               # 테두리와 캡처 영역 사이 여백
        HANDLE_H = 24              # 핸들 바 크기 (정사각형)
        GREEN    = "#00FF00"       # 테두리 색상

        # 오버레이 원점 (핸들 좌상단)
        ox = left - pad
        oy = top - HANDLE_H - pad
        app.overlay_ox = ox
        app.overlay_oy = oy

        # 캡처 영역 상대 좌표 (오버레이 원점 기준)
        cx1 = pad - 1
        cy1 = HANDLE_H + pad - 1
        cx2 = pad + width
        cy2 = HANDLE_H + pad + height

        # 4개의 선 윈도우 (Top / Bottom / Left / Right)
        line_configs = [
            (ox + cx1,  oy + cy1,  cx2 - cx1 + 1, 1),  # Top
            (ox + cx1,  oy + cy2,  cx2 - cx1 + 1, 1),  # Bottom
            (ox + cx1,  oy + cy1,  1, cy2 - cy1 + 1),   # Left
            (ox + cx2,  oy + cy1,  1, cy2 - cy1 + 1),   # Right
        ]

        app.overlays = []
        for (x, y, gw, gh) in line_configs:
            win = tk.Toplevel(self.root)
            win.overrideredirect(True)
            win.attributes('-topmost', True)
            win.geometry(f"{gw}x{gh}+{x}+{y}")
            win.config(bg=GREEN)
            app.overlays.append(win)

        # 핸들 윈도우 (드래그용 정사각형 버튼)
        handle_win = tk.Toplevel(self.root)
        handle_win.overrideredirect(True)
        handle_win.attributes('-topmost', True)
        handle_win.geometry(f"{HANDLE_H}x{HANDLE_H}+{ox}+{oy}")
        handle_win.config(bg=GREEN)
        lbl = tk.Label(handle_win, text="✥", bg=GREEN, fg="black",
                       font=("Arial", 16, "bold"), bd=0)
        lbl.pack(fill="both", expand=True)
        app.overlays.append(handle_win)

        # 좌표 표시용 별도 윈도우 (핸들 옆에 부착)
        coord_win = tk.Toplevel(self.root)
        coord_win.overrideredirect(True)
        coord_win.attributes('-topmost', True)
        coord_win.geometry(f"+{ox + HANDLE_H + 5}+{oy}")
        coord_win.config(bg=GREEN)
        coord_text = f"{app.capture_area['left']}, {app.capture_area['top']}"
        app.handle_label = tk.Label(coord_win, text=coord_text, bg=GREEN, fg="black",
                                    font=("Arial", 10, "bold"), padx=5)
        app.handle_label.pack()
        app.overlays.append(coord_win)

        # 모든 오버레이 윈도우에 드래그 이벤트 바인딩
        for win in app.overlays:
            win.bind("<ButtonPress-1>",   self._ol_drag_start)
            win.bind("<B1-Motion>",       self._ol_drag_move)
            win.bind("<ButtonRelease-1>", self._ol_drag_end)
            for child in win.winfo_children():
                child.bind("<ButtonPress-1>",   self._ol_drag_start)
                child.bind("<B1-Motion>",       self._ol_drag_move)
                child.bind("<ButtonRelease-1>", self._ol_drag_end)

        self.root.focus_force()
        self._keep_overlay_on_top()

    # ──────────────────────────────────────────
    # 항상 최상위 유지 (1초 주기)
    # ──────────────────────────────────────────
    def _keep_overlay_on_top(self) -> None:
        """1초 주기로 오버레이가 항상 최상위에 있도록 유지"""
        app = self.app
        if app.overlays and app.show_overlay_var.get():
            if not app.is_dragging:
                for win in app.overlays:
                    try:
                        win.attributes('-topmost', True)
                    except tk.TclError:
                        pass  # 윈도우 이미 소멸된 경우 정상
            self.root.after(1000, self._keep_overlay_on_top)

    # ──────────────────────────────────────────
    # 오버레이 제거
    # ──────────────────────────────────────────
    def hide_overlay(self) -> None:
        """오버레이 윈도우 전체 제거"""
        app = self.app
        for win in app.overlays:
            try:
                win.destroy()
            except Exception:
                pass
        app.overlays = []

    # ──────────────────────────────────────────
    # 드래그 이벤트
    # ──────────────────────────────────────────
    def _ol_drag_start(self, event: Any) -> None:
        """오버레이 드래그 시작"""
        app = self.app
        app.is_dragging   = True
        app.drag_start_x  = event.x_root
        app.drag_start_y  = event.y_root
        app.win_start_x   = app.overlay_ox
        app.win_start_y   = app.overlay_oy

    def _ol_drag_move(self, event: Any) -> None:
        """드래그 중: 오버레이 위치 실시간 갱신"""
        app = self.app
        dx = event.x_root - app.drag_start_x
        dy = event.y_root - app.drag_start_y
        self._update_overlay_positions(app.win_start_x + dx, app.win_start_y + dy)

    def _ol_drag_end(self, event: Any) -> None:
        """드래그 완료: capture_area 갱신 후 드래그 플래그 해제"""
        app = self.app
        dx = event.x_root - app.drag_start_x
        dy = event.y_root - app.drag_start_y
        self._update_overlay_positions(app.win_start_x + dx, app.win_start_y + dy)
        app.is_dragging = False
        try:
            self.root.focus_force()
        except Exception:
            pass

    def _update_overlay_positions(self, new_ox: int, new_oy: int) -> None:
        """
        오버레이 6개 윈도우 위치 갱신 및 capture_area.left/top 동기화.
        """
        app = self.app
        app.overlay_ox = new_ox
        app.overlay_oy = new_oy

        if not app.capture_area:
            return

        pad      = 2
        HANDLE_H = 24
        w = app.capture_area['width']
        h = app.capture_area['height']

        # capture_area의 실제 좌표 갱신 (캡처 루프가 이 값을 읽음)
        app.capture_area['left'] = new_ox + pad
        app.capture_area['top']  = new_oy + HANDLE_H + pad

        cx1 = pad - 1
        cy1 = HANDLE_H + pad - 1
        cx2 = pad + w
        cy2 = HANDLE_H + pad + h

        if len(app.overlays) == 6:
            app.overlays[0].geometry(f"+{new_ox + cx1}+{new_oy + cy1}")        # Top
            app.overlays[1].geometry(f"+{new_ox + cx1}+{new_oy + cy2}")        # Bottom
            app.overlays[2].geometry(f"+{new_ox + cx1}+{new_oy + cy1}")        # Left
            app.overlays[3].geometry(f"+{new_ox + cx2}+{new_oy + cy1}")        # Right
            app.overlays[4].geometry(f"+{new_ox}+{new_oy}")                    # Handle
            app.overlays[5].geometry(f"+{new_ox + HANDLE_H + 5}+{new_oy}")    # Coord

            # 좌표 실시간 업데이트
            if hasattr(app, 'handle_label'):
                try:
                    app.handle_label.config(
                        text=f"{new_ox + pad}, {new_oy + HANDLE_H + pad}"
                    )
                except Exception:
                    pass
