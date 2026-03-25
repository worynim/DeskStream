# 📝 Release Notes - Stats_Monitor

## [v1.1.0] - 2026-03-26
### 🚀 Added
- **macOS(M2/M3) 정밀 지표 매칭**: `psutil` 및 시스템 파티션 분석을 통해 상용 앱(`mac stats`)과 동일한 수준의 RAM/Disk 수치 동기화 성공.
- **Apple Silicon GPU 실시간 모니터링**: `ioreg` 성능 지표 파싱을 통한 GPU 가동률 추출 로직 구현.
- **실시간 센서 스트림 파서**: 백그라운드에서 `powermetrics` 데이터를 실시간으로 낚아채는(Hooking) 스레드 파서 클래스(`PowerMetricsReader`) 도입 (1Hz 갱신).
- **BLE 통신 인프라 안정화**: UART 서비스 기반의 비동기 전송 프로토콜 구축 및 자동 재접속(Auto-Reconnect) 로직 강화.
- **UI/UX 고도화**: 대형 폰트(`maniac_tr`) 및 슬림 프로그레스 바를 적용한 GPS_DASH 스타일의 세련된 대시보드 레이아웃 적용.
- **디버깅 가시화**: 펌웨어 및 파이썬 클라이언트 전반에 실시간 데이터 수신 상태 로그(Serial/Terminal) 추가.
- **계획 수립**: `PLAN.md`를 통한 차기 개발 옵션 구체화.

---

## [v1.0.0] - 2026-03-26
### ✨ Added
- 프로젝트 초기 아키텍처 수립 및 `metrics.py`, `ble_client.py`, `main.py` 핵심 모듈 기초 구현.
- ESP32-C3 펌웨어(`Stats_Monitor.ino`)의 기초 BLE 서버 구조 설계.
- 프로젝트 핵심 문서화(`README.md`, `RELEASE_NOTES.md`, `PLAN.md`) 완료.

---

## [v0.1.0] - 2026-03-25
### 🐣 Added
- 프로젝트 폴더 생성 (`Stats_Monitor`) 및 기초 구성 요소 준비.
