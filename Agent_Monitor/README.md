# 🤖 Agent_Monitor — AI 개발 에이전트 모니터 (v0.1.0)

Claude Code 세션의 작업 상태를 DeskStream 4-OLED에 실시간으로 보여주는 책상 위 모니터입니다.
OLED 한 장이 세션 하나를 맡아 **생각 중 / 실행 중(도구·명령) / 승인 대기 / 입력 대기 / 완료 / 오류**를 표시하고,
상태가 바뀌면 부저로 알려 줍니다.

> 현재 단계: **P1 — 상태 표시**. 버튼으로 권한 승인/거절은 P2에서 추가합니다. 계획·조사: `PLAN.md`, 훅 검증 결과: [p0_spike/RESULTS.md](./p0_spike/RESULTS.md)

## 구성

```
Claude Code ──HTTP 훅──▶ bridge_py/agent_bridge.py (PC 데몬) ──USB 시리얼(JSON Lines)──▶ DeskStream
                         127.0.0.1:9230                                                  OLED 4 · 버튼 · 부저
```

- 데몬은 모든 훅에 **즉시 빈 응답**을 돌려줍니다. 데몬이나 장치가 꺼져 있어도 Claude Code 작업에는 영향이 없고,
  권한 요청은 평소처럼 터미널 대화상자로 결정됩니다.
- 화면 글꼴이 ASCII 뿐이라 한글 등은 `?`로 표시됩니다.

## 화면

| 상태 | 아이콘 | 표시 |
|:--|:--|:--|
| IDLE | 빈 원 | 대기 |
| THINKING | 점 3개 애니메이션 | 프롬프트 처리 중 |
| RUNNING | 회전 스피너 | 도구 이름 + 명령/파일 요약 |
| APPROVE? | `!` | 권한 요청 — 화면 전체 반전 깜빡임 + 알림음 |
| NEED INPUT | `?` | 질문·입력 대기 — 반전 깜빡임 + 알림음 |
| DONE | 체크 + 굵은 테두리 | 마지막 답변 요약 + 완료음 |
| ERROR | X | API 오류로 턴 종료 + 오류음 |

- 오른쪽 위: 현재 상태가 시작된 뒤 경과 시간. 도구 실패가 있으면 도구 줄 오른쪽에 `E<횟수>`.
- 세션이 5개 이상이면 승인/입력 대기가 아닌 가장 오래된 세션 자리를 새 세션이 씁니다.
- PC 데몬 신호(2초 하트비트)가 6초 동안 없으면 "PC link lost".

## 버튼

| 동작 | 기능 |
|:--|:--|
| BTN i 짧게 | 슬롯 i 의 완료(DONE) 표시를 확인 처리 → IDLE |
| BTN i 길게 | 데몬에 전달만 함 (PC 명령 실행은 P3에서 결정) |

## 사용 방법

### 1. 펌웨어
- 필요 라이브러리: **DeskStreamCore**(저장소 `libraries/`, 루트 README "최초 설정"), **U8g2**
- 보드: `esp32:esp32:makergo_c3_supermini`, USB CDC On Boot = Enabled, CPU 160MHz, Partition = Default

```bash
arduino-cli compile --fqbn "esp32:esp32:makergo_c3_supermini:CDCOnBoot=default,CPUFreq=160,PartitionScheme=default" Agent_Monitor
```

### 2. PC 데몬 (Python 3.10+, macOS에서 검증)

```bash
cd Agent_Monitor/bridge_py
pip install -r requirements.txt
python3 agent_bridge.py run -v          # 보드(Espressif USB)를 자동으로 찾는다. 지정: --serial /dev/cu.usbmodemXXXX
```

> ⚠️ **AgentDeck 등 다른 앱이 보드의 시리얼 포트를 열고 있으면** 업로드도 데몬 연결도 실패합니다. 그 앱을 먼저 종료하세요.

### 3. Claude Code 훅 연결

전역 설정을 건드리지 않고 한 세션에만 적용:
```bash
python3 agent_bridge.py settings > ~/agent_monitor_hooks.json
claude --settings ~/agent_monitor_hooks.json
```

모든 세션에 적용 (`~/.claude/settings.json`에 추가, 기존 훅 보존·백업 생성):
```bash
python3 agent_bridge.py install-hooks
python3 agent_bridge.py uninstall-hooks   # 추가한 항목만 제거
```

### 4. 화면 확인 (Claude Code 없이)
```bash
python3 demo_hooks.py        # 슬롯 4개를 실행 중 / 승인 대기 / 완료 / 생각 중으로
python3 demo_hooks.py --end
```

## 파일 구성

| 파일 | 내용 |
|:--|:--|
| `Agent_Monitor.ino` | 시리얼 수신 · 버튼 · 알림음 · 100ms 주기 렌더링 |
| `agent_state.h/.cpp` | 슬롯 모델과 메시지 해석 (순수 C++, 호스트 테스트) |
| `agent_ui.h/.cpp` | 슬롯 화면·오프라인 화면 그리기 |
| `config.h` | 링크 timeout · 프레임 주기 · 깜빡임 주기 등 |
| `bridge_py/agent_bridge.py` | 데몬 (훅 서버 · 송신 루프 · 훅 설정 명령) |
| `bridge_py/sessions.py` | 훅 이벤트 → 세션 상태 · 슬롯 배정 |
| `bridge_py/protocol.py` | 시리얼 메시지 형식 (프로토콜 설명 포함) |
| `bridge_py/transport.py` | USB 시리얼 (자동 탐색 · 재연결 · 쓰기 timeout) |
| `bridge_py/hooks_config.py` | 훅 설정 생성 · 설치 · 제거 |
| `bridge_py/demo_hooks.py` | 화면 확인용 가짜 훅 |
| `p0_spike/` | P0 훅 동작 검증 도구와 결과 |

## 테스트

```bash
cd Agent_Monitor/bridge_py && python3 -m unittest discover -s tests -v      # 데몬 16건

# 펌웨어 상태 해석 (저장소 루트에서)
L=libraries/DeskStreamCore/src
g++ -std=c++17 -IAgent_Monitor -I$L Agent_Monitor/test/test_agent_state.cpp Agent_Monitor/agent_state.cpp \
    $L/util/ds_json_lite.cpp -o /tmp/test_agent_state && /tmp/test_agent_state
```
