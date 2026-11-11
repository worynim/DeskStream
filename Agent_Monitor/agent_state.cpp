// worynim@gmail.com
/**
 * @file agent_state.cpp
 * @brief agent_state.h 구현 — 메시지 해석은 DeskStreamCore 의 ds_json_lite 를 쓴다
 */
#include "agent_state.h"

#include <stdio.h>
#include <string.h>

#include <util/ds_json_lite.h>

namespace am {

namespace {

struct StateName {
    const char* key;
    State st;
    const char* label;
};

const StateName kStates[] = {
    {"idle", State::Idle, "IDLE"},          {"think", State::Think, "THINKING"},
    {"run", State::Run, "RUNNING"},         {"approve", State::Approve, "APPROVE?"},
    {"input", State::Input, "NEED INPUT"},  {"done", State::Done, "DONE"},
    {"error", State::Error, "ERROR"},
};

/** 문자열 필드: 없으면 빈 문자열, 잘못된 값(이스케이프·길이 초과)이면 false */
bool getStr(const char* line, const char* key, char* out, size_t cap) {
    if (!ds::jsonFindValue(line, key)) {
        out[0] = '\0';
        return true;
    }
    return ds::jsonGetString(line, key, out, cap);
}

long getIntOr(const char* line, const char* key, long fallback) {
    long v;
    return ds::jsonGetInt(line, key, &v) ? v : fallback;
}

uint8_t clampU8(long v) { return v < 0 ? 0 : v > 255 ? 255 : (uint8_t)v; }

}  // namespace

bool parseState(const char* s, State* out) {
    for (const auto& n : kStates) {
        if (strcmp(s, n.key) == 0) {
            *out = n.st;
            return true;
        }
    }
    return false;
}

const char* stateLabel(State st) {
    for (const auto& n : kStates) {
        if (n.st == st) return n.label;
    }
    return "?";
}

ParseResult applyLine(Model& m, const char* line, uint32_t nowMs) {
    ParseResult r;
    char type[8];
    if (!ds::jsonGetString(line, "t", type, sizeof(type))) return r;

    if (strcmp(type, "hb") == 0) {
        r.type = MsgType::Heartbeat;
    } else if (strcmp(type, "slot") == 0 || strcmp(type, "alert") == 0) {
        long i;
        if (!ds::jsonGetInt(line, "i", &i) || i < 0 || i >= kSlots) return r;
        r.slot = (int)i;
        if (type[0] == 'a') {
            char kind[10];
            if (!ds::jsonGetString(line, "kind", kind, sizeof(kind)) || !parseState(kind, &r.alert)) return r;
            r.type = MsgType::Alert;
        } else {
            Slot tmp;   // 전부 해석된 뒤에만 반영 (반쯤 바뀐 슬롯을 그리지 않게)
            tmp.on = getIntOr(line, "on", 0) != 0;
            if (tmp.on) {
                char st[10];
                if (!ds::jsonGetString(line, "st", st, sizeof(st)) || !parseState(st, &tmp.st)) return r;
                if (!getStr(line, "name", tmp.name, sizeof(tmp.name)) ||
                    !getStr(line, "tool", tmp.tool, sizeof(tmp.tool)) ||
                    !getStr(line, "sum", tmp.sum, sizeof(tmp.sum))) return r;
                long el = getIntOr(line, "el", 0);
                tmp.elS = el < 0 ? 0 : (uint32_t)el;
                tmp.err = clampU8(getIntOr(line, "err", 0));
                tmp.sub = clampU8(getIntOr(line, "sub", 0));
            }
            tmp.rxMs = nowMs;
            m.slots[i] = tmp;
            r.type = MsgType::Slot;
        }
    } else if (strcmp(type, "detail") == 0) {
        long i;
        if (!ds::jsonGetInt(line, "i", &i) || i < 0 || i >= kSlots) return r;
        static Detail tmp;   // 답변이 커서(1.7KB) 스택에 두지 않는다. 전부 해석된 뒤에만 m.detail 에 반영
        tmp = Detail();
        tmp.slot = (int8_t)i;
        tmp.on = getIntOr(line, "on", 0) != 0;
        if (tmp.on) {
            char st[10];
            if (!ds::jsonGetString(line, "st", st, sizeof(st)) || !parseState(st, &tmp.st)) return r;
            static const char* const hk[4] = {"h0", "h1", "h2", "h3"};
            if (!getStr(line, "name", tmp.name, sizeof(tmp.name)) ||
                !getStr(line, "mode", tmp.mode, sizeof(tmp.mode)) ||
                !getStr(line, "msg", tmp.msg, sizeof(tmp.msg))) return r;
            for (int k = 0; k < 4; k++) {
                if (!getStr(line, hk[k], tmp.h[k], sizeof(tmp.h[k]))) return r;
            }
            long el = getIntOr(line, "el", 0);
            tmp.elS = el < 0 ? 0 : (uint32_t)el;
            tmp.err = clampU8(getIntOr(line, "err", 0));
            tmp.sub = clampU8(getIntOr(line, "sub", 0));
        }
        tmp.rxMs = nowMs;
        m.detail = tmp;
        r.type = MsgType::Detail;
        r.slot = (int)i;
    } else if (strcmp(type, "cmd") == 0) {
        long b;
        char st[6];
        if (!ds::jsonGetInt(line, "b", &b) || b < 0 || b >= kSlots) return r;
        if (!ds::jsonGetString(line, "st", st, sizeof(st))) return r;
        CmdToast tmp;
        if (strcmp(st, "run") == 0) tmp.st = CmdState::Run;
        else if (strcmp(st, "ok") == 0) tmp.st = CmdState::Ok;
        else if (strcmp(st, "fail") == 0) tmp.st = CmdState::Fail;
        else return r;
        if (!getStr(line, "label", tmp.label, sizeof(tmp.label)) ||
            !getStr(line, "msg", tmp.msg, sizeof(tmp.msg))) return r;
        tmp.rxMs = nowMs;
        m.toasts[b] = tmp;
        r.type = MsgType::Cmd;
        r.slot = (int)b;
    } else if (strcmp(type, "dev") == 0) {
        long mute, bright;
        if (!ds::jsonGetInt(line, "mute", &mute) || !ds::jsonGetInt(line, "bright", &bright) || bright < 1 ||
            bright > 255) return r;
        m.dev.mute = mute != 0;
        m.dev.bright = (uint8_t)bright;
        r.type = MsgType::Dev;
    } else if (strcmp(type, "usage") == 0) {
        static const char* const pk[kUsageWindows] = {"p5", "p7"};
        static const char* const rk[kUsageWindows] = {"r5", "r7"};
        Usage tmp[kUsageWindows];
        for (int k = 0; k < kUsageWindows; k++) {
            long pct, reset;
            if (!ds::jsonGetInt(line, pk[k], &pct) || !ds::jsonGetInt(line, rk[k], &reset) || pct > 100 ||
                reset < 0) return r;
            tmp[k].known = pct >= 0;   // 음수 = 데몬도 아직 모름
            tmp[k].pct = tmp[k].known ? (uint8_t)pct : 0;
            tmp[k].resetS = (uint32_t)reset;
            tmp[k].rxMs = nowMs;
        }
        for (int k = 0; k < kUsageWindows; k++) m.usage[k] = tmp[k];
        r.type = MsgType::Usage;
    } else if (strcmp(type, "perm") == 0) {
        long id;
        if (!ds::jsonGetInt(line, "id", &id) || id < 0) return r;
        Perm tmp;
        if (id != 0) {
            if (!getStr(line, "name", tmp.name, sizeof(tmp.name)) ||
                !getStr(line, "tool", tmp.tool, sizeof(tmp.tool)) ||
                !getStr(line, "sum", tmp.sum, sizeof(tmp.sum))) return r;
            tmp.id = (uint32_t)id;
            long slot = getIntOr(line, "slot", -1);
            tmp.slot = (slot >= 0 && slot < kSlots) ? (int8_t)slot : -1;
            tmp.n = clampU8(getIntOr(line, "n", 1));
            tmp.q = clampU8(getIntOr(line, "q", 1));
            r.newPerm = tmp.id != m.perm.id;
            tmp.shownMs = r.newPerm ? nowMs : m.perm.shownMs;
        }
        m.perm = tmp;
        r.type = MsgType::Perm;
    } else {
        return r;
    }
    m.lastRxMs = nowMs;
    m.everRx = true;
    return r;
}

bool linkAlive(const Model& m, uint32_t nowMs, uint32_t timeoutMs) {
    return m.everRx && (uint32_t)(nowMs - m.lastRxMs) < timeoutMs;
}

uint32_t elapsedS(const Slot& s, uint32_t nowMs) { return s.elS + (uint32_t)(nowMs - s.rxMs) / 1000; }

void formatElapsed(uint32_t s, char* out, size_t cap) {
    if (s < 3600) {
        snprintf(out, cap, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
    } else {
        snprintf(out, cap, "%lu:%02lu:%02lu", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60),
                 (unsigned long)(s % 60));
    }
}

uint32_t remainS(const Usage& u, uint32_t nowMs) {
    uint32_t passed = (uint32_t)(nowMs - u.rxMs) / 1000;
    return u.resetS > passed ? u.resetS - passed : 0;
}

int usageWindowFor(const Model& m, uint8_t idx) {
    if (idx >= kSlots || m.slots[idx].on) return -1;
    int empty = 0, before = 0;   // 빈 슬롯 수 · idx 앞의 빈 슬롯 수
    for (int i = 0; i < kSlots; i++) {
        if (m.slots[i].on) continue;
        if (i < idx) before++;
        empty++;
    }
    int first = empty > kUsageWindows ? empty - kUsageWindows : 0;   // 사용량을 보여줄 첫 빈 슬롯 순번
    return before >= first ? before - first : -1;
}

void formatRemain(uint32_t s, char* out, size_t cap) {
    if (s < 3600) {
        snprintf(out, cap, "%lum", (unsigned long)(s / 60));
    } else if (s < 86400) {
        snprintf(out, cap, "%luh %02lum", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60));
    } else {
        snprintf(out, cap, "%lud %luh", (unsigned long)(s / 86400), (unsigned long)(s / 3600 % 24));
    }
}

}  // namespace am
