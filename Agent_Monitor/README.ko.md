[English](README.md) | **한국어**

# 🤖 Agent_Monitor — AI 개발 에이전트 모니터 (v0.8.2)

Claude Code 세션의 작업 상태를 DeskStream 4-OLED에 실시간으로 보여주는 책상 위 모니터입니다.
OLED 한 장이 세션 하나를 맡아 **생각 중 / 실행 중 / 승인 대기 / 입력 대기 / 완료 / 오류**를 표시하고, 상태가 바뀌면 부저로 알려 줍니다.

> 현재 단계: **P4 — BLE · 서명 · 트레이 · 웹 플래셔** 완료. 계획·조사: `PLAN.md`, 훅 검증 결과: [p0_spike/RESULTS.md](./p0_spike/RESULTS.md)

## ✨ 핵심 기능

- **세션 상태 표시**: 세션마다 화면 하나, 도구 이름과 명령 요약, 경과 시간
- **장치 버튼으로 권한 승인/거절** — 터미널 대화상자와 함께 기다리며 먼저 답한 쪽이 결정 (자동 허용 없음)
- **PC 명령**: 버튼을 길게 눌러 앱 앞으로 · 셸 명령 · 키 입력 실행
- **연결**: USB 또는 BLE(USB로 등록한 키로 서명), 메뉴 막대 트레이 앱
- 빈 화면에 Claude Code 사용량(5시간·주간) 표시, 데몬이 꺼져 있어도 Claude Code 동작에 영향 없음

```
Claude Code ──HTTP 훅──▶ PC 데몬(bridge_py) ──USB 또는 BLE──▶ DeskStream
```

## 🎮 버튼

| 화면 | 동작 | 기능 |
|:--|:--|:--|
| 개요 | BTN i 짧게 / 길게 | 슬롯 i 상세 화면 / PC 명령 i 실행 |
| 상세 | BTN1 · BTN2 | 개요로 · 세션 닫기 |
| 상세 | BTN3 · BTN4 | 마지막 답변 이전 · 다음 페이지 |
| 승인 | BTN1 · BTN2 · BTN3 · BTN4 | **승인** · 다음 요청 · **거절** · 나중에(터미널에서 결정) |

## 🚀 설치

1. **펌웨어** — [웹 플래셔](https://worynim.github.io/DeskStream/)에서 Agent Monitor를 고르거나 직접 빌드합니다.
2. **PC 데몬** — 웹 플래셔에서 macOS(Apple Silicon)·Windows(x64) 앱을 받거나, Python으로 실행합니다.

   ```bash
   cd Agent_Monitor/bridge_py
   pip install -r requirements.txt
   python3 agent_bridge.py tray
   ```

3. **서명 키 등록** — 처음 한 번 USB로 연결해 실행합니다 (`--transport usb`). 이후에는 BLE로 씁니다.
4. **Claude Code 훅 연결** — 트레이 메뉴의 "Claude Code 훅 설치" 또는 `python3 agent_bridge.py install-hooks`.

## 📖 자세히

화면 구성, BLE 보안, 설정 파일(음소거·밝기·사용량 조회·PC 명령), Windows 안내, 파일 구성, 테스트는 [GUIDE.ko.md](GUIDE.ko.md)에 있습니다.
