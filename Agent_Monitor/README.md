# 🤖 Agent_Monitor — AI 개발 에이전트 모니터 (v0.4.0)

Claude Code 세션의 작업 상태를 DeskStream 4-OLED에 실시간으로 보여주는 책상 위 모니터입니다.
OLED 한 장이 세션 하나를 맡아 **생각 중 / 실행 중(도구·명령) / 승인 대기 / 입력 대기 / 완료 / 오류**를 표시하고,
상태가 바뀌면 부저로 알려 줍니다. 권한 요청은 **장치 버튼으로 승인/거절**하고, 버튼을 길게 눌러 **PC 명령**(앱 앞으로 · 셸 명령 · 키 입력)을 실행할 수 있습니다. USB 케이블 또는 **BLE 무선**으로 연결되고, 메뉴 막대 **트레이 아이콘**으로 실행할 수 있습니다.

> 현재 단계: **P4 — BLE · 서명 · 트레이 · 웹 플래셔** 완료. 계획·조사: `PLAN.md`, 훅 검증 결과: [p0_spike/RESULTS.md](./p0_spike/RESULTS.md)

## 구성

```
Claude Code ──HTTP 훅──▶ bridge_py/agent_bridge.py (PC 데몬) ──USB 시리얼 또는 BLE(JSON Lines)──▶ DeskStream
                         127.0.0.1:9230                                                          OLED 4 · 버튼 · 부저
```

- 연결은 **USB 우선, 없으면 BLE**("DeskStream Agent")입니다. BLE 사용 중 USB가 꽂히면 USB로 바꿉니다.
- Claude Code CLI 와 **데스크톱 앱(Code 탭) 세션** 모두 훅을 보냅니다 (전역 훅 설치 시).

- 상태 훅에는 **즉시 빈 응답**을 돌려줍니다. 데몬이나 장치가 꺼져 있어도 Claude Code 작업에는 영향이 없습니다.
- 권한 요청(PermissionRequest)은 장치가 연결돼 있을 때만 최대 **290초** 응답을 붙잡고 버튼 결정을 기다립니다.
  그동안에도 터미널 대화상자가 함께 떠 있어 **먼저 답한 쪽**이 결정합니다.
  장치 미연결 · 시간 초과 · LATER 이면 빈 응답 → 터미널에서 결정합니다 (**자동 허용·자동 거절 없음**).
- 터미널에서 먼저 결정하면 같은 세션의 다음 이벤트(도구 실행 완료·턴 종료 등)로 장치 승인 화면이 닫힙니다.
- 화면 글꼴이 ASCII 뿐이라 한글 등은 `?`로 표시됩니다.

### BLE 보안 — USB 로 키 등록 + 서명

BLE 는 근처 누구나 접속할 수 있어, 같은 이름의 **가짜 장치**가 데몬에 "승인"이나 "PC 명령" 신호를 보낼 위험이 있습니다.
(macOS 의 BLE 라이브러리는 페어링된 장치인지 확인할 수 없어 BLE 페어링만으로는 막지 못합니다.)

1. **USB 로 처음 연결**하면 데몬이 32바이트 비밀키를 만들어 `~/.deskstream/device_key`(권한 600)에 두고 장치에 등록합니다. 장치는 키 등록을 **USB 에서만** 받습니다.
2. BLE 로 연결될 때마다 데몬이 새 무작위 값(nonce)을 보냅니다.
3. BLE 로 오는 버튼·승인 신호에는 `HMAC-SHA256(키, nonce + 본문)` 서명과 증가 카운터가 붙고, 데몬은 둘 다 맞을 때만 받습니다. 가짜 장치 · 변조 · 재전송은 거부됩니다.

- 키가 없는 장치를 BLE 로 쓰면 승인 화면에 `PAIR VIA USB` 가 뜨고 버튼 신호는 거부됩니다 → USB 로 한 번 연결하세요.
- 데몬 → 장치 방향(화면 표시)은 서명하지 않습니다. 근처 기기가 장치에 접속해 **가짜 화면을 띄울 수는** 있지만 PC 쪽 동작을 일으킬 수는 없습니다.

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

### 승인 화면 (권한 요청 대기 중 — 4화면이 한 장으로)

