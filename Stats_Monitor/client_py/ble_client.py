import asyncio
import json
from bleak import BleakScanner, BleakClient

# ESP32-C3 서비스 표준 UART UUID 정의 (펌웨어와 일치해야 함)
UART_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
UART_RX_CHAR_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

class BLEStatsTransmitter:
    """
    ESP32 Stats Monitor 장치를 검색하고 
    시스템 통계 데이터를 BLE(GATT)를 통해 무선 전송하는 클래스
    """
    
    def __init__(self, target_name="DeskStream_Stats"):
        self.target_name = target_name # 찾고자 하는 블루투스 이름
        self.client = None
        self.is_connected = False
        self.device = None

    async def connect(self) -> bool:
        """
        주변의 BLE 장치를 검색하고 연결 수립
        연결 성공 시 True, 실패 시 False 반환
        """
        if self.is_connected and self.client and self.client.is_connected:
            return True # 이미 연결되어 있음
        
        self.is_connected = False  # 상태 초기화
        
        try:
            print(f"Scanning for '{self.target_name}'...")
            # 타겟 장치 검색 (5초 타임아웃)
            self.device = await BleakScanner.find_device_by_name(self.target_name, timeout=5.0)
            
            if not self.device:
                print("Device NOT found. Check if ESP32 is powered on.")
                return False

            print(f"Connecting to [{self.device.address}]...")
            self.client = BleakClient(self.device)
            # BLE 연결 시도
            await self.client.connect()
            self.is_connected = self.client.is_connected
            print(f"Connection Status: {self.is_connected}")
            return self.is_connected
        except Exception as e:
            print(f"Connection failed: {e}")
            self.is_connected = False
            return False

    async def send_data(self, data: dict) -> bool:
        """
        딕셔너리 형태의 데이터를 JSON으로 변환하여 BLE 특성(Characteristic)에 쓰기(Write)
        - 자동 재연결 시도 포함
        """
        # 연결이 끊겼다면 재연결 시도
        if not self.is_connected:
            if not await self.connect():
                return False

        try:
            # 최종 연결 상태 확인
            if not self.client or not self.client.is_connected:
                self.is_connected = False
                return False
            
            # [전송 과정] 딕셔너리 -> JSON 문자열 -> 바이트스트림 인코딩
            payload = json.dumps(data).encode()
            
            # ESP32의 RX 특성 핸들러에 데이터 전송 (UART 방식 상속)
            # write_gatt_char를 통해 MTU 사이즈 내에서 데이터 패킷 전송
            await self.client.write_gatt_char(UART_RX_CHAR_UUID, payload)
            return True
        except Exception as e:
            print(f"Send failed: {e}")
            self.is_connected = False # 전송 실패 시 연결 상태 해제 (다음 루프에서 재접속 유도)
            return False
            
    async def disconnect(self):
        """블루투스 연결 명시적 해제"""
        if self.client: 
            await self.client.disconnect()
        self.is_connected = False
        print("Disconnected from BLE device.")
