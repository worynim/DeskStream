// worynim@gmail.com
/**
 * @file display_manager.cpp
 * @brief 고수준 디스플레이 및 UI 스테이지 관리 클래스 구현
 * @details 4개 OLED 디스플레이 제어, 부저 피드백, UI 상태 전환 로직 구현
 */
#include "display_manager.h"
#include "config.h"          // FONT_SLOT_COUNT
#include "renderer_layout.h" // layoutIsDayText() — 제목 화면의 우측 정렬 판정
#include "LittleFS.h"

DisplayManager display;

// 시보 표시용 종 모양 아이콘 (8x8)
static const uint8_t bell_icon[] = { 0x18, 0x3C, 0x3C, 0x3C, 0xFF, 0xDB, 0x18, 0x00 };

/**
 * @brief 눈 조립용 결정적 시드 조합
 * @details 전환마다 transitionId가 증가하므로 매초 다른 눈이 오고,
 *         같은 화면 안에서도 문자 슬롯과 x 위치에 따라 시선이 흩어진다.
 *         조립(k)과 소멸(k + offset) 시드가 겹치지 않도록 소멸 슬롯만 뒤로 민다.
 */
#define SNOW_DISPERSE_SLOT_OFFSET 8

static uint16_t snowSeed(uint8_t transitionId, int screenIdx, int slot, int x) {
    return (uint16_t)(transitionId * 7919 + screenIdx * 131 + slot * 17 + x * 3);
}

// DisplayManager 메서드 구현
DisplayManager::DisplayManager() : screens(core.screens()) {}

void DisplayManager::begin() {
    // 1. 화면 엔진 (I2C 버스·HW 태스크·U8G2 4개) — 기본값 = HW 1.1MHz·병합 전송·SW 100ns, U8G2_R2
    core.begin();

    // 2. 렌더러 초기화
    renderer.setScreens(screens);

    // 3. 설정 매니저 초기화 및 로드
    configManager.begin();
    
    for (int i = 0; i < 4; i++) {
        screens[i]->setBitmapMode(1);
        screens[i]->clearBuffer();
        lastTexts[i] = "";
    }
    // is_flipped는 **하드웨어 flip 명령**과 **화면 배정**(isTitleScreenOf 등) 두 곳에 쓰인다.
    //   여기서는 앞의 역할만 한다 — 뒤집힌 장착을 상쇄하는 물리 방향 보정이다.
    //   u8g2는 초기화 시 패널을 0xa1/0xc8(두 축 미러)로 두고, U8G2_R2가 버퍼를
    //   180° 돌린다. 두 변환이 상쇄되어 **false가 정방향**이 된다.
    //   ⚠ true로 두면 0xa0/0xc0(미러 없음)이 초기 미러를 취소해 R2의 180°가 남는다.
    core.setFlip(configManager.get().is_flipped);
    core.setInversion(configManager.get().is_inverted);
    core.setBrightness(configManager.get().brightness);
    pushParallel();

    _buzzer.begin();

    // 슬롯 이름 초기 캐싱
    for (int i = 0; i < FONT_SLOT_COUNT; i++) {
        String p = "/f" + String(i) + "/name.txt";
        if (LittleFS.exists(p)) {
            File f = LittleFS.open(p, "r");
            if (f) {
                _slotNames[i] = f.readString();
                f.close();
            }
        } else {
            _slotNames[i] = "Empty Slot";   // [A-4 수정] setFontSlot()의 빈 슬롯 표기와 같은 문자열
        }
    }
}

void DisplayManager::setFlipDisplay(bool flip) {
    configManager.get().is_flipped = flip;
    configManager.setDirty();
    core.setFlip(flip);
    setForceUpdate(true);
}

void DisplayManager::setChime(bool enable) {
    configManager.get().chime_enabled = enable;
    configManager.setDirty();
    setForceUpdate(true);
}

void DisplayManager::setForceUpdate(bool force) {
    _needsForceUpdate = force;
}

bool DisplayManager::checkForceUpdate() {
    bool f = _needsForceUpdate;
    _needsForceUpdate = false;
    return f;
}

