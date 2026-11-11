// worynim@gmail.com
/**
 * @file agent_state.h
 * @brief 슬롯(세션) 상태 모델과 데몬 메시지 해석 — 순수 C++ (Arduino 의존 없음)
 * @details 데몬(bridge_py/protocol.py)이 보내는 JSON Lines 한 줄을 Model 에 반영한다.
 *          문자열 버퍼 크기는 protocol.py 의 NAME_MAX / TOOL_MAX / SUM_MAX 와 맞춘다.
 *          호스트 테스트: test/test_agent_state.cpp
 */
#ifndef AGENT_STATE_H
#define AGENT_STATE_H

#include <stddef.h>
#include <stdint.h>

namespace am {

constexpr int kSlots = 4;

enum class State : uint8_t { Idle, Think, Run, Approve, Input, Done, Error };

struct Slot {
    bool on = false;
    char name[21] = "";
    State st = State::Idle;
    char tool[21] = "";
    char sum[97] = "";       ///< 요약 — 개요 화면 2줄(한글 16자 × 2)
    uint32_t elS = 0;      ///< 메시지를 받은 시점의 경과 초
    uint32_t rxMs = 0;     ///< 메시지를 받은 시각 (millis) — 이후 경과는 장치가 직접 센다
    uint8_t err = 0;
    uint8_t sub = 0;
};

/** 장치 버튼 결정을 기다리는 권한 요청 (id == 0 이면 없음 → 승인 화면 닫힘) */
struct Perm {
    uint32_t id = 0;
    int8_t slot = -1;
    char name[21] = "";
    char tool[21] = "";
    char sum[241] = "";      ///< 명령 요약 — 승인 화면 4화면 × 2줄(줄당 128px)에 나눠 표시
    uint8_t n = 0;           ///< 보이는 요청 번호 (1부터)
    uint8_t q = 0;           ///< 대기 중인 요청 수
    uint32_t shownMs = 0;    ///< 이 id 를 처음 받은 시각 — 오조작 방지·대기 시간 표시용
};

/** 상세 화면 정보 — 장치가 보고 있는 슬롯 하나 (데몬이 바뀔 때마다 다시 보냄) */
struct Detail {
    bool on = false;
    int8_t slot = -1;
    char name[21] = "";
    State st = State::Idle;
    char mode[13] = "";      ///< permission_mode
    uint32_t elS = 0;
    uint32_t rxMs = 0;
    uint8_t err = 0;
    uint8_t sub = 0;
    char msg[1729] = "";     ///< 마지막 답변 — 상세 화면 4페이지 · 한글 576자 = 1728바이트 (BTN3/4 로 페이지 넘김)
    char h[4][22] = {};      ///< 최근 도구 — 첫 글자 '>' 실행 중 '+' 성공 '!' 실패
};

enum class CmdState : uint8_t { None, Run, Ok, Fail };

constexpr int kUsageWindows = 2;   ///< 사용량 구간 수 — 0 = 5시간, 1 = 주간

/** Claude Code 사용량 한 구간 (데몬이 상태줄에서 받아 보냄 — 마지막 두 화면의 빈 슬롯에 표시) */
struct Usage {
    bool known = false;      ///< 아직 받은 값이 없으면 false
    uint8_t pct = 0;         ///< 사용률 0~100 %
    uint32_t resetS = 0;     ///< 메시지를 받은 시점에 리셋까지 남은 초 (0 = 모름)
    uint32_t rxMs = 0;       ///< 메시지를 받은 시각 (millis) — 이후 남은 시간은 장치가 직접 센다
};

/** 버튼 길게 누르기 PC 명령의 진행·결과 (버튼별 알림 띠) */
struct CmdToast {
    CmdState st = CmdState::None;
    char label[21] = "";
    char msg[13] = "";
    uint32_t rxMs = 0;
};

/** 데몬 설정 파일 [device] — 음소거·OLED 밝기 */
struct DevCfg {
    bool mute = false;
    uint8_t bright = 255;    ///< SSD1306 contrast 1~255
};

struct Model {
    Slot slots[kSlots];
    DevCfg dev;
    Perm perm;
    Detail detail;
    CmdToast toasts[kSlots];
    Usage usage[kUsageWindows];
    uint32_t lastRxMs = 0;   ///< 마지막으로 유효한 메시지를 받은 시각
    bool everRx = false;
};

enum class MsgType : uint8_t { Invalid, Slot, Alert, Heartbeat, Perm, Detail, Cmd, Dev, Usage };

struct ParseResult {
    MsgType type = MsgType::Invalid;
    int slot = -1;
    State alert = State::Idle;   ///< type == Alert 일 때 알림 종류 (Approve/Input/Done/Error)
    bool newPerm = false;        ///< type == Perm 이고 새 요청이 보이기 시작함
};

/** "idle"·"run" 등 → State. 모르는 값이면 false */
bool parseState(const char* s, State* out);

/** 데몬이 보낸 한 줄을 모델에 반영한다. 해석하지 못하면 type = Invalid (모델 변경 없음) */
ParseResult applyLine(Model& m, const char* line, uint32_t nowMs);

/** 마지막 수신 후 timeoutMs 안이면 연결된 것으로 본다 */
bool linkAlive(const Model& m, uint32_t nowMs, uint32_t timeoutMs);

/** 슬롯 상태가 시작된 뒤 지난 초 */
uint32_t elapsedS(const Slot& s, uint32_t nowMs);

/** 경과 초 → "m:ss" (1시간 미만) / "h:mm:ss" */
void formatElapsed(uint32_t s, char* out, size_t cap);

/** 사용량 구간이 리셋될 때까지 남은 초 (지났거나 모르면 0) */
uint32_t remainS(const Usage& u, uint32_t nowMs);

/** 남은 초 → "45m" (1시간 미만) / "2h 05m" (1일 미만) / "3d 4h" */
void formatRemain(uint32_t s, char* out, size_t cap);

/** 화면 표시용 상태 이름 ("RUNNING" 등) */
const char* stateLabel(State st);

}  // namespace am

#endif
