# 🖥️ Stats_Monitor

PC/Mac의 시스템 지표(CPU, GPU, RAM, SSD, 온도, 소비전력)를 실시간으로 표시해주는 데스크 가젯 프로젝트입니다.

## 🛠️ 하드웨어 구성
- **MCU**: ESP32-C3
- **Display**: 4 x SSD1306 OLED (128x64)
- **Interface**: I2C (HW 2개, SW 2개)

## 📡 주요 기능
- CPU 사용량 및 온도 모니터링
- GPU 사용량 및 온도 모니터링
- RAM/SSD 사용 현황 표시
- 시스템 실시간 소비전력 확인

## 📋 사용 방법
1. PC에서 데이터를 전송할 클라이언트 프로그램 실행
2. ESP32-C3 보드에서 데이터를 수신하여 4개의 OLED에 분산 표시