void DisplayManager::setDisplayMode(uint8_t mode) {
    configManager.get().display_mode = mode;
    configManager.setDirty();
    setForceUpdate(true);
}

void DisplayManager::setHourFormat(uint8_t format) {
    configManager.get().hour_format = format;
    configManager.setDirty();
    setForceUpdate(true);
}

void DisplayManager::setAnimMode(uint8_t mode) {
    configManager.get().anim_mode = mode;
    configManager.setDirty();
    setForceUpdate(true);
}

void DisplayManager::applyTimezone() {
    // 실제 TZ 적용과 SNTP (재)시작은 time_sync 서비스가 한다.
    //   [2026-11-07] 주기 재동기화·WiFi 복구·TZ 변경이 **같은 경로**를 쓰도록 옮겼다.
    //   경로가 둘로 갈라지면 한쪽만 고쳐져 "웹에서 TZ를 바꾸면 동기화가 멈춘다" 같은
    //   결함이 생긴다. configTime(오프셋,...)이 TZ를 UTC로 덮어쓰는 함정은
    //   time_sync.cpp의 startSntp() 주석 참조.
    timeSync.applyTimezone();
    setForceUpdate(true);   // 다음 루프에서 새 시각으로 즉시 재렌더링
}

String DisplayManager::getSlotName(uint8_t slot) {
    if (slot >= FONT_SLOT_COUNT) return "";
    return _slotNames[slot];
}

/**
 * @brief 이름표를 **지정한 슬롯**에 쓴다 — 파일·캐시·(현재 슬롯이면) config까지
 * @details [A-2③④ 수정 — 2026-10-06, 중국어판 §12.13/§12.14 승계] 검증과 파일 쓰기를
 *          이 한 곳에 모은다. 검증을 두 곳에 두면 한쪽만 고쳐졌을 때 어느 쪽이 진짜
 *          규칙인지 알 수 없다.
 */
bool DisplayManager::setSlotName(uint8_t slot, const String& name) {
    if (slot >= FONT_SLOT_COUNT) return false;

    // [리뷰 §3.3 승계 / A-2④ 수정] 이 값은 (1) JSON 응답에 붙고 (2) "/fN/name.txt" 의
    //   **내용**으로 쓰인다. 경로는 리터럴이라 이름이 경로 조작을 일으키지 않으므로
    //   (config.h FONT_NAME_MAX_LEN 주석 참조) 상한은 **길이 폭주 방지** 목적이다.
    //   ⚠ 예전의 `>= 32`(실질 31바이트)는 "이름을 파일명으로도 쓴다"는 잘못된 전제에서
    //     나온 값이었고, 실제 폰트 이름을 **조용히** 거부해 슬롯이 "Empty Slot"으로 보이게 했다.
    //   `"` `\` `/` 제어문자 금지는 그대로 둔다 — 이름은 사람이 읽는 값이고 여기 들어올
    //   이유가 없다 (경로 조작 방어가 아니라 **출력 위생** 목적이다).
    if (name.length() == 0 || name.length() > FONT_NAME_MAX_LEN) return false;
    for (size_t i = 0; i < name.length(); i++) {
        const char c = name[i];
        if (c == '"' || c == '\\' || c == '/' || c < 0x20) return false;
    }

    // 이미 그 슬롯의 이름이면 파일을 다시 쓰지 않는다 — 글리프 업로드가 40회 부른다.
    if (_slotNames[slot] == name) return true;

    const String path = "/f" + String(slot) + "/name.txt";
    File f = LittleFS.open(path, "w");
    if (!f) return false;      // 슬롯 폴더가 없거나 열 수 없다 — 캐시를 갱신하지 않고 알린다
    f.print(name);
    f.close();

    _slotNames[slot] = name;   // 드롭다운이 읽는 곳

    // 현재 슬롯의 이름표를 바꾼 경우에만 config까지 — 다른 슬롯을 고치는 것이
    //   지금 화면의 "현재 적용 폰트" 표시를 흔들면 안 된다.
    if (configManager.get().font_slot == slot && configManager.get().font_name != name) {
        configManager.get().font_name = name;
        configManager.setDirty();
    }
    return true;
}

