# Release Notes - Agent_Monitor

## [v0.1.0] - 2026-11-10
### ✨ 첫 버전 — P1 상태 표시
- **PC 데몬 `bridge_py/agent_bridge.py`**: Claude Code HTTP 훅(127.0.0.1:9230)을 받아 세션 상태를 USB 시리얼(JSON Lines)로 DeskStream에 보낸다. 모든 훅에 즉시 빈 응답 → 데몬·장치가 꺼져 있어도 Claude Code 동작에 영향 없음.
  - 세션 상태: idle / think / run / approve / input / done / error. 세션 4개까지 OLED 슬롯 배정.
  - P0 관찰 반영: SessionStart 는 curl command 훅(http 훅 미실행), `agent_type` 빈 SubagentStop 무시, 완료 뒤 `idle_prompt` 무시.
  - 훅 설정: `settings`(claude --settings 용 출력) · `install-hooks` · `uninstall-hooks`(기존 훅 보존·백업).
  - 시리얼 쓰기 timeout 1초 + 송신 스레드 한 곳에서만 쓰기 (쓰기가 막혀 데몬이 멈추던 문제를 pty 테스트에서 발견해 수정).
- **펌웨어**: OLED 1장 = 세션 1개. 상태 아이콘·애니메이션, 승인/입력 대기 반전 깜빡임, 완료 테두리, 경과 시간, 알림음(완료·주의·오류), PC 링크 끊김 표시. 버튼 짧게 = 완료 확인, 길게 = 데몬 전달(P3 예정).
- 크기: 425,372 B (32%).
- 테스트: 데몬 단위 테스트 16건(P0 실제 훅 기록 재생 포함), 펌웨어 상태 해석 호스트 테스트, pty 가짜 장치 종단 간 테스트(훅 응답 1ms 미만).
- 실기 확인: 데모 세션 4개 화면·버튼, 실제 Claude Code 세션(시작 → 실행 → 승인 대기 → 터미널 승인 → 완료).
