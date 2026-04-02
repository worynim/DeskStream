import psutil
import platform
import subprocess
import os
import re
import threading
import time
import GPUtil  # 윈도우 GPU 점유율 수집용

class PowerMetricsReader:
    """
    sudo powermetrics를 백그라운드에서 실행하며 1초 단위로 
    M1/M2 맥의 최신 온도 및 전력 데이터를 수집하는 클래스
    """
    def __init__(self):
        self.power_w = 0.0
        self.temp_c = 0.0
        self.is_running = True
        # 백그라운드 스레드에서 powermetrics 실행
        self.thread = threading.Thread(target=self._run_powermetrics, daemon=True)
        self.thread.start()

    def _run_powermetrics(self):
        # M-series 맥의 실시간 하드웨어 센서 데이터를 읽기 위한 명령어
        # 1000ms 간격으로 cpu_power 및 thermal(온도) 데이터 수집
        cmd = ["sudo", "powermetrics", "-i", "1000", "--samplers", "cpu_power,thermal"]
        
        try:
            # 실시간 출력을 한 줄씩 파이프 처리
            process = subprocess.Popen(
                cmd, 
                stdout=subprocess.PIPE, 
                stderr=subprocess.DEVNULL, 
                text=True, 
                bufsize=1 # 라인 버퍼링 활성화
            )
            
            for line in process.stdout:
                if not self.is_running:
                    process.terminate()
                    break
                    
                line = line.strip()
                
                # [전력 데이터 파싱]
                # "Package Power: 1543 mW" 같은 패턴을 찾아 W 단위로 변환
                if any(x in line for x in ["CPU Power:", "Combined Power:", "Package Power:"]):
                    match = re.search(r"(\d+)\s+mW", line)
                    if match:
                        self.power_w = round(int(match.group(1)) / 1000.0, 1)
                        
                # [온도 데이터 파싱]
                # "CPU die temperature: 54.2 C" 같은 패턴을 찾아 가공
                elif any(x in line.lower() for x in ["temp", "thermal", "die"]):
                    match = re.search(r"([\d.]+)\s?[C|c]", line)
                    if match:
                        val = float(match.group(1))
                        if 10.0 <= val <= 110.0:  # 극단적 이상값 필터링
                            self.temp_c = round(val, 1)

        except Exception as e:
            print(f"[PowerMetricsReader] Failed: {e}")

    def stop(self):
        """데이터 수집 중단"""
        self.is_running = False

