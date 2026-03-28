# 🚀 DeskStream Release Notes

## [v1.2.0] - 2026-03-27
### Enhanced (Stats_Monitor 프로젝트 고도화)
- **실시간 네트워크 통합 모니터링**: 의미가 고정적인 전력 수치 대신, 생동감 넘치는 다운로드/업로드 속도 실시간 측정 로직 도입.
- **OLED 렌더링 물리 엔진 최적화**: 
  - **Shadow Buffer** 기반의 Dirty Checking 기법을 수동으로 구현하여 I2C 전송 대역폭 효율 극대화.
  - ESP32-C3 하드웨어 레지스터 직접 제어(Register-direct)를 통한 소프트웨어 I2C 성능 획기적 개선.
  - I2C 버스 클럭 **800kHz** 상향 조정.
- **가독성 기반 UI/UX 리뉴얼**: 
  - 7-Segment 스타일 및 `profont15` 적용으로 세련된 대시보드 시각화 구현.
  - 모든 상태 지표를 '라벨+수치+게이지 바' 레이아웃으로 통일하여 통합된 디자인 경험 제공.
- **시스템 모니터 트레이 아이콘 최적화**: macOS 투명 상태 바에 최적화된 화이트 실루엣 아이콘 적용.
- **코드 무결성 강화**: 모든 프로젝트 소스 코드 및 최적화 기법에 대한 상세 기술 주석 추가 완료.

---

## [v1.1.0] - 2026-03-26
### Added (Stats_Monitor 프로젝트 강화)
- **macOS(M2/M3) 정밀 지표 매칭**: `psutil` 및 시스템 파티션 분석을 통해 상용 앱(`mac stats`)과 동일한 수준의 RAM/Disk 수치 동기화 성공.
- **Apple Silicon GPU 실시간 모니터링**: `ioreg` 성능 지표 파싱을 통한 GPU 가동률 추출 로직 구현.
- **실시간 센서 스트림 파서**: 백그라운드에서 `powermetrics` 데이터를 실시간으로 낚아채는(Hooking) 스레드 파서 클래스(`PowerMetricsReader`) 도입 (1Hz 갱신).
- **BLE 통신 인프라 안정화**: UART 서비스 기반의 비동기 전송 프로토콜 구축 및 자동 재접속(Auto-Reconnect) 로직 강화.
- **UI/UX 고도화**: 대형 폰트(`maniac_tr`) 및 슬림 프로그레스 바를 적용한 GPS_DASH 스타일의 세련된 대시보드 레이아웃 적용.
- **디버깅 가시화**: 펌웨어 및 파이썬 클라이언트 전반에 실시간 데이터 수신 상태 로그(Serial/Terminal) 추가.
- **계획 수립**: `PLAN.md`를 통한 차기 개발 옵션 구체화.

---

## [v1.0.0] - 2026-03-26
### Added
- DeskStream 프로젝트 초기 구성 완료.
- 4가지 주요 프로그램 통합 관리:
  - `Smart_Info_Station`: 스마트 정보 스테이션 (시계, 날씨, 증시 등)
  - `GPS_DASH`: GPS 기반 드라이빙 대시보드
  - `Sound Visualizer 4 OLEDs`: 마이크 입력 기반 고성능 오디오 비주얼라이저
  - `OLED_Wide_test`: 4개 OLED 통합 캔버스 제어 테스트
  - `Stats_Monitor`: 시스템 상태 모니터 (CPU, GPU, RAM, SSD 등)
- 하드웨어 명세서(`hardware.md`) 및 회로도 연결 정보 정리.
- 루트 디렉토리 `README.md` 생성 및 프로젝트 가이드라인 구축.
