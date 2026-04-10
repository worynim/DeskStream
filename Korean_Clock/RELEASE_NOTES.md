# Release Notes - Korean Clock

## [v1.1.0] - 2026-04-10
### Character-Based Font Engine & Studio
- **낱자(낱자) 기반 폰트 최적화**: 134개의 단어 이미지 대신 **28개의 고유 낱자** 저장 방식으로 전환하여 메모리 및 업로드 효율 극대화
- **지능형 레이아웃 엔진**:
    - **오전/오후**: 화면 정중앙 배치
    - **시, 분, 초**: "시/분/초" 단위를 우측 끝에 고정하고, 앞의 숫자들을 남은 공간의 중앙에 배치하는 가변 정렬 구현
- **Font Studio v2 (Web)**: 폰트 파일(.ttf, .otf)을 웹 서버(`/`)에서 직접 시계용 낱자 비트맵으로 변환 및 일괄 업로드 기능 제공
- **Fixed Graphics Noise**: U8g2 호환 가로형(Horizontal MSB) 인코딩 적용으로 출력 품질 개선

## [v1.0.0] - 2026-04-10
### Initial Release (MVP)
- **Core**: NTP 시간 동기화 및 대한민국 표준시(KST) 적용
- **Korean Logic**: 숫자를 한글로 변환하는 엔진 구현
- **High-Performance Display**: 1MHz I2C 전송 및 병렬 태스크 가속
