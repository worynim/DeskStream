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
#include <string>
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

static void testPerm() {
    Model m;
    auto r = applyLine(m,
                       "{\"t\":\"perm\",\"id\":7,\"slot\":2,\"name\":\"web-app\",\"tool\":\"Bash\","
                       "\"sum\":\"rm -rf build\",\"n\":1,\"q\":2}",
                       100);
    CHECK(r.type == MsgType::Perm && r.newPerm);
    CHECK(m.perm.id == 7 && m.perm.slot == 2 && m.perm.n == 1 && m.perm.q == 2);
    CHECK(strcmp(m.perm.sum, "rm -rf build") == 0 && m.perm.shownMs == 100);
    // 같은 요청이 다시 오면(대기 개수만 바뀜) 처음 본 시각을 유지 — 오조작 방지 시간이 다시 시작되지 않게
    r = applyLine(m, "{\"t\":\"perm\",\"id\":7,\"slot\":2,\"name\":\"web-app\",\"tool\":\"Bash\",\"sum\":\"x\",\"n\":1,\"q\":1}",
                  900);
    CHECK(!r.newPerm && m.perm.shownMs == 100 && m.perm.q == 1);
    r = applyLine(m, "{\"t\":\"perm\",\"id\":0}", 1000);
    CHECK(r.type == MsgType::Perm && m.perm.id == 0);
    CHECK(applyLine(m, "{\"t\":\"perm\"}", 0).type == MsgType::Invalid);          // id 없음
    CHECK(applyLine(m, "{\"t\":\"perm\",\"id\":-1}", 0).type == MsgType::Invalid);
}

static void testDetailCmd() {
    Model m;
    auto r = applyLine(m,
                       "{\"t\":\"detail\",\"i\":2,\"on\":1,\"name\":\"api\",\"st\":\"done\",\"mode\":\"plan\",\"el\":5,"
                       "\"err\":1,\"sub\":0,\"msg\":\"All good\",\"h0\":\"+Bash npm test\",\"h1\":\"!Edit x.py\","
                       "\"h2\":\"\",\"h3\":\"\"}",
                       50);
    CHECK(r.type == MsgType::Detail && r.slot == 2);
    CHECK(m.detail.on && m.detail.slot == 2 && m.detail.st == State::Done);
    CHECK(strcmp(m.detail.mode, "plan") == 0 && strcmp(m.detail.msg, "All good") == 0);
    CHECK(strcmp(m.detail.h[1], "!Edit x.py") == 0 && m.detail.h[2][0] == '\0' && m.detail.err == 1);
    r = applyLine(m, "{\"t\":\"detail\",\"i\":2,\"on\":0}", 60);
    CHECK(r.type == MsgType::Detail && !m.detail.on);

    {   // 긴 상세 답변(4페이지 = 1728바이트)이 통째로 해석된다
        std::string big = "{\"t\":\"detail\",\"i\":1,\"on\":1,\"name\":\"p\",\"st\":\"done\",\"mode\":\"plan\",\"el\":1,\"err\":0,\"sub\":0,\"msg\":\"";
        for (int k = 0; k < 576; k++) big += "한";
        big += "\",\"h0\":\"\",\"h1\":\"\",\"h2\":\"\",\"h3\":\"\"}";
        r = applyLine(m, big.c_str(), 200);
        CHECK(r.type == MsgType::Detail && strlen(m.detail.msg) == 1728);
        std::string over = big;
        over.insert(over.find("\",\"h0\""), "가");   // 1731바이트 — 버퍼(1729)를 넘으면 통째로 버린다
        r = applyLine(m, over.c_str(), 201);
        CHECK(r.type == MsgType::Invalid && strlen(m.detail.msg) == 1728);
    }
    r = applyLine(m, "{\"t\":\"cmd\",\"b\":1,\"st\":\"fail\",\"label\":\"npm test\",\"msg\":\"exit 1\"}", 70);
    CHECK(r.type == MsgType::Cmd && m.toasts[1].st == CmdState::Fail);
    CHECK(strcmp(m.toasts[1].msg, "exit 1") == 0 && m.toasts[1].rxMs == 70);
    CHECK(applyLine(m, "{\"t\":\"cmd\",\"b\":1,\"st\":\"zz\"}", 0).type == MsgType::Invalid);
    CHECK(applyLine(m, "{\"t\":\"cmd\",\"b\":4,\"st\":\"ok\"}", 0).type == MsgType::Invalid);
}

