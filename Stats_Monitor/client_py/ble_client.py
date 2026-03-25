import asyncio
import json
from bleak import BleakScanner, BleakClient

# UUID (표준 UART 예제 UUID와 동일)
UART_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
UART_RX_CHAR_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

class BLEStatsTransmitter:
    """ESP32 Stats Monitor에 데이터를 전송하는 클래스"""
    
    def __init__(self, target_name="DeskStream_Stats"):
        self.target_name = target_name
        self.client = None
        self.is_connected = False
        self.device = None

    async def connect(self) -> bool:
        """장치를 검색하고 연결 수립"""
        if self.is_connected and self.client and self.client.is_connected:
            return True
        
        self.is_connected = False  # 상태 초기화
        
        try:
            print(f"Scanning for '{self.target_name}'...")
            self.device = await BleakScanner.find_device_by_name(self.target_name, timeout=5.0)
            
            if not self.device:
                print("Device NOT found.")
                return False

            print(f"Connecting to [{self.device.address}]...")
            self.client = BleakClient(self.device)
            await self.client.connect()
            self.is_connected = self.client.is_connected
            print(f"Connected: {self.is_connected}")
            return self.is_connected
        except Exception as e:
            print(f"Connection failed: {e}")
            self.is_connected = False
            return False

    async def send_data(self, data: dict) -> bool:
        """데이터(JSON) 전송"""
        if not self.is_connected:
            if not await self.connect():
                return False

        try:
            # 연결 상태 재확인 (BLE 끊김 감지)
            if not self.client or not self.client.is_connected:
                self.is_connected = False
                return False
            
            payload = json.dumps(data).encode()
            await self.client.write_gatt_char(UART_RX_CHAR_UUID, payload)
            return True
        except Exception as e:
            print(f"Send failed: {e}")
            self.is_connected = False
            return False
            
    async def disconnect(self):
        if self.client: await self.client.disconnect()
        self.is_connected = False