void DisplayManager::setFontName(const String& name) {
    setSlotName(configManager.get().font_slot, name);
}

void DisplayManager::setFontSlot(uint8_t slot) {
    if (slot >= FONT_SLOT_COUNT) slot = 0;
    
    // 이미 해당 슬롯이면 중복 로딩 방지
    if (configManager.get().font_slot == slot && renderer.isCacheLoaded()) {
        return;
    }
    
    configManager.get().font_slot = slot;
    
    // 새 슬롯의 이름 로드 — config(현재 이름)와 _slotNames(슬롯 목록용)를 **같이** 갱신한다.
    //   [A-3 수정] 예전엔 configManager.font_name만 갱신했으므로, 슬롯을 옮겨도
    //   /api/config가 내보내는 slotNames는 **부팅 시점에 읽은 값**으로 남아 있었다.
    String path = "/f" + String(slot) + "/name.txt";
    String name;
    if (LittleFS.exists(path)) {
        File f = LittleFS.open(path, "r");
        if (f) {
            name = f.readString();
            f.close();
        }
    }
    // 파일이 없거나(빈 슬롯) 열지 못했거나(내용 없음) — 셋을 같은 문자열로 좁힌다.
    //   [A-4 수정] 부팅 캐시의 빈 슬롯 표기와 **같은 문자열**이어야 한다.
    //   다른 이름이 섞이면 같은 상태가 두 이름으로 보인다.
    if (name.length() == 0) name = "Empty Slot";
    configManager.get().font_name = name;
    _slotNames[slot] = name;
    
    configManager.setDirty();
    
    // 애니메이션 중이면 즉시 중단 (폰트 맵이 바뀌면 애니메이션 데이터가 무효화됨)
    if (_animState.active) {
        _animState.active = false;
        refreshNow();
    }
    
    // 렌더러 캐시 갱신
    renderer.loadBitmapCache(slot);
    
    // 폰트 로드 후 화면 강제 갱신 (잔상 제거)
    refreshNow();
    
    setForceUpdate(true);
}

void DisplayManager::refreshNow() {
    for (int i = 0; i < 4; i++) drawCenterText(i, lastTexts[i]);
    pushParallel();
}

void DisplayManager::setInversion(bool invert) {
    configManager.get().is_inverted = invert;
    configManager.setDirty();
    core.setInversion(invert);
    setForceUpdate(true);
}

void DisplayManager::setBrightness(uint8_t brightness) {
    configManager.get().brightness = brightness;
    configManager.setDirty();
    core.setBrightness(brightness);
}

// saveConfig()은 ConfigManager로 대체됨

void DisplayManager::loadBitmapCache() {
    renderer.loadBitmapCache();
}

// findChar, getHexKey 로직은 Renderer로 이동됨

void DisplayManager::clearAll() {
    core.clearAll();       // 버퍼 지우기 + 셰도 무효화 → 모든 0픽셀 강제 전송 (잔상 제거)
    setForceUpdate(true);  // 즉시 다음 프레임 그리기 예약
}

void DisplayManager::beep(int duration, int freq) {
    _buzzer.beep(duration, freq);
}

void DisplayManager::setYieldCallback(void (*cb)()) {
    on_yield_callback = cb;
}

// === 화면 판정 규칙 (단일 진실원) ===
// [리뷰 §2.2] 아래 식이 display_manager.cpp 안에 3번 복제돼 있었다.

/** @brief 이 화면이 제목(자릿수) 화면인가 — 뒤집기면 3번 화면이 자릿수다 */
static bool isTitleScreenOf(int idx) {
    return configManager.get().is_flipped ? (idx == 3) : (idx == 0);
}

// drawDitheredChar, drawZoomedChar, drawSingleChar, drawScaledChar, getCharData 로직 Renderer로 이관됨
void DisplayManager::drawChimeIcon(int idx) {
    if (isTitleScreenOf(idx) && configManager.get().chime_enabled) screens[idx]->drawXBM(0, 0, 8, 8, bell_icon);
}

