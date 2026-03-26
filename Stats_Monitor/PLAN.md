# 📋 Stats_Monitor Project Plan

이 보고서는 PC/Mac의 시스템 정보를 실시간으로 수집하여 DeskStream 하드웨어로 전송하고 표시하는 프로젝트의 통합 계획을 담고 있습니다.

## 1. 프로젝트 개요
- **목표**: 상시 켜져 있는 외부 디스플레이를 통해 PC의 자원 사용량(CPU, GPU, RAM, SSD, 온도, 네트워크)을 모니터링함.
- **핵심 철학**: **"Dumb Display, Smart Client"**. 하드웨어는 들어오는 데이터를 렌더링만 하고, 모든 데이터 가공 및 로직은 파이썬 프로그램에서 처리하여 확장성을 극대화함.

## 2. 시스템 아키텍처

### 💻 PC/Mac 클라이언트 (Python)
- **운영체제**: macOS (Apple Silicon 최적화 완료), Windows 지원 예정.
- **실행 방식**: 시스템 트레이(System Tray) 인터페이스를 가진 백그라운드 서비스.
- **주요 기술**:
    - `psutil`: CPU, RAM, Disk, Network 트래픽 수집.
    - `bleak`: macOS 통합 BLE 통신 엔진.
    - `powermetrics` (Sudo): M-series 칩셋의 전력/온도 정밀 파싱.
    - `pystray`: 투명 아이콘 기반의 시스템 트레이 관리.

### 📡 통신 프로토콜 (Bluetooth BLE)
- **방식**: **BLE UART 서비스** (Nordic UART Service 호환).
- **데이터 포맷**: **JSON (Compact)**.
- **최적화**: 필드명을 최소화(`n_i`, `n_o` 등)하여 패킷 크기 최적화 및 레이턴시 감소.

### 📟 하드웨어 (ESP32-C3)
- **역할**: BLE 서버로 대기하며, 수신된 JSON을 파싱하여 4개의 OLED에 데이터 매핑.
- **고속 렌더링**: **Dirty Checking (Smart Flush)** 엔진을 탑재하여 변경된 페이지만 선택적으로 업데이트 (I2C 부하 80% 감소).

## 3. 데이터 포맷 정의 (JSON Schema)
```json
{
  "c_u": 15,    // CPU Usage (%)
  "c_t": 48,    // CPU Temp (℃)
  "g_u": 20,    // GPU Usage (%)
  "r_u": 62,    // RAM Usage (%)
  "d_u": 40,    // Disk Usage (%)
  "n_i": 1250,  // Network Download (KB/s)
  "n_o": 340    // Network Upload (KB/s)
}
```

## 4. 🚀 진행 상황 요약 (Phase 2 고도화 완료)

### ✅ 완료된 작업 (2026-03-27 기준)
1. **네트워크 모니터링 전환**: 의미가 고정적인 전력(W) 지표 대신, 생동감이 넘치는 **실시간 네트워크 업/다운로드(KB/s, MB/s)** 수집 및 표시 로직으로 전면 교체 완료.
2. **OLED 전송 엔진 최적화**: 
    - **800kHz I2C Clock** 상향 조정.
    - **Fast GPIO bit-banging** 적용 (Reg-direct access).
    - **Shadow Buffer 기반 Dirty Checking** 도입 (변경된 데이터 페이지만 전송).
3. **UI 디자인 통일**: 네트워크 화면의 디자인을 메모리/디스크 화면과 동일한 레이아웃(Label + Value + Progress Bar)으로 통일하여 시각적 완성도 향상.
4. **트레이 아이콘 시인성 개선**: 배경 투명화 및 화이트 모니터 실루엣 디자인 적용 (macOS 다크 모드 완벽 대응).
5. **MetricKit 기술 검토**: Apple MetricKit 검토 결과, 실시간성(24시간 집계 방식) 부족으로 현재의 실시간 수집 방식을 유지하기로 결정.

## 5. 💡 다음 단계 제안 (Future Roadmap)

현재 시스템은 안정적이고 빠른 속도로 동작하고 있습니다. 다음 단계로 고려해볼 수 있는 기능들은 다음과 같습니다:

1. **실시간 그래프 (Sparklines) 도입**
   - OLED의 남는 영역에 지난 30초간의 CPU/Network 변화량을 선 그래프(Sparklines)로 그려넣어 데이터의 추세를 한눈에 파악.
   
2. **화면 구성 커스터마이징 (UI Slots)**
   - 파이썬 클라이언트에서 특정 화면에 어떤 데이터를 보낼지 설정(e.g. 1번 화면에 시계 표시 등)할 수 있는 가변형 레이아웃 기능.

3. **자동 실행 (Launch at Login)**
   - macOS 부팅 시 파이썬 프로그램이 자동으로 알림 영역에 실행되도록 `.plist` 가이드 제공 및 설정 기능 추가.

4. **멀티 플랫폼 지원 (Windows)**
   - `GPUtil` 및 Windows 전용 센서 라이브러리를 통합하여 윈도우 환경에서도 동일한 하드웨어를 사용할 수 있도록 호환성 확보.

---
*최종 업데이트: 2026-03-27*
