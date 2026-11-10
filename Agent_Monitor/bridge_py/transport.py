# worynim@gmail.com
"""
@file transport.py
@brief DeskStream 연결 — USB 시리얼 우선, 없으면 BLE UART. 끊기면 다시 찾는다. 줄 단위 송수신.
@details - SerialLink: pyserial. 포트를 지정하지 않으면 Espressif USB(VID 0x303A) 장치를 찾는다.
         - BleLink: bleak. 이름 "DeskStream Agent" 로 광고하는 장치에 연결, Nordic UART UUID 로 주고받는다.
           BLE 로 온 버튼·승인 신호는 Bridge 가 서명을 확인한다 (security.py).
         - AutoLink: USB 장치가 보이면 USB, 아니면 BLE. BLE 사용 중 USB 가 꽂히면 USB 로 바꾼다.
         - register_key_via_usb: BLE 전용 모드에서 키가 다를 때 USB 포트를 잠깐 열어 키만 등록하고 닫는다.
         공통 인터페이스: start() · send(line) · connected · kind("usb" / "ble" / None)
"""
import asyncio
import threading
import time

import serial
from serial.tools import list_ports

ESPRESSIF_VID = 0x303A
RETRY_SEC = 2
WRITE_TIMEOUT_SEC = 1   # 장치가 읽지 않아 쓰기가 막히면 끊고 재연결 (송신 스레드가 멈추지 않게)
PROVISION_SEC = 3       # BLE 모드에서 USB 로 키를 등록할 때 장치 hello 를 기다리는 시간
BLE_NAME = "DeskStream Agent"   # 펌웨어 config.h AM_BLE_NAME
BLE_RX_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"   # PC → 장치
BLE_TX_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"   # 장치 → PC
BLE_SCAN_SEC = 5


def find_port():
    for p in list_ports.comports():
        if p.vid == ESPRESSIF_VID:
            return p.device
    return None


def register_key_via_usb(setkey_line: str, kid: str, log=print, port=None, timeout=PROVISION_SEC,
                         open_serial=None) -> bool:
    """BLE 로 쓰는 중에 USB 포트를 잠깐 열어 서명 키를 등록하고 바로 닫는다.
    장치가 hello 로 kid 를 알려 주면 그 값이 kid 와 같을 때 성공. USB 장치가 없거나 포트가 사용 중이면 False."""
    port = port or find_port()
    if port is None:
        return False
    opener = open_serial or (lambda p: serial.Serial(p, 115200, timeout=0.2, write_timeout=WRITE_TIMEOUT_SEC))
    try:
        ser = opener(port)
    except (serial.SerialException, OSError):
        return False    # 시리얼 모니터·다른 앱이 쓰는 중 — 조용히 다음 시도를 기다린다
    try:
        ser.write((setkey_line + "\n").encode("utf-8"))
        end = time.monotonic() + timeout
        buf = b""
        while time.monotonic() < end:
            buf += ser.read(256)
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                if b'"t":"hello"' in raw and f'"kid":"{kid}"'.encode() in raw:
                    return True
        return False
    except (serial.SerialException, OSError) as e:
        log(f"[usb] 키 등록 실패: {e}")
        return False
    finally:
        try:
            ser.close()
        except Exception:
            pass


class SerialLink:
    kind = "usb"

    def __init__(self, on_line, on_connect, log=print):
        self.on_line = on_line          # 장치에서 한 줄 받을 때마다 호출 (str)
        self.on_connect = on_connect    # 연결될 때마다 호출 (전체 상태 재전송용)
        self.log = log
        self._ser = None
        self._lock = threading.Lock()

    @property
    def connected(self) -> bool:
        return self._ser is not None

    def send(self, line: str):
        with self._lock:
            if self._ser is None:
                return
            try:
                self._ser.write((line + "\n").encode("utf-8"))
            except (serial.SerialException, OSError) as e:
                self.log(f"[serial] 쓰기 실패: {e}")
                self._close_locked()

    def session(self, port: str):
        """port 에 연결해 끊길 때까지 읽는다 (호출한 스레드를 붙잡는다)."""
        try:
            ser = serial.Serial(port, 115200, timeout=1, write_timeout=WRITE_TIMEOUT_SEC)
        except (serial.SerialException, OSError):
            return
        with self._lock:
            self._ser = ser
        self.log(f"[usb] 연결됨: {port}")
        self.on_connect()
        while True:
            try:
                raw = ser.readline()
            except (serial.SerialException, OSError, TypeError):
                break       # TypeError: 다른 스레드가 포트를 닫은 경우
            if raw:
                self.on_line(raw.decode("utf-8", errors="replace"))
            with self._lock:
                if self._ser is not ser:
                    break
        with self._lock:
            if self._ser is ser:
                self._close_locked()
        self.log("[usb] 연결 끊김")

    def _close_locked(self):
        try:
            self._ser.close()
        except Exception:
            pass
        self._ser = None