int DisplayManager::findOldIndexAtX(const ScreenAnimData& sd, int x) const {
    for (int k = 0; k < sd.oldCount; k++) {
        if (sd.oldChars[k].x == x) return k;
    }
    return -1;
}

int DisplayManager::findNewIndexAtX(const ScreenAnimData& sd, int x) const {
    for (int j = 0; j < sd.newCount; j++) {
        if (sd.newChars[j].x == x) return j;
    }
    return -1;
}


/**
 * @brief 이 화면의 텍스트를 우측 정렬하는가 (단일 진실원)
 * @details 24시간제의 제목 화면은 날짜("...일")를 보여준다. 시·분·초 화면은
 *          layoutCharX(count, j, false)가 마지막 글자를 우측 고정 열(x=96)에 붙이는데,
 *          제목 화면만 가운데 정렬이라 **날짜의 "일"만 우측 끝에 붙지 않아** 정렬이
 *          어긋나 보였다. 그래서 제목 화면의 날짜를 시·분·초와 같은 우측 정렬로 보낸다.
 *          날짜 판정은 layoutIsDayText()(순수 모듈)가 한다 — 12시간제의 "오전"/"오후"는
 *          단위 "일"로 끝나지 않으므로 가운데 정렬이 그대로 유지된다.
 * @note 정적 경로(drawCenterText)·애니메이션 경로(updateAll)·refreshNow가 **모두**
 *       isCenteredText()를 거치므로, 이 갈래를 우회하는 새 경로를 만들면 글자 위치가
 *       어긋난다. 새 경로도 반드시 이 두 함수를 쓴다.
 */
static bool isRightAlignedText(int idx, const String& text) {
    return isTitleScreenOf(idx) && layoutIsDayText(text.c_str());
}

/**
 * @brief 이 화면의 텍스트를 가운데 정렬하는가 (단일 진실원)
 * @details [리뷰 §2.2] 제목 화면 판정과 '정각' 강제 정렬이 4곳에 복제돼 있었다.
 *          drawCenterText() 안에 `|| (text == "정각")`가 있고, updateAll()의
 *          세 호출부는 drawCenterText()를 거치지 않고 getCharData()를 직접 불러
 *          같은 규칙을 각자 다시 구현했다. 한 곳이 바뀌면 나머지가 어긋난다.
 *          우측 정렬 갈래(제목 화면의 날짜)는 isRightAlignedText()가 판정한다.
 * @param idx 화면 번호 (is_flipped면 3번이 제목)
 * @param text 이 화면에 그릴 문자열
 */
static bool isCenteredText(int idx, const String& text) {
    return !isRightAlignedText(idx, text) && (isTitleScreenOf(idx) || (text == "정각"));
}

/**
 * @brief 한 화면을 문자열로 즉시 그린다 (애니메이션 없는 최종 상태)
 * @details 가운데 정렬 여부는 isCenteredText()가 정한다. 호출부가 그 값을
 *          따로 만들어 넘기면 규칙이 두 곳으로 갈라지므로, 여기서는 받지 않는다.
 */
void DisplayManager::drawCenterText(int idx, const String& text) {
    U8G2* u8g2 = screens[idx];
    u8g2->clearBuffer();
    CharData chars[LAYOUT_MAX_CHARS]; int count;
    renderer.getCharData(text, chars, count, isCenteredText(idx, text));
    for (int i = 0; i < count; i++) {
        renderer.drawSingleChar(idx, chars[i].c, chars[i].x, 0);
    }
    drawChimeIcon(idx);
}

extern int uiStage;

