# worynim@gmail.com
"""
@file network_manager.py
@brief ESP32 IP 자동 검색 및 UDP 프레임 전송 관리 모듈
@details screen_streamer.py에서 분리 (Step 6). 로직 변경 없음.
"""

import socket
import threading
import queue
import time
import logging
from typing import Callable, Dict, Optional, TYPE_CHECKING

if TYPE_CHECKING:
    from screen_streamer import VibeStreamerApp

logger = logging.getLogger("ScreenStreamer")


class NetworkManager:
    """
    ESP32 IP 탐색 및 UDP 프레임 전송을 담당하는 네트워크 매니저.

    Parameters:
        app    : VibeStreamerApp — 앱 인스턴스 (스트리밍 상태 및 큐 참조용)
        config : dict           — CONFIG 딕셔너리
    """

    def __init__(self, app: "VibeStreamerApp", config: Dict) -> None:
        self.app    = app
        self.config = config

    # ──────────────────────────────────────────
    # IP 자동 검색
    # ──────────────────────────────────────────
    def discover_esp32_ip(self) -> Optional[str]:
        """
        UDP 수신 대기로 ESP32 장치를 검색하고 IP를 반환.
        [Step 2 수정] with 문으로 소켓 자동 해제 — bind 실패 시에도 누수 없음

        Returns:
            발견된 IP 문자열, 없으면 None
        """
        config = self.config
        found = None
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try:
                s.bind(('', config['DISCOVERY_PORT']))
            except Exception:
                return None
            s.settimeout(1.5)
            start = time.time()
            while time.time() - start < 2.0:
                try:
                    data, addr = s.recvfrom(1024)
                    if config['DISCOVERY_MSG'] in data:
                        found = addr[0]
                        break
                except Exception:
                    continue
        return found

    def scan_ip_async(self, callback: Callable[[Optional[str]], None]) -> None:
        """
        백그라운드 스레드에서 IP 검색 후 메인 스레드에서 콜백 호출.

        Parameters:
            callback : 검색 완료 시 호출할 함수 (found_ip: Optional[str])
        """
        def _worker() -> None:
            found = self.discover_esp32_ip()
            self.app.root.after(0, callback, found)

        threading.Thread(target=_worker, daemon=True).start()

    # ──────────────────────────────────────────
    # UDP 전송 루프 (송신 스레드)
    # ──────────────────────────────────────────
    def udp_sender_loop(self) -> None:
        """
        4096 bytes를 1024 bytes 청크로 분할하여 UDP 전송.
        헤더: [Frame ID(1)] [Total Chunks(1)] [Chunk Index(1)]
        """
        app        = self.app
        config     = self.config
        chunk_size = config['CHUNK_SIZE']

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)  # 브로드캐스트 전송 허용

        # try/finally로 예외 발생 시에도 소켓 안전 해제 보장
        try:
            while app.streaming_active:
                try:
                    fid, data = app.net_queue.get(timeout=0.5)
                except queue.Empty:
                    continue

                total = (len(data) + chunk_size - 1) // chunk_size
                for i in range(total):
                    header  = bytes([fid, min(total, 255), i])
                    payload = data[i * chunk_size: (i + 1) * chunk_size]
                    try:
                        sock.sendto(header + payload, (app.target_ip_str, config['UDP_PORT']))
                        if app._send_error_count >= 10:  # 네트워크 복구 감지
                            app.root.after(0, lambda: app.scan_status_var.set("Ready"))
                        app._send_error_count = 0
                    except OSError as _send_err:
                        app._send_error_count += 1
                        logger.debug(f"UDP 전송 실패 ({app._send_error_count}회): {_send_err}")
                        if app._send_error_count == 10:
                            app.root.after(0, lambda: app.scan_status_var.set("⚠ Network Error"))
                    # 청크 간 500μs 딜레이: ESP32가 pushCanvas() 블로킹 중일 때
                    # 다음 청크를 즉시 보내면 UDP 수신 버퍼에 쌓여 누락 발생
                    # → 약간 분산하여 전송하면 조립 성공률 현저히 향상
                    if i < total - 1:
                        time.sleep(0.0005)  # 500μs × 3 = 1.5ms 추가, 30fps에서 무시 가능

                # task_done(): ValueError 방어 (큐 상태 불일치 시 스레드 사망 방지)
                try:
                    app.net_queue.task_done()
                except ValueError:
                    pass
        finally:
            sock.close()
            logger.info("[Sender] 소켓 해제 완료")
