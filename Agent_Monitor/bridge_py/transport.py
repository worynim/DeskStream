# worynim@gmail.com
"""
@file transport.py
@brief DeskStream USB 시리얼 연결 — 포트 자동 탐색, 끊기면 재연결, 줄 단위 송수신
@details pyserial 필요. 포트를 지정하지 않으면 Espressif USB(VID 0x303A) 장치를 찾는다.
         P4 에서 BLE 전송을 같은 인터페이스(start / send)로 추가한다.
"""
import threading
import time

import serial
from serial.tools import list_ports

ESPRESSIF_VID = 0x303A
RETRY_SEC = 2
WRITE_TIMEOUT_SEC = 1   # 장치가 읽지 않아 쓰기가 막히면 끊고 재연결 (송신 스레드가 멈추지 않게)


def find_port():
    for p in list_ports.comports():
        if p.vid == ESPRESSIF_VID:
            return p.device
    return None


class SerialLink:
    def __init__(self, on_line, on_connect, port=None, baud=115200, log=print):
        self.on_line = on_line          # 장치에서 한 줄 받을 때마다 호출 (str)
        self.on_connect = on_connect    # 연결될 때마다 호출 (전체 상태 재전송용)
        self.port, self.baud, self.log = port, baud, log
        self._ser = None
        self._lock = threading.Lock()

    @property
    def connected(self) -> bool:
        return self._ser is not None

    def start(self):
        threading.Thread(target=self._run, daemon=True).start()

    def send(self, line: str):
        with self._lock:
            if self._ser is None:
                return
            try:
                self._ser.write((line + "\n").encode("ascii"))
            except (serial.SerialException, OSError) as e:
                self.log(f"[serial] 쓰기 실패: {e}")
                self._close_locked()

    def _run(self):
        while True:
            port = self.port or find_port()
            if not port:
                time.sleep(RETRY_SEC)
                continue
            try:
                ser = serial.Serial(port, self.baud, timeout=1, write_timeout=WRITE_TIMEOUT_SEC)
            except (serial.SerialException, OSError):
                time.sleep(RETRY_SEC)
                continue
            with self._lock:
                self._ser = ser
            self.log(f"[serial] 연결됨: {port}")
            self.on_connect()
            self._read_loop(ser)
            self.log("[serial] 연결 끊김 — 재연결 대기")
            time.sleep(RETRY_SEC)

    def _read_loop(self, ser):
        while True:
            try:
                raw = ser.readline()
            except (serial.SerialException, OSError, TypeError):
                break       # TypeError: 다른 스레드가 포트를 닫은 경우
            if raw:
                self.on_line(raw.decode("utf-8", errors="replace"))
            with self._lock:
                if self._ser is not ser:
                    return
        with self._lock:
            if self._ser is ser:
                self._close_locked()

    def _close_locked(self):
        try:
            self._ser.close()
        except Exception:
            pass
        self._ser = None
