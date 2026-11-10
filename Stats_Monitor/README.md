# 🖥️ Stats_Monitor

PC/Mac의 시스템 지표(CPU, GPU, RAM, SSD, 온도, 소비전력)를 실시간으로 표시해주는 데스크 가젯 프로젝트입니다.

https://www.youtube.com/playlist?list=PL2He47zwR3XjQv_0kjdzW76SOO_vlLyVU

## 🛠️ 하드웨어 구성
- **MCU**: ESP32-C3
- **Display**: 4 x SSD1306 OLED (128x64)
- **Interface**: I2C (HW 2개, SW 2개)

## 📡 주요 기능
- CPU/GPU 사용량 및 온도 실시간 모니터링
- RAM/SSD 사용 현황 및 네트워크 전력 트래픽 표시
- **화면 180도 회전(Flip)**: 물리적 배치에 따른 화면 순서 자동 반전 기능 포함
- **OLED 밝기 조절**: 4단계(사용자 정의) 밝기 제어 및 비주얼 가이드 제공
- **설정 저장(NVS)**: 마지막 회전 상태 및 밝기를 플래시에 저장하여 재부팅 시 복원
- **부저 피드백**: 버튼 조작 및 시스템 시작 시 청각적 피드백 제공
- **고속 렌더링 최적화**: Dirty Checking 및 Fast GPIO 기반의 부드러운 UI 갱신

## 🕹️ 하드웨어 컨트롤
| 버튼 | 동작 (Short Press) | 기능 설명 |
| :--- | :--- | :--- |
| **BTN 1** | 화면 180도 회전 | 디스플레이를 뒤집고, 데이터 출력 순서를 반전(4-3-2-1) 시킵니다. |
| **BTN 2** | 밝기 조절 | OLED 밝기를 4단계(1 > 50 > 100 > 240)로 순환 조절합니다. |
| **BTN 3** | (미할당) | 향후 기능 확장용 버튼입니다. |
| **BTN 4** | 도움말 토글 | 각 버튼의 기능과 현재 설정 상태(각도, 밝기)를 화면에 표시합니다. |

## 🐍 Python 클라이언트 설정
PC의 시스템 지표를 BLE를 통해 ESP32로 전송하기 위해 다음 설정이 필요합니다.

> **펌웨어 빌드**: 화면 전송·버튼·부저는 공통 라이브러리 [DeskStreamCore](../libraries/DeskStreamCore/) — 빌드 전 저장소 루트에서 `bash scripts/link-arduino-lib.sh` 1회를 씁니다 (v1.6.0~).

### 1. 필수 라이브러리 설치
Python 3.10 이상 환경에서 다음 명령어를 실행하여 필요한 패키지를 설치합니다.
```bash
pip install -r client_py/requirements.txt
```

**설치되는 주요 패키지:**
- `psutil`: 시스템 지표(CPU, RAM, Disk, Network) 수집
- `GPUtil`: GPU 정보(NVIDIA) 수집
- `bleak`: BLE(Bluetooth Low Energy) 통신
- `Pillow`: 시스템 트레이 아이콘 이미지 생성
- `pystray`: 시스템 트레이 대화형 아이콘 관리

### 2. 클라이언트 실행
```bash
python client_py/main.py
```

### 3. 단독 실행 파일 빌드 (PyInstaller)
`client_py/stats_monitor.spec` 으로 Python 설치 없이 더블클릭으로 실행되는 앱을 만듭니다. 다른 OS용 크로스 빌드는 되지 않으므로 각 OS에서 빌드합니다.
```bash
cd client_py
pip install -r requirements.txt pyinstaller
pyinstaller stats_monitor.spec --noconfirm
```
- **macOS (arm64)**: `dist/StatsMonitor.app` — 메뉴 막대 전용(Dock 아이콘 없음). 처음 실행 시 블루투스 권한을 허용해야 합니다.
- **Windows (x64)**: `dist/StatsMonitor.exe` — 콘솔 창 없는 단일 실행 파일.
- 서명하지 않은 앱이라 macOS에서는 처음 한 번 **시스템 설정 → 개인정보 보호 및 보안 → "그래도 열기"** 가 필요합니다.
- 터미널에서 `.app` 안의 실행 파일을 직접 띄우면 블루투스 권한 주체가 터미널로 잡혀 크래시할 수 있습니다. 반드시 Finder 더블클릭이나 `open` 으로 실행하세요.

### 4. 배포 (GitHub Release)
`stats_monitor.spec` 의 `VERSION` 과 같은 버전으로 태그를 푸시하면 [Build PC clients](../.github/workflows/build-clients.yml) 워크플로가 macOS·Windows 실행 파일을 빌드해 Release 에 올립니다.
```bash
git tag Stats_Monitor-client-v2.4.1
git push origin Stats_Monitor-client-v2.4.1
```
- 산출물: `StatsMonitor-<버전>-macos-arm64.zip`, `StatsMonitor-<버전>-windows-x64.exe`
- 버전을 올릴 때는 spec 의 `VERSION` 과 `main.py` 트레이 툴팁의 버전을 함께 바꿉니다.

## 📋 사용 방법
1. PC에서 데이터를 전송할 클라이언트 프로그램 실행 (`python client_py/main.py`)
2. ESP32-C3 보드 전원 연결 시 자동으로 BLE 연결 후 4개의 OLED에 데이터 표시
3. 필요한 경우 하드웨어 버튼을 통해 화면 방향이나 밝기 조절