```
┌──────────────┬──────────────┬──────────────┬──────────────┐
│APPROVE?      │web-app       │Bash          │1/2 0:12      │  ← 반전 바, 깜빡임
│rm -rf node_mo│dules && npm i│nstall        │              │  ← 명령 요약 (화면당 21자 × 2줄)
│  [APPROVE]   │   [NEXT >]   │    [DENY]    │   [LATER]    │
└──────────────┴──────────────┴──────────────┴──────────────┘
     BTN1           BTN2           BTN3           BTN4
```

### 상세 화면 (개요에서 슬롯 버튼 짧게)

| 화면 1 | 화면 2 | 화면 3 · 4 |
|:--|:--|:--|
| 이름 · 상태 · 경과 · 권한 모드(`mode plan` 등) · 하위 에이전트/오류 수 | 최근 도구 4개 — `>` 실행 중 · `+` 성공 · `!` 실패 | 마지막 답변 (최대 120자) |

- 60초 동안 버튼을 누르지 않거나 세션이 끝나면 개요로 돌아갑니다.
- PC 명령 결과는 해당 버튼 화면 아래에 `RUN …` → `OK … exit 0` / `FAIL … exit 2` 띠로 4초간 보입니다.
- 음소거 중이거나 무음 시간대면 개요의 OLED 4 오른쪽 위에 스피커+X 아이콘이 보입니다.

## 버튼

| 화면 | 동작 | 기능 |
|:--|:--|:--|
| 개요 | BTN i 짧게 | 슬롯 i 상세 화면 (완료 표시도 확인 처리 → IDLE) |
| 개요·상세 | BTN i 길게 | 설정 파일의 PC 명령 i 실행 |
| 상세 | BTN1 / BTN2 | 개요로 / 음소거 켜기·끄기 (보드에 저장) |
| 상세 | BTN3 / BTN4 | 이전 / 다음 세션 |
| 승인 | BTN1 | **승인** — 도구가 바로 실행됨 |
| 승인 | BTN2 | 다음 대기 요청 보기 (대기 2개 이상일 때만) |
| 승인 | BTN3 | **거절** — Claude에게 "장치에서 거절" 메시지 전달 |
| 승인 | BTN4 | 나중에 — 장치에서 결정하지 않고 터미널 대화상자에 맡김 |

- 새 승인 요청이 뜬 직후 0.7초 동안은 버튼을 무시합니다 (다른 걸 누르려다 승인되는 것 방지).
- "항상 허용"은 없습니다. 허용 규칙은 터미널에서 직접 추가하세요.

## 사용 방법

### 1. 펌웨어
- 필요 라이브러리: **DeskStreamCore**(저장소 `libraries/`, 루트 README "최초 설정"), **U8g2**
- 보드: `esp32:esp32:makergo_c3_supermini`, USB CDC On Boot = Enabled, CPU 160MHz, Partition = Default

```bash
arduino-cli compile --fqbn "esp32:esp32:makergo_c3_supermini:CDCOnBoot=default,CPUFreq=160,PartitionScheme=default" Agent_Monitor
```

### 2. PC 데몬 (Python 3.11+, macOS에서 검증)

```bash
cd Agent_Monitor/bridge_py
pip install -r requirements.txt          # pyserial · bleak(BLE) · pystray/Pillow(트레이)
python3 agent_bridge.py tray             # 메뉴 막대 아이콘과 함께 실행 (권장)
python3 agent_bridge.py run -v           # 터미널에서 실행 · 이벤트 출력
```

