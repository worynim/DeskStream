// worynim@gmail.com
/**
 * @file view.cpp
 * @brief 4화면 그리기 — 위쪽 아이콘/내용 + 설명 + 맨 아래 반전 이름표 (DESIGN §6.6)
 */
#include "view.h"

#include "config.h"

namespace view {

static ds::DisplayCore* display = nullptr;
static const int W = 64;

enum class Arrow : uint8_t { Up, Down, Left, Right };

// ---------- 그리기 도구 ----------
static void center(U8G2& u, int y, const char* t) { u.drawStr((W - u.getStrWidth(t)) / 2, y, t); }

static U8G2& blank(int i) {
    U8G2& u = display->screen(i);
    u.clearBuffer();
    u.setDrawColor(1);
    return u;
}

// 반전 이름표는 화면 맨 아래(y 114~127)에 둔다 — 세로로 쥐면 버튼 쪽 끝이라 눈에 덜 걸린다
static U8G2& withHeader(int i, const char* head) {
    U8G2& u = blank(i);
    u.drawBox(0, 114, W, 14);
    u.setDrawColor(0);
    u.setFont(u8g2_font_6x10_tf);
    center(u, 125, head);
    u.setDrawColor(1);
    return u;
}

static void caption(U8G2& u, const char* t) {
    u.setFont(u8g2_font_helvB08_tr);
    center(u, 10, t);   // 화면 맨 위
}

static void thickLine(U8G2& u, int x0, int y0, int x1, int y1) {
    for (int d = -2; d <= 2; d++) u.drawLine(x0 + d, y0, x1 + d, y1);
}

// 굵은 화살표 (머리 삼각형 + 몸통 사각형), 중심 y≈64
static void drawArrow(U8G2& u, Arrow a) {
    switch (a) {
        case Arrow::Up:    u.drawTriangle(32, 34, 8, 64, 56, 64);  u.drawBox(22, 64, 20, 30); break;
        case Arrow::Down:  u.drawTriangle(32, 94, 8, 64, 56, 64);  u.drawBox(22, 34, 20, 30); break;
        case Arrow::Left:  u.drawTriangle(4, 64, 32, 40, 32, 88);  u.drawBox(32, 54, 26, 20); break;
        case Arrow::Right: u.drawTriangle(60, 64, 32, 40, 32, 88); u.drawBox(6, 54, 26, 20);  break;
    }
}

// 타이머 아이콘: 시계 + 위쪽 단추
static void drawTimer(U8G2& u) {
    u.drawBox(28, 36, 8, 5);
    u.drawDisc(32, 66, 22);
    u.setDrawColor(0);
    u.drawDisc(32, 66, 18);
    u.setDrawColor(1);
    u.drawBox(31, 52, 3, 15);   // 분침
    u.drawBox(31, 64, 12, 3);   // 시침
}

static void drawCamera(U8G2& u) {
    u.drawBox(22, 44, 20, 8);         // 셔터 돌기
    u.drawRBox(6, 50, 52, 34, 5);     // 몸체
    u.setDrawColor(0);
    u.drawDisc(32, 67, 12);           // 렌즈 테두리
    u.setDrawColor(1);
    u.drawDisc(32, 67, 7);
}

// 이름은 왼쪽, 값은 오른쪽 정렬로 한 줄
static void nameValue(U8G2& u, int y, const char* name, const char* val) {
    u.drawStr(2, y, name);
    u.drawStr(W - 2 - u.getStrWidth(val), y, val);
}

static const char* signalName(Signal s) {
    switch (s) {
        case Signal::Wheel: return "Wheel";
        case Signal::Arrow: return "Arrow";
        case Signal::Page:  return "PgUp/Dn";
        default:            return "Volume";   // Volume, Camera
    }
}

// ---------- 화면 ----------
void begin(ds::DisplayCore& d) { display = &d; }

void drawSplash() {
    U8G2& u1 = blank(0);
    drawArrow(u1, Arrow::Left);
    U8G2& u2 = blank(1);
    drawArrow(u2, Arrow::Right);
    U8G2& u3 = blank(2);
    u3.setFont(u8g2_font_helvB10_tr);
    center(u3, 56, "Page");
    center(u3, 76, "Turner");
    U8G2& u4 = blank(3);
    u4.setFont(u8g2_font_6x10_tf);
    center(u4, 66, "v" PT_VERSION);
    display->push();
}

// BTN1/BTN2: 페이지 넘김(웹소설·PPT) ←/→, 스크롤 ↑/↓, 카메라는 셔터/딜레이 셔터
static void drawActionButtons(const Mode& m, const ShutterTimer& timer) {
    U8G2& u1 = withHeader(0, "BTN1");
    U8G2& u2 = withHeader(1, "BTN2");
    if (m.signal == Signal::Camera) {
        drawCamera(u1);
        caption(u1, "Shutter");
        char buf[16];
        if (timer.running()) {   // 남은 초를 크게
            snprintf(buf, sizeof(buf), "%d", timer.secondsLeft());
            u2.setFont(u8g2_font_logisoso32_tn);
            center(u2, 82, buf);
            caption(u2, "Cancel");
        } else {
            drawTimer(u2);
            snprintf(buf, sizeof(buf), "Timer %ds", valueOf(m.settings[0]));
            caption(u2, buf);
        }
    } else if (m.signal == Signal::Volume || m.signal == Signal::Page) {
        drawArrow(u1, Arrow::Left);
        caption(u1, "Prev page");
        drawArrow(u2, Arrow::Right);
        caption(u2, "Next page");
    } else {
        drawArrow(u1, Arrow::Up);
        caption(u1, "Scroll up");
        drawArrow(u2, Arrow::Down);
        caption(u2, "Scroll down");
    }
}

void drawNormal(int modeIdx, int slot, bool connected, const ShutterTimer& timer) {
    const Mode& m = modeAt(modeIdx);
    char buf[16];
    drawActionButtons(m, timer);

    snprintf(buf, sizeof(buf), "MODE %d/%d", modeIdx + 1, kNumModes);
    U8G2& u3 = withHeader(2, buf);
    u3.setFont(u8g2_font_helvB10_tr);
    center(u3, m.line2[0] ? 56 : 66, m.line1);
    center(u3, 76, m.line2);
    caption(u3, "Change mode");

    // OLED4: 보내는 신호 종류 + 설정값 + Beep
    snprintf(buf, sizeof(buf), connected ? "BT%d ON" : "BT%d wait", slot + 1);
    U8G2& u4 = withHeader(3, buf);
    u4.setFont(u8g2_font_helvB10_tr);
    center(u4, 32, signalName(m.signal));
    u4.drawHLine(4, 37, W - 8);
    u4.setFont(u8g2_font_6x10_tf);
    int y = 52;
    for (int i = 0; i < m.numSettings; i++, y += 14) {
        formatValue(buf, sizeof(buf), m.settings[i], m.settings[i].idx);
        nameValue(u4, y, m.settings[i].name, buf);
    }
    const Setting& beep = beepSetting();
    formatValue(buf, sizeof(buf), beep, beep.idx);
    nameValue(u4, y, beep.name, buf);
    caption(u4, "Settings");
    display->push();
}

void drawMenu(const SettingsMenu& menu) {
    const int k = menu.current();
    const Setting& s = menu.item(k);
    const uint8_t idx = menu.pending(k);
    char buf[16];

    // OLED1: 무엇을 설정하는지
    snprintf(buf, sizeof(buf), "SET %d/%d", k + 1, menu.itemCount());
    U8G2& u1 = withHeader(0, buf);
    u1.setFont(u8g2_font_helvB10_tr);
    center(u1, 34, s.name);
    u1.setFont(u8g2_font_5x8_tf);
    center(u1, 50, s.desc1);
    center(u1, 60, s.desc2);

    // OLED2: 고르는 값
    U8G2& u2 = withHeader(1, "VALUE");
    formatValue(buf, sizeof(buf), s, idx);
    u2.setFont(u8g2_font_helvB14_tr);
    center(u2, 42, buf);

    if (s.unit == Unit::OnOff || s.unit == Unit::Hide) {
        // 두 값 항목(Beep·Cursor): BTN1 = 첫 값(Off/Show), BTN2 = 둘째 값(On/Hide), 고른 쪽을 반전
        for (int v = 0; v < 2; v++) {
            U8G2& u = v ? u2 : u1;
            formatValue(buf, sizeof(buf), s, v);
            u.setFont(u8g2_font_helvB14_tr);
            if (idx == v) {
                u.drawRBox(4, 78, W - 8, 30, 4);
                u.setDrawColor(0);
            } else {
                u.drawRFrame(4, 78, W - 8, 30, 4);
            }
            center(u, 100, buf);
            u.setDrawColor(1);
        }
    } else {
        // 여러 값 항목: BTN1 줄이기(−), BTN2 위치 막대 + 늘리기(+)
        u1.drawBox(12, 84, 40, 10);
        caption(u1, "Less");
        u2.drawFrame(4, 52, 56, 7);
        u2.drawBox(4, 52, 56 * (idx + 1) / s.numValues, 7);
        u2.drawBox(12, 84, 40, 10);
        u2.drawBox(27, 69, 10, 40);
        caption(u2, "More");
    }

    // OLED3: 취소(X)
    U8G2& u3 = withHeader(2, "BTN3");
    thickLine(u3, 16, 46, 48, 82);
    thickLine(u3, 48, 46, 16, 82);
    caption(u3, "Cancel");

    // OLED4: 다음 항목(▶) 또는 저장(✓)
    const bool last = k + 1 >= menu.itemCount();
    U8G2& u4 = withHeader(3, "BTN4");
    if (last) {
        thickLine(u4, 12, 66, 26, 82);
        thickLine(u4, 26, 82, 52, 46);
    } else {
        u4.drawTriangle(18, 42, 18, 86, 50, 64);
    }
    caption(u4, last ? "Save" : "Next item");
    display->push();
}

void drawPairing(int slot, uint8_t secondsLeft) {
    char buf[12];
    blank(0);
    blank(1);
    U8G2& u3 = withHeader(2, "PAIRING");
    u3.setFont(u8g2_font_6x10_tf);
    center(u3, 52, "Pair a new");
    center(u3, 66, "device with");
    center(u3, 80, PT_BLE_NAME_SHORT);
    if (secondsLeft) caption(u3, "Cancel");   // BTN3 짧게 = 취소하고 이전 슬롯으로
    snprintf(buf, sizeof(buf), "SLOT %d", slot + 1);
    U8G2& u4 = withHeader(3, buf);
    if (secondsLeft) {
        snprintf(buf, sizeof(buf), "%d", secondsLeft);
        u4.setFont(u8g2_font_logisoso32_tn);
        center(u4, 82, buf);
    }
    caption(u4, "Waiting");
    display->push();
}

// 기기(슬롯) 선택: OLED1~3 = 슬롯 1~3 (옆 버튼으로 바로 고름), OLED4 = 취소
void drawDevices(int selected, int active, const bool paired[3]) {
    char buf[12];
    for (int s = 0; s < 3; s++) {
        snprintf(buf, sizeof(buf), "SLOT %d", s + 1);
        U8G2& u = withHeader(s, buf);
        // 고른 슬롯은 상자 전체를 반전 (fg = 글자 색, bg = 상자 안 바탕 색)
        const uint8_t fg = (s == selected) ? 0 : 1;
        if (s == selected) u.drawRBox(2, 20, W - 4, 52, 4);
        else u.drawRFrame(2, 20, W - 4, 52, 4);
        snprintf(buf, sizeof(buf), "%d", s + 1);
        u.setFont(u8g2_font_logisoso20_tn);
        if (s == active) {   // 쓰는 중인 슬롯은 번호 칸을 한 번 더 반전해 눈에 띄게
            u.setDrawColor(fg);
            u.drawRBox(14, 25, 36, 27, 3);
            u.setDrawColor(1 - fg);
        } else {
            u.setDrawColor(fg);
        }
        center(u, 48, buf);
        u.setDrawColor(fg);
        u.setFont(u8g2_font_6x10_tf);
        center(u, 64, s == active ? "In use" : (paired[s] ? "Paired" : "Empty"));
        u.setDrawColor(1);
        u.setFont(u8g2_font_5x8_tf);
        center(u, 88, s == selected ? "Press again" : "Press");
        center(u, 98, s == selected ? "to use" : "to choose");
        center(u, 110, "Hold: new");
    }
    U8G2& u4 = withHeader(3, "BTN4");
    thickLine(u4, 16, 46, 48, 82);
    thickLine(u4, 48, 46, 16, 82);
    caption(u4, "Cancel");
    display->push();
}

void drawSleep() {
    blank(0);
    blank(1);
    U8G2& u3 = blank(2);
    u3.setFont(u8g2_font_helvB10_tr);
    center(u3, 66, "Sleep");
    U8G2& u4 = blank(3);
    u4.setFont(u8g2_font_6x10_tf);
    center(u4, 56, "BTN1/2");
    center(u4, 70, "to wake");
    display->push();
}

void setScreensOn(bool on) {
    display->waitIdle();   // HW 전송 태스크가 버스를 쓰는 중이면 기다린다
    for (int i = 0; i < ds::kNumScreens; i++) display->screen(i).setPowerSave(on ? 0 : 1);
}

}  // namespace view