class BleLink:
    kind = "ble"

    def __init__(self, on_line, on_connect, log=print, name=BLE_NAME):
        self.on_line, self.on_connect, self.log, self.name = on_line, on_connect, log, name
        self._client = None
        self._loop = None
        self._buf = b""

    @property
    def connected(self) -> bool:
        return self._client is not None

    def send(self, line: str):
        client, loop = self._client, self._loop
        if client is None or loop is None:
            return
        fut = asyncio.run_coroutine_threadsafe(self._write(client, (line + "\n").encode("utf-8")), loop)
        try:
            fut.result(timeout=3)
        except Exception as e:      # 연결 끊김·시간 초과 — session 이 정리한다
            self.log(f"[ble] 쓰기 실패: {e!r}")

    def session(self, should_stop):
        """장치를 찾아 연결하고 끊기거나 should_stop() 이 참이 될 때까지 유지한다."""
        asyncio.run(self._session(should_stop))

    async def _write(self, client, data: bytes):
        size = max(20, (client.mtu_size or 23) - 3)
        for off in range(0, len(data), size):
            await client.write_gatt_char(BLE_RX_UUID, data[off:off + size], response=False)

    def _on_notify(self, _char, data: bytearray):
        self._buf += bytes(data)
        while b"\n" in self._buf:
            raw, self._buf = self._buf.split(b"\n", 1)
            self.on_line(raw.decode("utf-8", errors="replace"))

    async def _session(self, should_stop):
        from bleak import BleakClient, BleakScanner
        dev = await BleakScanner.find_device_by_filter(
            lambda d, ad: (ad.local_name or d.name) == self.name, timeout=BLE_SCAN_SEC)
        if dev is None or should_stop():
            return
        gone = asyncio.Event()
        client = BleakClient(dev, disconnected_callback=lambda _c: gone.set())
        try:
            await client.connect()
            self._buf = b""
            await client.start_notify(BLE_TX_UUID, self._on_notify)
            self._loop, self._client = asyncio.get_running_loop(), client
            self.log(f"[ble] 연결됨: {self.name} ({dev.address})")
            await asyncio.to_thread(self.on_connect)
            while not gone.is_set() and not should_stop():
                try:
                    await asyncio.wait_for(gone.wait(), 1)
                except asyncio.TimeoutError:
                    pass
        except Exception as e:
            self.log(f"[ble] 연결 실패: {e!r}")
        finally:
            self._client = self._loop = None
            if client.is_connected:
                await client.disconnect()
            self.log("[ble] 연결 끊김")


class AutoLink:
    """mode: auto = USB 가 보이면 USB, 아니면 BLE · usb = USB 만 · ble = BLE 만 (USB 로 전원만 받는 시험용).
    bleak 가 없거나 serial_port 를 지정하면 USB 만."""

    def __init__(self, on_line, on_connect, serial_port=None, log=print, mode="auto"):
        self.serial_port = serial_port
        self.mode = "usb" if serial_port else mode
        self.usb = SerialLink(on_line, on_connect, log)
        self.ble = None
        if self.mode != "usb":
            try:
                import bleak  # noqa: F401
                self.ble = BleLink(on_line, on_connect, log)
            except ImportError:
                log("[ble] bleak 미설치 — USB 만 사용 (pip install bleak)")
        self._active = None

    @property
    def kind(self):
        a = self._active
        return a.kind if a is not None and a.connected else None

    @property
    def connected(self) -> bool:
        return self.kind is not None

    def send(self, line: str):
        a = self._active
        if a is not None:
            a.send(line)

    def register_key(self, setkey_line: str, kid: str) -> bool:
        """BLE 로 연결된 장치의 키가 다를 때 — USB 가 꽂혀 있으면 잠깐 열어 키를 등록한다."""
        return register_key_via_usb(setkey_line, kid, log=self.usb.log, port=self.serial_port)

    def start(self):
        threading.Thread(target=self._run, daemon=True).start()

    def _run(self):
        usb_first = self.mode == "auto"
        while True:
            port = (self.serial_port or find_port()) if self.mode != "ble" else None
            if port:
                self._active = self.usb
                self.usb.session(port)
            elif self.ble is not None:
                self._active = self.ble
                self.ble.session(should_stop=lambda: usb_first and find_port() is not None)   # auto 는 USB 우선
            self._active = None
            time.sleep(RETRY_SEC)
