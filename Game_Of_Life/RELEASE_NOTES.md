# Release Notes - DeskStream Game of Life

## [v1.0.1] ✅ 커밋완료 - 2026-04-30
### 🚀 Improved & Fixed
- **FreeRTOS Button Polling Task**: 메인 루프의 렌더링 블로킹 시간(약 50ms) 동안 짧은 버튼 입력이 무시되는 현상을 완벽히 해결하기 위해, 5ms마다 독립적으로 버튼을 스캔하는 백그라운드 태스크 엔진 도입.
- **FPS 조정**: 시뮬레이션 3단계 속도를 5/15/30에서 5/10/20 FPS로 현실화.

## [v1.0.0] - 2026-04-30
### 🌟 New Features
- **Standalone Engine**: 외부 장치 의존성 없는 ESP32-C3 단독 Game of Life 엔진 구현.
- **Quad-OLED Parallel Driver**: 하드웨어 I2C와 소프트웨어 I2C를 혼합한 4개 디스플레이 병렬 제어 시스템 구축.
- **Interactive UI**: 물리 버튼 4개를 활용한 시뮬레이션 제어 로직 (재생, 일시정지, 단계 진행, 속도 변환, 리셋).
- **Splash Screen**: 부팅 시 "Conway's GAME OF LIFE" 텍스트 애니메이션 및 스타트업 멜로디 추가.

### 🛠 Technical Improvements
- **1-Bit Bitmasking**: 512x64 그리드를 비트 단위로 관리하여 연산 속도 및 메모리 효율성 극대화.
- **Asynchronous I2C Task**: FreeRTOS 태스크를 활용한 비동기 전송으로 메인 루프 지연 방지.
- **Shared Buffer Fix**: 각 디스플레이 인스턴스에 독립적인 메모리 버퍼를 할당하여 화면 간 간섭 문제 해결.
- **Hardware Flip**: 물리적인 하드웨어 장착 방향에 맞춘 180도 화면 반전 기능 적용.

### 🐞 Bug Fixes
- I2C 주소 충돌 및 디바이스 핸들 오할당 문제 해결.
- U8g2 소프트웨어 I2C와 ESP-IDF 하드웨어 I2C 간의 핀 점유 충돌(0x103 Error) 수정.

---
*Initial Release for DeskStream Ecosystem.*
