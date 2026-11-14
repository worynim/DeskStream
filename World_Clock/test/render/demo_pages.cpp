// worynim@gmail.com
/**
 * @file demo_pages.cpp
 * @brief 화면 미리보기용 고정 장면 구현
 */
#include "demo_pages.h"

#include "../../clock_view.h"
#include "../../misc_view.h"
#include "../../setup_view.h"

namespace {

// 서울 금요일 오후 11:41 기준 네 도시 — 낮/밤 · AM/PM 네 조합
const ClockFace kClocks[4] = {
    {"서울", LANG_KO, 23, 41, 11, 13, 5, 0, false, true, true},
    {"런던", LANG_KO, 14, 41, 11, 13, 5, 0, false, true, true},
    {"뉴욕", LANG_KO, 9, 41, 11, 13, 5, 0, false, true, true},
    {"시드니", LANG_KO, 1, 41, 11, 14, 6, +1, false, true, true},
};

// 자리가 가장 빠듯한 경우: 네 자리 시각, 긴 도시 이름, 날짜 차이, 시각 미확정
const ClockFace kWide[4] = {
    {"요하네스버그", LANG_KO, 12, 0, 12, 28, 3, -1, false, true, true},
    {"로스앤젤레스", LANG_KO, 0, 8, 12, 28, 3, -1, false, true, true},
    {"호놀룰루", LANG_KO, 22, 22, 12, 28, 3, 0, true, true, true},
    {"서울", LANG_KO, 0, 0, 1, 1, 0, 0, false, true, false},
};

// 영문 표시: 긴 도시 이름과 영문 요일
const ClockFace kEnglish[4] = {
    {"Seoul", LANG_EN, 23, 41, 11, 13, 5, 0, false, true, true},
    {"Rio de Janeiro", LANG_EN, 11, 41, 11, 13, 5, 0, false, true, true},
    {"Buenos Aires", LANG_EN, 11, 41, 11, 12, 4, -1, false, true, true},
    {"Sydney", LANG_EN, 1, 41, 11, 14, 6, +1, false, true, true},
};

int mapX(int lon) { return (lon + 180) * 128 / 360; }
int mapY(int lat) { return (90 - lat) * 64 / 180; }

void drawClockPage(const ClockFace* faces, int screen, u8g2_t* u, bool blinkOn, bool force24) {
    ClockFace f = faces[screen];
    f.colonOn = blinkOn;
    if (force24) f.hour24 = true;
    drawClock(u, f);
}

/** 설정 장면 하나: 화면 edit 를 고치는 중. 나머지 화면의 역할은 장치와 같은 setupRole 로 정한다 */
struct SetupScene {
    int edit;
    SetupPreview preview;
    const char* west; int westOffsetMin;
    const char* east; int eastOffsetMin;
    int lon, lat;
};

const SetupScene kSetupScenes[3] = {
    {1, {"파리", LANG_KO, 15, 41, 60, false, "서울", -480, true, true}, "런던", 0, "베를린", 60, 2, 49},
    {0, {"뉴델리", LANG_KO, 20, 11, 330, false, nullptr, 0, true, true}, "카라치", 300, "카트만두", 345, 77, 29},
    {2, {"New York", LANG_EN, 10, 41, -240, true, "Seoul", -780, true, true}, "Bogota", -300, "Boston", -240, -74, 41},
};

void drawSetupScene(const SetupScene& sc, int screen, u8g2_t* u, bool blinkOn) {
    SetupPreview p = sc.preview;
    p.frameOn = blinkOn;
    switch (setupRole(sc.edit, screen)) {
        case SETUP_EDIT: drawSetupPreview(u, p); break;
        case SETUP_MAP: drawSetupMap(u, p.lang, mapX(sc.lon), mapY(sc.lat), blinkOn); break;
        case SETUP_WEST: drawSetupMove(u, p.lang, false, sc.west, sc.westOffsetMin); break;
        default: drawSetupMove(u, p.lang, true, sc.east, sc.eastOffsetMin); break;
    }
}

/** 정보 카드(화면 3 을 짧게 눌렀을 때) + 알림 딱지 */
void drawInfoPage(int screen, u8g2_t* u) {
    static const InfoCard kCards[4] = {
        {"서울", LANG_KO, 2026, 11, 13, 5, 540, false, true, 0},
        {"뉴델리", LANG_KO, 2026, 11, 13, 5, 330, false, false, -210},
        {"New York", LANG_EN, 2026, 7, 4, 6, -240, true, false, -780},
        {"시드니", LANG_KO, 2026, 11, 14, 6, 660, true, false, 120},
    };
    if (screen == 3) {
        drawClock(u, kClocks[3]);
        drawToast(u, "밝기 2/4");
    } else {
        drawInfoCard(u, kCards[screen]);
    }
}

/** WiFi 설정 포털 안내 */
void drawPortalPage(int screen, u8g2_t* u) {
    static const char* const kKo[5] = {"WiFi 설정이 필요합니다", nullptr, "휴대폰으로 아래 WiFi 에 접속", "World_Clock_Setup", "192.168.4.1"};
    static const char* const kEn[5] = {"WiFi setup needed", nullptr, "Join this WiFi on your phone", "World_Clock_Setup", "192.168.4.1"};
    static const char* const kWait[3] = {nullptr, nullptr, "WiFi 연결 중..."};
    if (screen == 0) drawMessage(u, kKo, 5);
    else if (screen == 1) drawMessage(u, kEn, 5);
    else if (screen == 2) drawMessage(u, kWait, 3);
    else drawMessage(u, nullptr, 0);
}

}  // namespace

const char* demoPageName(int page) {
    static const char* const kNames[kDemoPageCount] = {"clock_12h", "clock_24h", "clock_wide", "setup_screen2", "setup_screen1",
                                                       "clock_english", "setup_english", "info_toast", "wifi"};
    return kNames[page];
}

void drawDemoPage(int page, int screen, u8g2_t* u, bool blinkOn) {
    switch (page) {
        case 0: drawClockPage(kClocks, screen, u, blinkOn, false); break;
        case 1: drawClockPage(kClocks, screen, u, blinkOn, true); break;
        case 2: drawClockPage(kWide, screen, u, blinkOn, false); break;
        case 3: drawSetupScene(kSetupScenes[0], screen, u, blinkOn); break;
        case 4: drawSetupScene(kSetupScenes[1], screen, u, blinkOn); break;
        case 5: drawClockPage(kEnglish, screen, u, blinkOn, false); break;
        case 6: drawSetupScene(kSetupScenes[2], screen, u, blinkOn); break;
        case 7: drawInfoPage(screen, u); break;
        default: drawPortalPage(screen, u); break;
    }
}
