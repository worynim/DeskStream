# worynim@gmail.com
"""
@file main.py
@brief PC 시스템 정보 수집 및 BLE 전송 메인 스크립트
@details psutil을 이용한 메트릭 수집 및 BLE 클라이언트를 통한 ESP32 데이터 전송 통합 관리
"""
import asyncio
import threading
import sys
import time
from PIL import Image, ImageDraw, ImageFont
import pystray
from metrics import MetricsCollector
from ble_client import BLEStatsTransmitter

# 데이터 수집 및 BLE 전송 주기 (초)
SEND_INTERVAL = 1  

class StatsMonitorApp:
    """
    메인 애플리케이션 클래스 (macOS 시스템 트레이 지원)
    - 64x64 사이즈의 다이내믹 아이콘 생성
    - macOS Template Image 기능을 지원하여 배경색에 따라 아이콘 색상 자동 전환
    """

    def __init__(self) -> None:
        self.collector = MetricsCollector() # 시스템 데이터 수집 객체
        self.transmitter = BLEStatsTransmitter(target_name="DeskStream_Stats") # BLE 전송 객체
        self.is_running: bool = True
        self.icon: pystray.Icon | None = None
        self.send_count: int = 0
        self.win_theme: str = self.get_windows_taskbar_theme()

    def get_windows_taskbar_theme(self) -> str:
        """Windows 작업 표시줄 단독 테마 감지 (다크/라이트 모드)"""
        if sys.platform != "win32":
            return "Dark"
        
        try:
            import winreg
            registry = winreg.ConnectRegistry(None, winreg.HKEY_CURRENT_USER)
            key = winreg.OpenKey(registry, r"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize")
            # SystemUsesLightTheme: 0 = 다크 모드, 1 = 라이트 모드
            value, _ = winreg.QueryValueEx(key, "SystemUsesLightTheme")
            winreg.CloseKey(key)
            return "Light" if value == 1 else "Dark"
        except Exception:
            return "Dark" # 읽기 실패 시 기본적으로 다크 모드로 간주

    def get_font(self, size: int):
        """OS별로 가장 쾌적한 폰트 경로 매핑"""
        try:
            if sys.platform == "darwin":
                font_path = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
            elif sys.platform == "win32":
                font_path = "C:\\Windows\\Fonts\\arialbd.ttf"
            else:
                font_path = "arial.ttf"
            return ImageFont.truetype(font_path, size)
        except Exception:
            return ImageFont.load_default()

    def create_image(self) -> Image.Image:
        """
        OS 호환 아이콘 이미지 생성
        - macOS: Template Image를 위해 항상 검정색 반환 (OS가 알아서 반전 처리)
        - Windows: 수집된 윈도우 테마에 따라 뚜렷한 흰색/검은색 명시적 반환
        """
        image = Image.new("RGBA", (64, 64), color=(0, 0, 0, 0))
        d = ImageDraw.Draw(image)

        if sys.platform == "darwin":
            # macOS는 Template Mode를 활용하므로 시스템이 반전시킬 수 있도록 검은색 지정
            icon_color = (0, 0, 0, 255)
        else:
            # 윈도우 등은 Template Mode가 없으므로 테마에 맞춰 직접 색상 렌더링
            icon_color = (255, 255, 255, 255) if self.win_theme == "Dark" else (0, 0, 0, 255)

        # 모니터 실루엣 디자인
        d.rectangle([0, 40, 63, 60], outline=icon_color, width=4) 
        d.rectangle([15, 40, 31, 60], outline=icon_color, width=2) 
        d.rectangle([31, 40, 47, 60], outline=icon_color, width=2) 

        # OS 독립적 'D/S' 텍스트 추가
        font = self.get_font(36)
        d.text((2, 0), "D/S", font=font, fill=icon_color)
        
        return image

    def set_template_mode(self) -> bool:
        """
        pystray 아이콘 객체의 내부 NSImage를 찾아 Template 모드 활성화
        성공 시 True를 반환함
        """
        if sys.platform != 'darwin' or self.icon is None:
            return False

        try:
            # pystray macOS 백엔드 구조에 직접 접근
            # _status_item은 pystray._darwin.Icon 내의 NSStatusItem 인스턴스임
            status_item = getattr(self.icon, '_status_item', None)
            
            # 만약 _status_item이 직접 없다면 _icon 내에서 찾아봄
            if status_item is None:
                inner_icon = getattr(self.icon, '_icon', None)
                if inner_icon:
                    status_item = getattr(inner_icon, '_item', None) or getattr(inner_icon, '_status_item', None)

            if status_item:
                button = status_item.button()
                if button and button.image():
                    # Template 모드 활성화 - macOS가 밝은 배경에선 검정, 어두운 배경에선 흰색으로 그려줌
                    button.image().setTemplate_(True)
                    print("macOS Template Mode enabled successfully via NSStatusItem.")
                    return True
            
            return False
        except Exception as e:
            # 초기화 도중에는 오류가 날 수 있으므로 재시도 유도
            return False

    def on_quit(self, icon: pystray.Icon, item: pystray.MenuItem) -> None:
        """종료 메뉴 선택 시 실행되는 콜백"""
        print("Quitting Stats Monitor...")
        self.is_running = False
        icon.stop()

    async def data_loop(self) -> None:
        """[비동기 루틴] 데이터 수집 및 BLE 전송"""
        print("Starting data collection loop...")
        
        # [중요] 아이콘이 완전히 렌더링될 때까지 반복적으로 템플릿 모드 적용 시도 (최대 10초)
        template_applied = False
        for _ in range(20):
            if self.set_template_mode():
                template_applied = True
                break
            await asyncio.sleep(0.5)
        
        if not template_applied:
            print("Warning: Could not enable macOS Template Mode. The icon won't adapt to background brightness.")

        while self.is_running:
            try:
                # [Windows 테마 갱신] Windows 환경에서 실시간 배경 테마 변경 감지
                if sys.platform == "win32":
                    new_win_theme = self.get_windows_taskbar_theme()
                    if new_win_theme != self.win_theme and self.icon is not None:
                        print(f"Windows theme changed to {new_win_theme}. Updating tray icon...")
                        self.win_theme = new_win_theme
                        self.icon.icon = self.create_image()

                data = self.collector.collect_all()
                success = await self.transmitter.send_data(data)
                
                if success:
                    self.send_count += 1
                    print(f"[#{self.send_count}] Sent: {data}")
            except Exception as e:
                print(f"Data loop error: {e}")

            await asyncio.sleep(SEND_INTERVAL)

    def start_async_loop(self) -> None:
        """스레드 내에서 asyncio 루프 실행"""
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        try:
            loop.run_until_complete(self.data_loop())
        except Exception as e:
            print(f"Async loop error: {e}")

    def run(self) -> None:
        """애플리케이션 실행"""
        # 데이터 수집 루프를 백그라운드 스레드에서 시작
        data_thread = threading.Thread(
            target=self.start_async_loop, daemon=True
        )
        data_thread.start()

        # 시스템 트레이 아이콘 설정 및 실행
        menu = pystray.Menu(pystray.MenuItem("Desk Stream Stats Monitor Quit", self.on_quit))
        self.icon = pystray.Icon(
            "StatsMonitor", 
            self.create_image(), 
            "DeskStream Stats Monitor (v2.4.1)", 
            menu
        )
        
        print("Starting System Tray Icon...")
        self.icon.run()

if __name__ == "__main__":
    app = StatsMonitorApp()
    app.run()