void DisplayManager::updateAll(String inTexts[4], bool force) {
    String texts[4];
    bool changed[4] = {false, false, false, false};
    bool anyChanged = false;
    
    for (int i = 0; i < 4; i++) {
        texts[i] = configManager.get().is_flipped ? inTexts[3 - i] : inTexts[i];
        if (force || texts[i] != lastTexts[i]) { changed[i] = true; anyChanged = true; }
    }
    if (!anyChanged) return;

    if (configManager.get().anim_mode == ANIMATION_TYPE_NONE) {
        for (int i = 0; i < 4; i++) {
            if (changed[i]) {
                drawCenterText(i, texts[i]);
                lastTexts[i] = texts[i];
            }
        }
        pushParallel();
        return;
    }

    // [Step 3.1] 비차단 애니메이션 상태 초기화
    bool isSnow = (configManager.get().anim_mode == ANIMATION_TYPE_SNOW_ASSEMBLE);

    // 이전 전환이 끝나기 전에 새 전환이 시작될 수 있다(매초 호출되기 때문).
    // 이번에 변경되지 않은 화면이 이전 전환에서 애니메이션 중이었다면, 아래 루프에서
    // changed가 false로 덮어써져 렌더링이 완전히 멈추고 중간 프레임이 화면에 남는다.
    // 그 잔상을 먼저 정착된 최종 상태로 지운다.
    for (int i = 0; i < 4; i++) {
        if (_animState.screens[i].changed && !changed[i]) drawCenterText(i, lastTexts[i]);
    }

    _animState.active = true;
    _animState.currentStep = 0;
    _animState.maxStep = isSnow ? ANIMATION_STEPS_SNOW : ANIMATION_STEPS_DEFAULT;
    _animState.transitionId++;      // 이번 전환에만 쓰이는 눈 시드
    _animState.lastUpdateMs = 0; // 즉시 첫 틱 실행

    for (int i = 0; i < 4; i++) {
        _animState.screens[i].changed = changed[i];
        if (changed[i]) {
            // 정적 경로·drawCenterText()와 **같은 규칙**을 쓴다 (isCenteredText 단일 진실원).
            renderer.getCharData(lastTexts[i], _animState.screens[i].oldChars,
                                 _animState.screens[i].oldCount, isCenteredText(i, lastTexts[i]));
            renderer.getCharData(texts[i], _animState.screens[i].newChars,
                                 _animState.screens[i].newCount, isCenteredText(i, texts[i]));
            lastTexts[i] = texts[i]; // 타겟 텍스트 선점
        }
    }
}

void DisplayManager::updateTick() {
    if (!_animState.active) return;

    // [Step 4.1] 시계 모드가 아니면 애니메이션 중단 (UI 전환 대응)
    if (uiStage != 0) {
        _animState.active = false;
        return;
    }

    unsigned long now = millis();
    unsigned long stepDelay = (configManager.get().anim_mode == ANIMATION_TYPE_SNOW_ASSEMBLE)
                              ? ANIMATION_STEP_DELAY_SNOW_MS : ANIMATION_STEP_DELAY_MS;
    if (now - _animState.lastUpdateMs < stepDelay) {
        // 대기 시간 동안 yield 콜백 실행하여 타 서비스 기회 제공
        if (on_yield_callback) on_yield_callback();
        return;
    }

    _animState.lastUpdateMs = now;
    _animState.currentStep++;

    if (_animState.currentStep > _animState.maxStep) {
        _animState.active = false;
        return;
    }

    for (int i = 0; i < 4; i++) {
        if (_animState.screens[i].changed) {
            renderAnimFrame(i, _animState.currentStep);
        }
    }
    pushParallel();
}

