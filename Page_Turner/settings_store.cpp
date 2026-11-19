// worynim@gmail.com
/**
 * @file settings_store.cpp
 * @brief ds::ConfigStore 필드 표 ↔ Setting::idx
 */
#include "settings_store.h"

#include <DeskStreamCore.h>

#include "model.h"

namespace store {

// NVS에 저장하는 값 (모두 값 목록의 위치)
struct Persist {
    uint8_t ytSteps, ytGap, ytCur;
    uint8_t ktKeys, ktGap;
    uint8_t stSteps, stGap, stCur;
    uint8_t camTimer;
    uint8_t beep;
    uint8_t mode;
};

static Persist p;
static ds::ConfigStore cfg;

// 범위 상한 = 값 목록 개수 - 1 (model.cpp: Steps 13, Keys 12, Gap 8, Cursor 2, Timer 4, Beep 2, 모드 7)
#define F(m, nvs, hi) DS_CFG_U8_F(Persist, m, nvs, nvs, 0, hi, nullptr, ds::FIELD_NO_WEB)
static const ds::ConfigField kFields[] = {
    F(ytSteps, "yt_steps", 12), F(ytGap, "yt_gap", 7), F(ytCur, "yt_cur", 1),
    F(ktKeys, "kt_keys", 11),   F(ktGap, "kt_gap", 7),
    F(stSteps, "st_steps", 12), F(stGap, "st_gap", 7), F(stCur, "st_cur", 1),
    F(camTimer, "cam_timer", 3),
    F(beep, "beep", 1),
    F(mode, "mode", kNumModes - 1),
};

// Persist 필드 ↔ Setting 대응 (mode 제외)
struct Link {
    uint8_t* field;
    Setting* setting;
};

static Link links[10];

static void buildLinks() {
    Setting* yt = modeAt(0).settings;
    Setting* kt = modeAt(1).settings;
    Setting* st = modeAt(3).settings;
    Setting* cam = modeAt(5).settings;
    Link l[] = {
        {&p.ytSteps, &yt[0]}, {&p.ytGap, &yt[1]}, {&p.ytCur, &yt[2]},
        {&p.ktKeys, &kt[0]},  {&p.ktGap, &kt[1]},
        {&p.stSteps, &st[0]}, {&p.stGap, &st[1]}, {&p.stCur, &st[2]},
        {&p.camTimer, &cam[0]},
        {&p.beep, &beepSetting()},
    };
    for (int i = 0; i < 10; i++) links[i] = l[i];
}

void begin() {
    buildLinks();
    for (Link& l : links) *l.field = l.setting->defIdx;   // 저장값이 없으면 기본값이 남는다
    p.mode = 0;
    cfg.begin("pturner", &p, kFields, sizeof(kFields) / sizeof(kFields[0]));
    for (Link& l : links) l.setting->idx = clampIdx(*l.field, *l.setting);
    if (p.mode >= kNumModes) p.mode = 0;
}

int lastMode() { return p.mode; }

static void copyOut(int modeIdx) {
    for (Link& l : links) *l.field = l.setting->idx;
    p.mode = (uint8_t)modeIdx;
}

void saveSoon(int modeIdx) {
    copyOut(modeIdx);
    cfg.setDirty();
}

void saveNow(int modeIdx) {
    copyOut(modeIdx);
    cfg.saveNow();
}

}  // namespace store
