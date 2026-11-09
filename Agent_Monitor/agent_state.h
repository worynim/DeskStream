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
    char sum[64] = "";
    uint32_t elS = 0;      ///< 메시지를 받은 시점의 경과 초
    uint32_t rxMs = 0;     ///< 메시지를 받은 시각 (millis) — 이후 경과는 장치가 직접 센다
    uint8_t err = 0;
    uint8_t sub = 0;
};

struct Model {
    Slot slots[kSlots];
    uint32_t lastRxMs = 0;   ///< 마지막으로 유효한 메시지를 받은 시각
    bool everRx = false;
};

enum class MsgType : uint8_t { Invalid, Slot, Alert, Heartbeat };

struct ParseResult {
    MsgType type = MsgType::Invalid;
    int slot = -1;
    State alert = State::Idle;   ///< type == Alert 일 때 알림 종류 (Approve/Input/Done/Error)
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

/** 화면 표시용 상태 이름 ("RUNNING" 등) */
const char* stateLabel(State st);

}  // namespace am

#endif