void DisplayManager::renderAnimFrame(int i, int step) {
    uint8_t mode = configManager.get().anim_mode;

    // 눈 조립(픽셀 단위)과 분할 플랩(한 열 안에서 옛·새 글자가 함께 접힘)은
    // 기존 글자 단위 전환 경로로는 표현할 수 없어 각각 분리한다.
    if (mode == ANIMATION_TYPE_SNOW_ASSEMBLE) { renderSnowFrame(i, step); return; }
    if (mode == ANIMATION_TYPE_SPLIT_FLAP)   { renderFlapFrame(i, step);   return; }

    ScreenAnimData& sd = _animState.screens[i];
    screens[i]->clearBuffer();

    for (int j = 0; j < sd.newCount; j++) {
        String nc = sd.newChars[j].c; int nx = sd.newChars[j].x;
        int oi = findOldIndexAtX(sd, nx);
        String oc = (oi >= 0) ? sd.oldChars[oi].c : "";

        if (oc == nc) {
            renderer.drawSingleChar(i, nc, nx, 0);
        } else {
            switch (mode) {
                case ANIMATION_TYPE_SCROLL_UP:
                    if (step < 16 && oc != "") renderer.drawSingleChar(i, oc, nx, -(step * 4));
                    renderer.drawSingleChar(i, nc, nx, 64 - (step * 4));
                    break;
                case ANIMATION_TYPE_SCROLL_DOWN:
                    if (step < 16 && oc != "") renderer.drawSingleChar(i, oc, nx, (step * 4));
                    renderer.drawSingleChar(i, nc, nx, -64 + (step * 4));
                    break;
                case ANIMATION_TYPE_VERTICAL_FLIP:
                    if (step <= 8) { if (oc != "") renderer.drawScaledChar(i, oc, nx, ((8 - step) * 64) / 8); }
                    else { renderer.drawScaledChar(i, nc, nx, ((step - 8) * 64) / 8); }
                    break;
                case ANIMATION_TYPE_DITHERED_FADE:
                    if (step <= 8) { if (oc != "") renderer.drawDitheredChar(i, oc, nx, 16 - (step * 2)); }
                    else { renderer.drawDitheredChar(i, nc, nx, (step - 8) * 2); }
                    break;
                case ANIMATION_TYPE_ZOOM:
                    if (step <= 8) { if (oc != "") renderer.drawZoomedChar(i, oc, nx, ((8 - step) * 100) / 8); }
                    else {
                        int sc = (step <= 12) ? ((step - 8) * 150 / 4) : (150 - (step - 12) * 50 / 4);
                        renderer.drawZoomedChar(i, nc, nx, sc);
                    }
                    break;
            }
        }
    }

    for (int k = 0; k < sd.oldCount; k++) {
        int ox = sd.oldChars[k].x; String oc = sd.oldChars[k].c;
        if (findNewIndexAtX(sd, ox) >= 0) continue;

        switch (mode) {
            case ANIMATION_TYPE_SCROLL_UP:   if (step < 16) renderer.drawSingleChar(i, oc, ox, -(step * 4)); break;
            case ANIMATION_TYPE_SCROLL_DOWN: if (step < 16) renderer.drawSingleChar(i, oc, ox, (step * 4)); break;
            case ANIMATION_TYPE_VERTICAL_FLIP: if (step <= 8) renderer.drawScaledChar(i, oc, ox, ((8 - step) * 64) / 8); break;
            case ANIMATION_TYPE_DITHERED_FADE: if (step <= 8) renderer.drawDitheredChar(i, oc, ox, 16 - (step * 2)); break;
            case ANIMATION_TYPE_ZOOM:          if (step <= 8) renderer.drawZoomedChar(i, oc, ox, ((8 - step) * 100) / 8); break;
        }
    }

    drawChimeIcon(i);
}

void DisplayManager::renderSnowFrame(int i, int step) {
    ScreenAnimData& sd = _animState.screens[i];
    uint8_t progress = (uint8_t)((int)step * ANIM_PROGRESS_FULL / (int)_animState.maxStep);
    screens[i]->clearBuffer();

    // 새 글자: 눈송이가 떨어져 쌓이며 조립된다.
    for (int j = 0; j < sd.newCount; j++) {
        int nx = sd.newChars[j].x;
        int oi = findOldIndexAtX(sd, nx);
        if (oi >= 0 && sd.oldChars[oi].c == sd.newChars[j].c) {
            renderer.drawSingleChar(i, sd.newChars[j].c, nx, 0); // 그대로인 글자는 움직이지 않는다
            continue;
        }
        uint16_t seed = snowSeed(_animState.transitionId, i, j, nx);
        renderer.drawAssemblingChar(i, sd.newChars[j].c, nx, progress, seed);
    }

    // 사라지는 옛 글자: 눈처럼 아래로 가라앉으며 흩어진다.
    // 같은 자리를 새 글자가 넘겨받더라도 함께 가라앉는다. 겹쳐 그려도 단색 픽셀이므로 문제가 없다.
    for (int k = 0; k < sd.oldCount; k++) {
        int ox = sd.oldChars[k].x;
        int ni = findNewIndexAtX(sd, ox);
        if (ni >= 0 && sd.newChars[ni].c == sd.oldChars[k].c) continue; // 같은 글자는 이미 정적으로 그렸다
        uint16_t seed = snowSeed(_animState.transitionId, i, k + SNOW_DISPERSE_SLOT_OFFSET, ox);
        renderer.drawDispersingChar(i, sd.oldChars[k].c, ox, progress, seed);
    }

    drawChimeIcon(i);
}

