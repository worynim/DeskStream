import psutil
import platform
import subprocess
import os
import re
import threading
import time

class PowerMetricsReader:
    """sudo powermetrics를 백그라운드에서 실행하며 1초 단위로 M2의 최신 온도/전력을 캐치하는 파서"""
    def __init__(self):
        self.power_w = 0.0
        self.temp_c = 0.0
        self.is_running = True
        self.thread = threading.Thread(target=self._run_powermetrics, daemon=True)
        self.thread.start()

    def _run_powermetrics(self):
        # M2에서 1초(1000ms) 단위로 온도와 전력(cpu_power,thermal)을 지속 출력하는 명령어
        cmd = ["sudo", "powermetrics", "-i", "1000", "--samplers", "cpu_power,thermal"]
        
        try:
            # 실시간 출력을 위해 subprocess.Popen 사용
            process = subprocess.Popen(
                cmd, 
                stdout=subprocess.PIPE, 
                stderr=subprocess.DEVNULL, 
                text=True, 
                bufsize=1 # Line buffering
            )
            
            # 파이프에서 한 줄씩 실시간으로 읽기
            for line in process.stdout:
                if not self.is_running:
                    process.terminate()
                    break
                    
                line = line.strip()
                
                # M2 powermetrics 예상 출력파싱
                # 예: "CPU Power: 1543 mW" -> 1.5W
                if "CPU Power:" in line or "Combined Power:" in line or "Package Power:" in line:
                    match = re.search(r"(\d+)\s+mW", line)
                    if match:
                        self.power_w = round(int(match.group(1)) / 1000.0, 1)
                        
                # 예: "CPU die temperature: 54.2 C"
                elif "temp" in line.lower() or "thermal" in line.lower() or "die" in line.lower():
                    # "54.20 C" 또는 "54.2 C" 형태
                    match = re.search(r"([\d.]+)\s?[C|c]", line)
                    if match:
                        val = float(match.group(1))
                        if 10.0 <= val <= 110.0:  # 비정상 값 필터링
                            self.temp_c = round(val, 1)

        except Exception as e:
            print(f"[PowerMetricsReader] Failed to start. Run with sudo! {e}")

    def stop(self):
        self.is_running = False

class MetricsCollector:
    """macOS M2 Mac 전용 정밀 수집기 (1Hz 실시간 온도/전력 포함)"""
    
    def __init__(self):
        self.os_type = platform.system()
        # 실시간 모니터 스레드 작동
        self.pm_reader = PowerMetricsReader()

    def get_cpu_perf(self) -> float:
        return psutil.cpu_percent(interval=0.1)

    def get_ram_perf(self) -> float:
        vm = psutil.virtual_memory()
        return round(vm.percent, 1)

    def get_disk_perf(self) -> float:
        try:
            path = "/System/Volumes/Data"
            if not os.path.exists(path):
                path = "/"
            usage = psutil.disk_usage(path)
            return round((usage.used / usage.total) * 100, 1)
        except Exception:
            return 0.0

    def get_gpu_usage_macos(self) -> float:
        try:
            cmd = "ioreg -c AGXAccelerator -r -l"
            res = subprocess.check_output(cmd, shell=True, stderr=subprocess.DEVNULL).decode()
            matches = re.findall(r"[Uu]tilization.*?(\d+)", res)
            if matches:
                return float(max(int(m) for m in matches))
        except Exception:
            pass
        return 0.0

    def get_cpu_temp(self) -> float:
        """powermetrics에서 수집한 최신 1초 갱신 온도 반환"""
        return self.pm_reader.temp_c

    def get_power_macos(self) -> float:
        """powermetrics에서 수집한 최신 1초 갱신 전력 반환"""
        return self.pm_reader.power_w

    def collect_all(self) -> dict:
        return {
            "c_u": self.get_cpu_perf(),
            "c_t": self.get_cpu_temp(),
            "g_u": self.get_gpu_usage_macos(),
            "g_t": 0.0,
            "r_u": self.get_ram_perf(),
            "d_u": self.get_disk_perf(),
            "p_w": self.get_power_macos()
        }
