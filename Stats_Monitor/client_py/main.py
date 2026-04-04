import asyncio
import threading
import sys
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
    - 백그라운드 스레드에서 시스템 지표 수집 및 BLE 전송 수행
    """

    def __init__(self) -> None:
        self.collector = MetricsCollector() # 시스템 데이터 수집 객체
        self.transmitter = BLEStatsTransmitter(target_name="DeskStream_Stats") # BLE 전송 객체
        self.is_running: bool = True
        self.icon: pystray.Icon | None = None
        self.send_count: int = 0

    def create_image(self) -> Image.Image:
        """
        아이콘 이미지 생성 (투명 배경 + 흰색 컴퓨터 실루엣 + DS 로고)
        macOS의 다크/라이트 모드와 투명한 상태 바에 맞춘 디자인
        """
        # RGBA(투명 채널 포함)로 64x64 캔버스 생성
        image = Image.new("RGBA", (64, 64), color=(0, 0, 0, 0))
        d = ImageDraw.Draw(image)
        
        # 흰색 모니터 프레임 (외곽선 3px 두께)
        d.rectangle([8, 12, 56, 44], outline=(255, 255, 255, 255), width=3) 
        # 모니터 스탠드 (내부 채우기)
        d.rectangle([24, 44, 40, 52], fill=(255, 255, 255, 255)) 

        # 모니터 내부 'DS' 텍스트 추가
        try:
            # macOS 시스템 폰트 경로 활용 (Arial Bold)
            font = ImageFont.truetype("/System/Library/Fonts/Supplemental/Arial Bold.ttf", 22)
        except Exception:
            # 폰트 로드 실패 시 기본 폰트 사용
            font = ImageFont.load_default()
        
        # 텍스트 중앙 정렬을 위한 좌표 계산 (박스: [8, 12, 56, 44])
        # 텍스트가 [20, 22] 근처에 오도록 수동 조정 (64x64 아이콘 기준)
        d.text((18, 16), "DS", font=font, fill=(255, 255, 255, 255))
        
        return image

    def on_quit(self, icon: pystray.Icon, item: pystray.MenuItem) -> None:
        """종료 메뉴 선택 시 실행되는 콜백"""
        print("Quitting Stats Monitor...")
        self.is_running = False
        icon.stop()

    async def data_loop(self) -> None:
        """
        [비동기 루틴] 시스템 지표 수집 및 BLE 데이터를 전송하는 무한 루프
        - 1초 단위로 collectors로부터 데이터를 가져와 JSON 형태로 전송
        """
        print("Starting data collection loop...")
        
        while self.is_running:
            try:
                # [수집] CPU, GPU, RAM, Disk, Net 데이터 가져오기
                data = self.collector.collect_all()
                # [전송] 수집한 데이터를 BLE 서버(ESP32-C3)로 전송
                success = await self.transmitter.send_data(data)
                
                if success:
                    self.send_count += 1
                    # 콘솔 앱 로그 출력 (터미널에서 확인 가능)
                    print(f"[#{self.send_count}] Sent: {data}")
            except Exception as e:
                print(f"Loop error: {e}")

            # 지정된 전송 주기만큼 대기 (비동기 처리)
            await asyncio.sleep(SEND_INTERVAL)

    def start_async_loop(self) -> None:
        """별도 스레드에서 asyncio 이벤트 루프를 생성하고 실행"""
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        try:
            loop.run_until_complete(self.data_loop())
        except Exception as e:
            print(f"Async loop error: {e}")

    def run(self) -> None:
        """
        애플리케이션 진입점
        - 데이터 루프는 데몬 스레드에서 작동
        - 메인 스레드에서는 macOS 필수 요구 사항인 시스템 트레이 GUI 실행
        """
        # [백그라운드 스레드] 데이터 수집 및 BLE 전송 가동
        data_thread = threading.Thread(
            target=self.start_async_loop, daemon=True
        )
        data_thread.start()

        # [메인 스레드] 시스템 트레이 아이콘 설정 및 실행
        menu = pystray.Menu(pystray.MenuItem("Desk Stream Stats Monitor Quit", self.on_quit))
        self.icon = pystray.Icon(
            "StatsMonitor", 
            self.create_image(), 
            "DeskStream Stats Monitor (v2.4.1)", # 툴팁 정보
            menu
        )
        
        print("Starting System Tray Icon...")
        # icon.run()은 루프가 중단될 때까지 메인 스레드를 점유함
        self.icon.run()

if __name__ == "__main__":
    app = StatsMonitorApp()
    app.run()