void DisplayManager::renderFlapFrame(int i, int step) {
    ScreenAnimData& sd = _animState.screens[i];
    uint8_t progress = (uint8_t)((int)step * ANIM_PROGRESS_FULL / (int)_animState.maxStep);
    screens[i]->clearBuffer();

    // 새 글자가 놓인 열: 옛 글자와 새 글자의 접힘을 한 번의 호출로 함께 그린다.
    for (int j = 0; j < sd.newCount; j++) {
        int nx = sd.newChars[j].x;
        int oi = findOldIndexAtX(sd, nx);
        String oc = (oi >= 0) ? sd.oldChars[oi].c : "";
        if (oc == sd.newChars[j].c) {
            renderer.drawSingleChar(i, sd.newChars[j].c, nx, 0); // 그대로인 글자는 움직이지 않는다
            continue;
        }
        renderer.drawFlapChar(i, oc, sd.newChars[j].c, nx, progress);
    }

    // 새 글자가 없는 열은 사라지는 글자이므로 접어 닫는다.
    for (int k = 0; k < sd.oldCount; k++) {
        int ox = sd.oldChars[k].x;
        if (findNewIndexAtX(sd, ox) >= 0) continue; // 새 글자가 있는 열은 위에서 함께 접었다
        renderer.drawFlapChar(i, sd.oldChars[k].c, "", ox, progress);
    }

    drawChimeIcon(i);
}

// 멜로디는 ds::Buzzer가 타이머로 재생한다 — 예전 tone()+delay()처럼 메인 루프를 막지 않는다.
//   각 음 = {주파수, 소리 길이, 다음 음까지 쉼} — 예전 delay 간격과 같은 박자다.
static const ds::Note STARTUP_MELODY[] = {   // Do-Mi-Sol-Do
    {2093, 100, 20}, {2637, 100, 20}, {3136, 100, 20}, {4186, 100, 20},
};
static const ds::Note CHIME_MELODY[] = {     // 경쾌한 '띠링~' (C7 → G7)
    {2093, 80, 20}, {3136, 150, 10},
};

void DisplayManager::playStartupMelody() {
    _buzzer.playMelody(STARTUP_MELODY, sizeof(STARTUP_MELODY) / sizeof(STARTUP_MELODY[0]));
}

void DisplayManager::playChimeMelody() {
    _buzzer.playMelody(CHIME_MELODY, sizeof(CHIME_MELODY) / sizeof(CHIME_MELODY[0]));
}


// IP 화면 도트 상수 — 마지막 숫자 옆("192.")에, 숫자 **밑동(baseline)** 에 맞춰 찍는다.
//   [2026-10-21 사용자 보고] 이전에는 도트를 화면 오른쪽 아래(120,56)에 고정해 찍었다.
//   숫자는 화면 세로 중앙인데 점만 아래 모서리에 있어 "줄바꿈"처럼 보였다.
//   도트를 숫자 뒤 인라인으로 옮기고, 세로는 실제 잉크 하단(=baseline)에 맞춘다.
static const int IP_DOT_SIZE    = 4;   // 점 한 변의 픽셀 수
static const int IP_DOT_GAP     = 8;   // 마지막 숫자 칸과 점 사이의 가로 간격
static const int IP_TITLE_BASELINE_Y = 10;   // 6x10 폰트의 baseline (y=7이면 윗부분이 잘린다)