static void testUsage() {
    Model m;
    CHECK(!m.usage[0].known && !m.usage[1].known);
    auto r = applyLine(m, "{\"t\":\"usage\",\"p5\":23,\"r5\":5400,\"p7\":-1,\"r7\":0}", 1000);
    CHECK(r.type == MsgType::Usage);
    CHECK(m.usage[0].known && m.usage[0].pct == 23);
    CHECK(!m.usage[1].known);                                     // 음수 = 모름
    CHECK(remainS(m.usage[0], 1000) == 5400);
    CHECK(remainS(m.usage[0], 61000) == 5340);                    // 장치가 직접 센다
    CHECK(remainS(m.usage[0], 1000 + 6000 * 1000u) == 0);         // 지나면 0
    // 필드 누락·범위 밖이면 거부 — 이전 값 유지
    CHECK(applyLine(m, "{\"t\":\"usage\",\"p5\":50,\"r5\":10}", 2000).type == MsgType::Invalid);
    CHECK(applyLine(m, "{\"t\":\"usage\",\"p5\":101,\"r5\":10,\"p7\":1,\"r7\":1}", 2000).type == MsgType::Invalid);
    CHECK(m.usage[0].pct == 23);
    // 이전 데몬이 보내는 cmds 는 모르는 메시지로 무시한다
    CHECK(applyLine(m, "{\"t\":\"cmds\",\"l0\":\"Terminal\",\"k0\":\"focus\"}", 2000).type == MsgType::Invalid);
}

static void testUsageWindow() {
    Model m;
    // 모두 비면 뒤쪽 두 화면(3·4) = 5시간 · 주간
    CHECK(usageWindowFor(m, 0) == -1 && usageWindowFor(m, 1) == -1);
    CHECK(usageWindowFor(m, 2) == 0 && usageWindowFor(m, 3) == 1);
    // 2·4 만 비면 2 = 5시간, 4 = 주간
    m.slots[0].on = m.slots[2].on = true;
    CHECK(usageWindowFor(m, 0) == -1 && usageWindowFor(m, 2) == -1);
    CHECK(usageWindowFor(m, 1) == 0 && usageWindowFor(m, 3) == 1);
    // 하나만 비면 5시간
    m.slots[3].on = true;
    CHECK(usageWindowFor(m, 1) == 0 && usageWindowFor(m, 3) == -1);
    // 다 차면 없음
    m.slots[1].on = true;
    CHECK(usageWindowFor(m, 1) == -1);
}

static void testDev() {
    Model m;
    CHECK(m.dev.bright == 255 && !m.dev.mute);
    auto r = applyLine(m, "{\"t\":\"dev\",\"mute\":1,\"bright\":102}", 0);
    CHECK(r.type == MsgType::Dev && m.dev.mute && m.dev.bright == 102);
    CHECK(applyLine(m, "{\"t\":\"dev\",\"mute\":0,\"bright\":0}", 0).type == MsgType::Invalid);    // 범위 밖
    CHECK(applyLine(m, "{\"t\":\"dev\",\"mute\":0}", 0).type == MsgType::Invalid);                  // 밝기 없음
    CHECK(m.dev.mute && m.dev.bright == 102);
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
    formatRemain(59, b, sizeof(b));
    CHECK(strcmp(b, "0m") == 0);
    formatRemain(2700, b, sizeof(b));
    CHECK(strcmp(b, "45m") == 0);
    formatRemain(7500, b, sizeof(b));
    CHECK(strcmp(b, "2h 05m") == 0);
    formatRemain(3 * 86400 + 4 * 3600 + 59, b, sizeof(b));
    CHECK(strcmp(b, "3d 4h") == 0);
}

int main() {
    testSlot();
    testRejects();
    testAlertAndHeartbeat();
    testPerm();
    testDetailCmd();
    testUsage();
    testUsageWindow();
    testDev();
    testFormat();
    printf(fails ? "❌ %d failed\n" : "✅ all passed\n", fails);
    return fails ? 1 : 0;
}
