# P0 스파이크 결과 — Claude Code 훅 동작 검증

- 일시: 2026-11-10 · Claude Code **v2.1.318** · macOS
- 방법: `claude --settings hooks.settings.json` 으로 훅을 이 세션에만 적용(전역 `~/.claude/settings.json` 변경 없음).
  `hook_logger.py`가 `127.0.0.1:9230`에서 받아 `logs/*.jsonl`에 기록.
  - run1: 사용자가 터미널에서 수동 조작 (`--perm ask`)
  - run2: `drive.exp`(expect)로 대화형 claude 자동 조작 + `hook_logger.py --seq allow,deny,pass,timeout`

## 파일

| 파일 | 내용 |
|:--|:--|
| `hook_logger.py` | 훅 수신·기록 서버. PermissionRequest 를 allow/deny/pass/보류로 응답 |
| `hooks.settings.json` | 훅 14종 → `http://127.0.0.1:9230/hooks/<Event>` (PermissionRequest timeout 35초, 나머지 5초) |
| `drive.exp` | 시나리오 S1~S4 + idle 대기 자동 실행 스크립트 (`sandbox/`에서 실행) |
| `logs/`, `sandbox/` | 실행 산출물 (git 제외) |

## 권한 요청(PermissionRequest) 결과

| 시나리오 | 훅 응답 | 결과 |
|:--|:--|:--|
| S1 | 즉시 `allow` | ✅ 바로 실행(PostToolUse 0초), 터미널 대화상자 없음 |
| S2 | 즉시 `deny` + message | ✅ 실행 안 됨, Claude가 턴 종료(Stop) |
| S3 | 빈 본문 200 (`pass`) | ✅ 터미널 권한 대화상자가 뜸 (6초 뒤 `Notification[permission_prompt]`) |
| S4 | 45초 보류 (훅 timeout 35초 초과) | ✅ 보류 중에도 터미널 대화상자가 뜨고, 오류 표시 없음. 늦은 응답은 무시됨 |
| run1 | 응답 보류 중 사용자가 터미널에서 승인 | ✅ 터미널 승인으로 그대로 실행됨 |

## 핵심 발견

1. **훅과 터미널 대화상자가 동시에 대기한다 (먼저 답한 쪽이 이김).**
   훅이 응답을 붙잡고 있어도 터미널 대화상자가 뜨고, 터미널에서 답하면 진행된다.
   → 계획의 "보류 시간을 짧게 + '터미널로' 버튼" 대응이 불필요. 장치는 승인 화면만 정리하면 된다.
2. **터미널에서 먼저 결정된 경우를 장치가 알아야 한다.** PermissionRequest 에는 `tool_use_id`가 없다.
   같은 `session_id`의 다음 이벤트(PostToolUse / Stop / UserPromptSubmit / 다음 PermissionRequest)가 오면 대기 중인 승인을 닫는 방식으로 처리한다.
   (Claude Code가 터미널 결정 시 HTTP 연결을 즉시 끊는지는 이번에 판별하지 못함 — 35초 timeout 과 구분 안 됨)
3. **SessionStart 는 `http` 훅을 실행하지 않는다.** `command` 훅으로는 정상 수신 (`source: startup`).
   → SessionStart 만 command 훅(curl 등)으로 데몬에 전달하거나, 세션의 첫 이벤트로 세션을 등록한다.
4. **`Stop` 직후 `SubagentStop`(agent_type 빈 값)이 매 턴 따라온다** (SubagentStart 없음). 내부 백그라운드 작업으로 보이며 상태 표시에서 무시한다.
5. **알림 타이밍**: `permission_prompt` ≈ 대화상자 표시 6초 뒤, `idle_prompt` ≈ 입력 대기 60초 뒤.
6. **auto 모드에서는 PermissionRequest 가 오지 않는다** (분류기가 바로 결정). manual/default 모드에서만 장치 승인이 의미 있음.
7. **유용한 필드**
   - 모든 이벤트: `session_id`, `prompt_id`(턴 단위 묶음), `permission_mode`, `cwd`
   - `Stop.last_assistant_message`: 완료 메시지 원문 → 장치에 요약 표시 가능 (한글 포함)
   - `PostToolUse.duration_ms`, `tool_response`
   - `SessionEnd.reason` (예: `prompt_input_exit`)
8. **"항상 허용"용 `permission_suggestions`** 는 Bash 요청에서 `addDirectories`, `setMode(acceptEdits)` 만 제공했다.
   명령 단위 허용 규칙은 제안되지 않으므로 "항상 허용" 버튼은 규칙을 직접 만들어야 한다 → 범위 결정 필요.

## 미검증

- Claude Code **데스크톱 앱(Code 탭)** 세션에서의 훅 동작 (이번은 CLI 만)
- 터미널에서 먼저 결정했을 때 훅 HTTP 연결이 즉시 끊기는지
- Notification 의 `elicitation_dialog`, `agent_needs_input` (질문 대기) 실제 발생 조건

## 재현 방법

```bash
cd Agent_Monitor/p0_spike
python3 hook_logger.py --seq allow,deny,pass,timeout     # 탭 1
cd sandbox && expect ../drive.exp                        # 탭 2 (로그인된 셸에서, 약 4분)
```