- 연결 선택: `--transport auto`(기본: USB 우선, 없으면 BLE) · `usb` · `ble`, 포트 지정: `--serial /dev/cu.usbmodemXXXX`
- 트레이 아이콘(막대 4개) 색: **회색** 장치 없음 · **초록** USB · **파랑** BLE · **주황** 승인 대기. 메뉴: 상태 · 설정 파일 열기 · 종료
- BLE 는 macOS **Bluetooth 권한**이 필요합니다. 데몬을 실행한 앱(터미널 등)에서 처음 한 번 허용하세요.
- 펌웨어는 [웹 플래셔](https://worynim.github.io/DeskStream/)에서도 설치할 수 있습니다.

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

### 4. PC 명령 · 무음 시간대 설정 (선택)

```bash
python3 agent_bridge.py init-config     # 예시 → ~/.deskstream/agent_monitor.toml (권한 600)
```

```toml
[quiet]                 # 이 시간에는 알림음을 보내지 않음 (자정 넘김 가능)
start = "23:00"
end = "08:00"

[[commands]]            # 버튼 길게 누르기 — button = 1~4
button = 2
label = "npm test"
type = "shell"          # focus(app 앞으로) | shell(command 실행) | keys(text 입력 + key)
command = "npm test"
cwd = "~/work/web-app"
timeout = 300
```

- 장치는 **버튼 번호만** 보냅니다. 실행 내용은 이 파일에만 있고, 버튼을 누를 때마다 다시 읽습니다 (데몬 재시작 불필요).
- 파일을 다른 사용자도 쓸 수 있는 권한이면(group/other write) 실행하지 않습니다.
- `keys` 는 ASCII 텍스트와 `return · escape · tab · space · ctrl+c` 만 지원하고, macOS **손쉬운 사용** 권한(데몬을 실행한 터미널 앱)이 필요합니다. `app` 을 지정하면 그 앱을 먼저 앞으로 가져옵니다.
- 전체 예시: [bridge_py/agent_monitor.example.toml](./bridge_py/agent_monitor.example.toml). 다른 위치의 설정은 `run --config 경로`.

### 5. 화면 확인 (Claude Code 없이)
```bash
python3 demo_hooks.py        # 슬롯 4개를 실행 중 / 승인 대기 / 완료 / 생각 중으로
python3 demo_hooks.py --end
```

### Windows (실기 미검증)

코드는 Windows 에서도 돌도록 정리했지만 실제 Windows 에서는 확인하지 않았습니다.

- 데몬·USB·BLE·트레이·훅 서버는 OS 공통 코드입니다. 설정 파일 권한 검사는 Windows 에서는 하지 않습니다(ACL 사용).
- `SessionStart` 훅은 `curl … || true` 셸 명령이라 **Git Bash** 가 필요합니다 (Windows 용 Claude Code 요구 사항과 같음).
- PC 명령 중 `focus`·`keys` 는 **macOS 전용**입니다 (Windows 에서는 `macOS only` 로 실패 표시). `shell` 은 동작합니다.

## 파일 구성

| 파일 | 내용 |
|:--|:--|
| `Agent_Monitor.ino` | USB·BLE 줄 수신 · 버튼 · 알림음 · 100ms 주기 렌더링 |
| `agent_ble.h/.cpp` | BLE UART (NimBLE, Nordic UART UUID) |
| `agent_auth.h/.cpp` | 서명 키(NVS) · nonce · HMAC-SHA256 서명 |
| `agent_state.h/.cpp` | 슬롯 모델과 메시지 해석 (순수 C++, 호스트 테스트) |
| `agent_ui.h/.cpp` | 슬롯 · 승인 · 상세 · 오프라인 화면, PC 명령 결과 띠, 음소거 아이콘 |
| `config.h` | 링크 timeout · 프레임 주기 · 깜빡임 주기 · 승인 오조작 방지 · 상세 화면 복귀 시간 등 |
| `bridge_py/agent_bridge.py` | 데몬 (훅 서버 · 송신 루프 · 훅 설정 명령) |
| `bridge_py/sessions.py` | 훅 이벤트 → 세션 상태 · 슬롯 배정 |
| `bridge_py/approvals.py` | 권한 요청 대기열 (결정 · 다음 · 터미널 선결정 정리) |
| `bridge_py/commands.py` | 사용자 설정 파일 · PC 명령 실행 · 무음 시간대 |
| `bridge_py/agent_monitor.example.toml` | 설정 예시 |
| `bridge_py/protocol.py` | 시리얼 메시지 형식 (프로토콜 설명 포함) |
| `bridge_py/transport.py` | USB 시리얼 · BLE(bleak) · USB 우선 자동 선택 |
| `bridge_py/security.py` | 서명 키 생성·등록 · BLE 신호 서명·카운터 검증 |
| `bridge_py/tray.py` | 메뉴 막대 트레이 아이콘 |
| `bridge_py/hooks_config.py` | 훅 설정 생성 · 설치 · 제거 |
| `bridge_py/demo_hooks.py` | 화면 확인용 가짜 훅 |
| `p0_spike/` | P0 훅 동작 검증 도구와 결과 |

## 테스트

```bash
cd Agent_Monitor/bridge_py && python3 -m unittest discover -s tests -v      # 데몬 43건

# 펌웨어 상태 해석 (저장소 루트에서)
L=libraries/DeskStreamCore/src
g++ -std=c++17 -IAgent_Monitor -I$L Agent_Monitor/test/test_agent_state.cpp Agent_Monitor/agent_state.cpp \
    $L/util/ds_json_lite.cpp -o /tmp/test_agent_state && /tmp/test_agent_state
```
