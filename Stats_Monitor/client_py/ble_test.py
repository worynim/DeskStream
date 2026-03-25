import asyncio
import json
from bleak import BleakScanner, BleakClient

# 기본 NUS UUID
UART_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
UART_RX_CHAR_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

async def run():
    print("1. 'DeskStream_Stats' 장치 검색 중...")
    device = await BleakScanner.find_device_by_name("DeskStream_Stats", timeout=10.0)
    
    if not device:
        print("❌ 장치를 찾을 수 없습니다.")
        return

    print(f"✅ 장치 발견! [{device.address}] 연결 시도 중...")
    
    async with BleakClient(device) as client:
        print(f"🔗 연결 성공: {client.is_connected}")
        
        # OLED에 표시될 테스트 데이터 (JSON)
        test_data = {
            "c_u": 25.5, "c_t": 45.0,
            "g_u": 10.2, "g_t": 50.0,
            "r_u": 60.0, "d_u": 15.5,
            "p_w": 12.5
        }
        
        payload = json.dumps(test_data)
        print(f"📤 데이터 전송: {payload}")
        await client.write_gatt_char(UART_RX_CHAR_UUID, payload.encode())
        
        await asyncio.sleep(2)
        print("✅ 전송 완료. OLED 화면이 업데이트 되었는지 확인하세요.")

if __name__ == "__main__":
    asyncio.run(run())
