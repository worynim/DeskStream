// worynim@gmail.com
/**
 * @file test_agent_state.cpp
 * @brief agent_state 호스트 단위 테스트
 * @details 실행 (저장소 루트에서):
 *   L=libraries/DeskStreamCore/src
 *   g++ -std=c++17 -IAgent_Monitor -I$L Agent_Monitor/test/test_agent_state.cpp Agent_Monitor/agent_state.cpp \
 *       $L/util/ds_json_lite.cpp -o /tmp/test_agent_state && /tmp/test_agent_state
 */
#include <stdio.h>
#include <string.h>

#include "agent_state.h"

static int fails = 0;
#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            fails++;                                               \
        }                                                          \
    } while (0)

using namespace am;

static void testSlot() {
    Model m;
    auto r = applyLine(m,
                       "{\"t\":\"slot\",\"i\":1,\"on\":1,\"name\":\"DeskStream\",\"st\":\"run\",\"tool\":\"Bash\","
                       "\"sum\":\"echo \\\"hi\\\"\",\"el\":75,\"err\":2,\"sub\":1}",
                       1000);
    CHECK(r.type == MsgType::Slot && r.slot == 1);
    const Slot& s = m.slots[1];
    CHECK(s.on && s.st == State::Run);
    CHECK(strcmp(s.name, "DeskStream") == 0);
    CHECK(strcmp(s.tool, "Bash") == 0);
    CHECK(strcmp(s.sum, "echo \"hi\"") == 0);
    CHECK(s.err == 2 && s.sub == 1);
    CHECK(elapsedS(s, 1000) == 75);
    CHECK(elapsedS(s, 6500) == 80);
    CHECK(linkAlive(m, 2000, 6000));
    CHECK(!linkAlive(m, 8000, 6000));

    r = applyLine(m, "{\"t\":\"slot\",\"i\":1,\"on\":0}", 2000);
    CHECK(r.type == MsgType::Slot && !m.slots[1].on);
}

static void testRejects() {
    Model m;
    CHECK(!linkAlive(m, 0, 6000));   // 받은 적 없음
    CHECK(applyLine(m, "ESP-ROM:esp32c3", 0).type == MsgType::Invalid);
    CHECK(applyLine(m, "{\"t\":\"slot\",\"i\":4,\"on\":0}", 0).type == MsgType::Invalid);    // 범위 밖
    CHECK(applyLine(m, "{\"t\":\"slot\",\"i\":0,\"on\":1,\"st\":\"zzz\"}", 0).type == MsgType::Invalid);
    // 이름이 버퍼(20자)를 넘으면 슬롯을 바꾸지 않는다
    CHECK(applyLine(m, "{\"t\":\"slot\",\"i\":0,\"on\":1,\"st\":\"idle\",\"name\":\"123456789012345678901\"}", 0).type ==
          MsgType::Invalid);
    CHECK(!m.slots[0].on);
    CHECK(!m.everRx);
}

static void testAlertAndHeartbeat() {
    Model m;
    auto r = applyLine(m, "{\"t\":\"alert\",\"kind\":\"approve\",\"i\":3}", 10);
    CHECK(r.type == MsgType::Alert && r.slot == 3 && r.alert == State::Approve);
    r = applyLine(m, "{\"t\":\"hb\"}", 20);
    CHECK(r.type == MsgType::Heartbeat && m.lastRxMs == 20);
}

static void testFormat() {
    char b[12];
    formatElapsed(0, b, sizeof(b));
    CHECK(strcmp(b, "0:00") == 0);
    formatElapsed(754, b, sizeof(b));
    CHECK(strcmp(b, "12:34") == 0);
    formatElapsed(3725, b, sizeof(b));
    CHECK(strcmp(b, "1:02:05") == 0);
    CHECK(strcmp(stateLabel(State::Approve), "APPROVE?") == 0);
}

int main() {
    testSlot();
    testRejects();
    testAlertAndHeartbeat();
    testFormat();
    printf(fails ? "❌ %d failed\n" : "✅ all passed\n", fails);
    return fails ? 1 : 0;
}
