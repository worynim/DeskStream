import asyncio
import threading
import sys
from PIL import Image, ImageDraw
import pystray
from metrics import MetricsCollector
from ble_client import BLEStatsTransmitter

# 전송 주기 (초)
SEND_INTERVAL = 1  


class StatsMonitorApp:
    """메인 애플리케이션 클래스"""

    def __init__(self) -> None:
        self.collector = MetricsCollector()
        self.transmitter = BLEStatsTransmitter(target_name="DeskStream_Stats")
        self.is_running: bool = True
        self.icon: pystray.Icon | None = None
        self.send_count: int = 0

    def create_image(self) -> Image.Image:
        """아이콘 이미지 생성"""
        image = Image.new("RGB", (64, 64), color=(30, 30, 30))
        d = ImageDraw.Draw(image)
        d.rectangle([8, 12, 56, 44], outline=(0, 200, 100), width=2) # 모니터 프레임
        d.rectangle([24, 44, 40, 52], fill=(0, 200, 100)) # 스탠드
        return image

    def on_quit(self, icon: pystray.Icon, item: pystray.MenuItem) -> None:
        """종료 처리"""
        print("Quitting Stats Monitor...")
        self.is_running = False
        icon.stop()

    async def data_loop(self) -> None:
        """데이터 수집 및 전송 메인 루프"""
        print("Starting data collection loop...")
        
        while self.is_running:
            try:
                data = self.collector.collect_all()
                success = await self.transmitter.send_data(data)
                if success:
                    self.send_count += 1
                    print(f"[#{self.send_count}] Sent: {data}")
            except Exception as e:
                print(f"Loop error: {e}")

            await asyncio.sleep(SEND_INTERVAL)

    def start_async_loop(self) -> None:
        """별도 스레드에서 asyncio 이벤트 루프 실행"""
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        try:
            loop.run_until_complete(self.data_loop())
        except Exception as e:
            print(f"Async loop error: {e}")

    def run(self) -> None:
        """메인 실행 (트레이=메인스레드)"""
        # 백그라운드 스레드: 데이터 수집 + BLE 전송
        data_thread = threading.Thread(
            target=self.start_async_loop, daemon=True
        )
        data_thread.start()

        # 메인 스레드: 시스템 트레이 (macOS 필수)
        menu = pystray.Menu(pystray.MenuItem("Quit", self.on_quit))
        self.icon = pystray.Icon(
            "StatsMonitor", self.create_image(), "DeskStream Stats", menu
        )
        print("Starting System Tray...")
        self.icon.run()


if __name__ == "__main__":
    app = StatsMonitorApp()
    app.run()