void DisplayManager::showLargeIP(IPAddress ip) {
    for (int i = 0; i < 4; i++) {
        screens[i]->clearBuffer();
        int ip_idx = configManager.get().is_flipped ? (3 - i) : i;
        String segment = String(ip[ip_idx]);
        int charCount = segment.length();
        bool hasDot = (i < 3);
        int blockW = charCount * GLYPH_CELL_W + (hasDot ? IP_DOT_GAP + IP_DOT_SIZE : 0);
        int startX = (SCREEN_WIDTH - blockW) / 2;   // 가로 중앙 (도트 포함)
        int dotX   = startX + charCount * GLYPH_CELL_W + IP_DOT_GAP;
        // 도트 세로: 캐시 글리프는 실제 잉크 하단, 폴백 폰트는 baseline(TEXT_Y_POS)이
        // 숫자 잉크 하단이다. 래스터 하단에 맞추면 래스터가 클수록(64px) 점이 아래로
        // 내려가므로 쓰지 않는다.
        int dotY = TEXT_Y_POS - IP_DOT_SIZE + 1;                  // 폴백/측정 실패 기본값
        if (hasDot) {
            int inkBottom = renderer.inkBottomY(segment.substring(charCount - 1), 0);
            if (inkBottom >= 0) dotY = inkBottom - IP_DOT_SIZE + 1;
            if (dotY < 0) dotY = 0;
            if (dotY > SCREEN_HEIGHT - IP_DOT_SIZE) dotY = SCREEN_HEIGHT - IP_DOT_SIZE;
        }
        for (int j = 0; j < charCount; j++) {
            renderer.drawSingleChar(i, segment.substring(j, j + 1), startX + j * GLYPH_CELL_W, 0);
        }
        if (hasDot) screens[i]->drawBox(dotX, dotY, IP_DOT_SIZE, IP_DOT_SIZE);
        if (isTitleScreenOf(i)) {
            // 폰트를 6x10으로 통일했다. 글자 높이가 6px→10px로 커지므로 baseline을
            //   y=7에서 y=10으로 내린다 (y=7이면 윗부분이 화면 밖으로 잘린다).
            screens[i]->setFont(STATUS_FONT);
            screens[i]->drawStr(0, IP_TITLE_BASELINE_Y, "SETTING ADDR");
        }
    }
    pushParallel();
}

void DisplayManager::showButtonHelp() {
    const char* titles[4] = {"BTN 1", "BTN 2", "BTN 3", "BTN 4"};
    char chimeStr[20], animStr[32], modeStr[12], fmtStr[16], fontStr[20];
    snprintf(chimeStr, sizeof(chimeStr), "S:CHIME(%s)", configManager.get().chime_enabled ? "ON" : "OFF");
    snprintf(animStr, sizeof(animStr), "S:ANIMATION MODE %d", configManager.get().anim_mode);
    snprintf(modeStr, sizeof(modeStr), "S:MODE(%s)", configManager.get().display_mode == CLOCK_MODE_HANGUL ? "HAN" : "NUM");
    snprintf(fmtStr, sizeof(fmtStr), "L:12/24 (%s)", configManager.get().hour_format == HOUR_FORMAT_24H ? "24H" : "12H");
    const char* shorts[4] = {chimeStr, modeStr, animStr, "S:NEXT PAGE"};
    char flipStr[16];
    snprintf(flipStr, sizeof(flipStr), "L:FLIP (%s)", configManager.get().is_flipped ? "ON" : "OFF");
    snprintf(fontStr, sizeof(fontStr), "L:FONT CHANGE(%d)", configManager.get().font_slot);
    const char* longs[4]  = {flipStr,  fmtStr, fontStr,  "L:INVERT"};
    for (int i = 0; i < 4; i++) {
        screens[i]->clearBuffer();
        // 폰트를 6x10으로 통일했다. 14px→10px로 줄었으므로 3줄 baseline(y=15/35/55)은
        //   그대로 두어도 잘리지 않고 20px 간격 유지된다.
        screens[i]->setFont(STATUS_FONT);
        screens[i]->drawStr(0, 15, titles[i]);
        screens[i]->drawStr(0, 35, shorts[i]);
        screens[i]->drawStr(0, 55, longs[i]);
    }
    pushParallel();
}

void DisplayManager::showStatus(const String& msg) {
    U8G2* u8g2 = screens[0]; u8g2->clearBuffer(); u8g2->setFont(STATUS_FONT);
    u8g2->drawStr(0, 10, msg.c_str()); pushParallel();
}