class MetricsCollector:
    """
    macOS M2 Mac 전용 정밀 하드웨어 데이터 수집기
    CPU, GPU, RAM, Disk, Network 트래픽 정보를 통합 수집
    """
    
    def __init__(self):
        self.os_type = platform.system()
        # 온도/전력 수집 전문 리더 시작
        self.pm_reader = PowerMetricsReader()
        
        # 네트워크 차분 계산을 위한 이전 데이터 저장
        try:
            self._prev_net = psutil.net_io_counters()
            self._prev_time = time.time()
            # CPU 카운터 초기화 (interval=None 사용을 위해 미리 호출)
            psutil.cpu_percent(interval=None)
        except Exception:
            self._prev_net = None
            self._prev_time = time.time()

    def get_cpu_perf(self) -> float:
        """CPU 점유율 (%) 반환 (지난 호출 이후의 평균값)"""
        return psutil.cpu_percent(interval=None)

    def get_ram_perf(self) -> float:
        """메모리 사용률 (%) 반환"""
        vm = psutil.virtual_memory()
        return round(vm.percent, 1)

    def get_disk_perf(self) -> float:
        """시스템 루트 디스크 사용률 (%) 반환"""
        try:
            path = "/System/Volumes/Data" # macOS 데이터 볼륨 우선 확인
            if not os.path.exists(path):
                path = "/"
            usage = psutil.disk_usage(path)
            return round((usage.used / usage.total) * 100, 1)
        except Exception:
            return 0.0

    def get_gpu_usage_macos(self) -> float:
        """ioreg를 활용한 macOS AGX(GPU) 점유율 (%) 추산"""
        try:
            cmd = "ioreg -c AGXAccelerator -r -l"
            res = subprocess.check_output(cmd, shell=True, stderr=subprocess.DEVNULL).decode()
            matches = re.findall(r"[Uu]tilization.*?(\d+)", res)
            if matches:
                return float(max(int(m) for m in matches))
        except Exception:
            pass
        return 0.0

    def get_gpu_usage_windows(self) -> float:
        """Windows 성능 카운터를 활용한 모든 GPU(인텔 내장 포함) 점유율 (%) 수합"""
        try:
            # PowerShell을 사용하여 모든 GPU 엔진의 Utilization Percentage 합산값 추출
            # 인텔 내장, NVIDIA, AMD 모두 대응 가능
            cmd = (
                "powershell -NoProfile -ExecutionPolicy Bypass -Command "
                "\"(Get-Counter '\\GPU Engine(*)\\Utilization Percentage' -ErrorAction SilentlyContinue).CounterSamples | "
                "Measure-Object -Property CookedValue -Sum | Select-Object -ExpandProperty Sum\""
            )
            res = subprocess.check_output(cmd, shell=True, stderr=subprocess.DEVNULL).decode().strip()
            
            if res and res != "0":
                # 여러 엔진(3D, Video, Copy 등) 점유율의 합이므로 100이 넘을 수 있으나 
                # 일반적인 'GPU 부하' 인식을 위해 최대 100으로 제한하여 반환
                val = float(res)
                return min(round(val, 1), 100.0)
                
            # 위 방식이 실패할 경우를 대비한 NVIDIA 전용 GPUtil (Fallback)
            if 'GPUtil' in globals():
                gpus = GPUtil.getGPUs()
                if gpus:
                    return round(gpus[0].load * 100, 1)
        except Exception:
            pass
        return 0.0

    def get_cpu_temp(self) -> float:
        """powermetrics에서 수집한 최신 CPU 다이 온도 반환"""
        return self.pm_reader.temp_c

    def get_network_speed(self) -> tuple[float, float]:
        """
        초당 네트워크 다운로드/업로드 속도 (KB/s) 계산
        현재 바이트 카운터와 이전 카운터의 차이를 시간 간격으로 나눔
        """
        if self._prev_net is None:
            return 0.0, 0.0
            
        try:
            curr_net = psutil.net_io_counters()
            curr_time = time.time()
            
            interval = curr_time - self._prev_time
            if interval <= 0.1: # 계산 간격이 너무 짧으면 무시
                return 0.0, 0.0
                
            # 바이트 변화량 계산
            sent_diff = curr_net.bytes_sent - self._prev_net.bytes_sent
            recv_diff = curr_net.bytes_recv - self._prev_net.bytes_recv
            
            # KB/s 단위로 변환 및 반올림
            up_kb = round((sent_diff / 1024.0) / interval, 1)
            down_kb = round((recv_diff / 1024.0) / interval, 1)
            
            # 다음 계산을 위해 상태 업데이트
            self._prev_net = curr_net
            self._prev_time = curr_time
            
            return down_kb, up_kb
        except Exception:
            return 0.0, 0.0

    def collect_all(self) -> dict:
        """전송을 위한 모든 수집 정보 통합 (OS별 분기 포함)"""
        n_i, n_o = self.get_network_speed()
        
        # 운영체제에 따른 GPU 데이터 수집
        if self.os_type == "Windows":
            gpu_u = self.get_gpu_usage_windows()
        else:
            gpu_u = self.get_gpu_usage_macos()

        return {
            "c_u": self.get_cpu_perf(),        # CPU Usage
            "c_t": self.get_cpu_temp(),        # CPU Temp
            "g_u": gpu_u,                      # GPU Usage
            "g_t": 0.0,                        # GPU Temp (기본 0)
            "r_u": self.get_ram_perf(),        # RAM Usage
            "d_u": self.get_disk_perf(),       # Disk Usage
            "n_i": n_i,                        # Network In (Down)
            "n_o": n_o                         # Network Out (Up)
        }
