# Screen_To_OLEDs (Screen Streamer)

모니터(mac)의 화면을 실시간으로 캡처하여 DeskStream 하드웨어(4개의 OLED 매트릭스)로 스트리밍하는 프로젝트입니다.

## 🚀 주요 기능

### macOS 스트리머 (`screen_streamer.py`)
- **실시간 화면 캡처**: macOS에 최적화된 MSS 라이브러리를 사용한 고성능 캡처.
- **다양한 이진화 알고리즘**:
  - `Threshold`: 정해진 기준값으로 흑백 변환.
  - `Dithering`: Floyd-Steinberg 알고리즘을 사용한 부드러운 하프톤 표현.
- **드래그 가능한 오버레이**: 4개 선 윈도우 방식을 사용하여 macOS의 투명 창 캡처 버그를 회피하면서 직관적으로 위치 선택 가능.
- **레이아웃 지원**: 
  - `Landscape (512x64)`: 가로 4열 배치.
  - `Portrait (64x512)`: 세로형 배치 (파이썬에서 자동 회전 처리).
- **macOS 특화 기능**: `replayd` 메모리 관리 로직을 포함하여 장시간 스트리밍 시에도 안정성 확보.
- **실시간 설정 적용**: 스트리밍 중에도 B&W 모드, 대비(Contrast), FPS, 오버레이 가시성 등을 즉시 조절 가능.

### ESP32 펌웨어
- **고속 UDP 수신**: 프레임을 1024바이트 청크로 분할 수신하여 4096바이트 1-bit 비트맵 복원.
- **Dirty-Tile 렌더링**: 변경된 영역만 OLED에 업데이트하여 고속 프레임 레이트 달성.
- **WiFiManager 연동**: 편리한 AP 설정을 지원하며, 부팅 시 AP 정보 표시.
- **비차단 부저**: 동작 상태를 부드러운 소리로 안내.

## 🛠 설치 및 사용 방법

### macOS 환경 (Python)
1. 필요한 라이브러리 설치:
   ```bash
   pip install Pillow mss numpy
   ```
2. 스트리머 실행:
   ```bash
   python screen_streamer.py
   ```
3. ESP32의 IP를 입력하거나 `Scan` 버튼을 눌러 자동으로 찾습니다.
4. `START STREAMING` 버튼을 눌러 스트리밍을 시작합니다.

### 하드웨어 환경 (ESP32)
1. `Screen_To_OLEDs` 폴더의 `.ino` 파일을 Arduino IDE 또는 PlatformIO로 업로드합니다.
2. 부팅 시 OLED에 표시되는 WiFi AP(`ESP_SCREEN_OLED`)에 접속하여 네트워크 설정을 완료합니다.
3. 연결 성공 시 표시되는 IP 주소를 스트리머에 입력합니다.

## 🕹 하드웨어 제어 (버튼 기능)
- **BTN1**: 롱 프레스 - 화면 180도 회전 (Flip) / 숏 프레스 - (Reserved)
- **BTN2**: 숏 프레스 - **밝기 4단계 순환 조절** / 롱 프레스 - **도움말 화면 표시**
- **BTN3**: (Not Used)
- **BTN4**: 숏 프레스 - 도움말/대기 화면 종료 (Exit) / 롱 프레스 - (Reserved)

---
**개발**: Google Deepmind Antigravity Pair Programming
