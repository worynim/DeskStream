# Release Notes - Agent_Monitor

## [v0.3.0] - 2026-11-10
### ✨ P3 — 세션 상세 화면 · 음소거/무음 시간대 · PC 명령
- **버튼 배치 변경**: 개요에서 짧게 = 그 슬롯 상세 화면(+완료 확인), 길게 = PC 명령.
- **상세 화면**: 이름·상태·경과·권한 모드·하위 에이전트/오류 수 · 최근 도구 4개(`>` 실행 중 `+` 성공 `!` 실패) · 마지막 답변 120자. BTN1 개요 · BTN2 음소거 · BTN3/4 이전/다음 세션. 60초 무입력·세션 종료 시 개요로.
- **음소거**: 상세 화면 BTN2, 보드 NVS 에 저장 (`ds::ConfigStore`). 버튼 클릭음은 유지.
- **PC 명령 (`commands.py`)**: `~/.deskstream/agent_monitor.toml` 의 `focus`(앱 앞으로) · `shell`(명령 실행, timeout) · `keys`(텍스트·키 입력, macOS 손쉬운 사용 권한). 장치는 버튼 번호만 보냄, 버튼마다 파일 재읽기, 다른 사용자 쓰기 권한이면 거부. 결과는 버튼 화면 아래 띠(RUN/OK/FAIL + exit 코드).
- **무음 시간대**: 설정 `[quiet]` 시간에는 알림음 메시지를 보내지 않음, 장치에 아이콘 표시.
- 데몬 `init-config`, `run --config` 추가. Python 3.11+ (tomllib).
- 데모 스크립트: 승인 대기 상태를 PermissionRequest 대신 Notification 으로 표시 (P2 이후 PermissionRequest 는 응답을 붙잡음).
- 크기: 445,034 B (33%). 테스트: 데몬 39건, 펌웨어 detail·cmd·quiet 해석.
- 실기 확인: 상세 화면 이동·음소거·무음 아이콘, PC 명령 focus/shell 성공·실패·미설정. **keys 명령은 실기 미확인**(단위 테스트만).

## [v0.2.0] - 2026-11-10
### ✨ P2 — 장치 버튼으로 권한 승인/거절
- **승인 화면**: 권한 요청이 오면 4화면이 한 장의 승인 화면으로 바뀐다 — 머리줄(APPROVE? · 세션 · 도구 · n/q + 대기 시간, 반전 깜빡임), 명령 요약 2줄, 버튼 라벨. BTN1 승인 · BTN2 다음 요청 · BTN3 거절 · BTN4 나중에(터미널에 맡김). 새 요청 직후 0.7초는 버튼 무시.
- **데몬 `approvals.py`**: 권한 요청 대기열. PermissionRequest 응답을 최대 290초(훅 timeout 300초) 붙잡고 장치 결정을 기다린다. 장치 미연결·시간 초과·나중에 → 빈 응답(터미널 결정). 자동 허용·자동 거절 없음.
- **터미널 선결정 정리**: 같은 세션의 PostToolUse(같은 도구·입력) 또는 UserPromptSubmit·Stop·StopFailure·SessionEnd 가 오면 장치 승인 화면을 닫는다.
- 프로토콜: `perm`(데몬→장치), `decide`(장치→데몬) 추가. 시리얼 줄 버퍼 256 → 512 B.
- 크기: 427,896 B (32%). 테스트: 데몬 27건(승인 대기열·결정·시간 초과·선결정 정리), 펌웨어 perm 해석.
- 실기 확인: 실제 Claude Code 세션에서 APPROVE(실행됨) · DENY(실행 안 됨) · LATER(터미널 대화상자로) 3가지.

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
