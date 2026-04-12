#include "display_manager.h"
#include "LittleFS.h"
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

DisplayManager display;

// 시보 표시용 종 모양 아이콘 (8x8)
static const uint8_t bell_icon[] = { 0x18, 0x3C, 0x3C, 0x3C, 0xFF, 0xDB, 0x18, 0x00 };

// 하위 레벨 I2C 콜백 및 설정은 i2c_platform.cpp로 이관됨

static void buzzerTimerCallback(TimerHandle_t xTimer) {
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
}

// DisplayManager 메서드 구현
DisplayManager::DisplayManager() : 
    u8g2_1(U8G2_R2, 255, 255, U8X8_PIN_NONE), u8g2_2(U8G2_R2, 255, 255, U8X8_PIN_NONE),
    u8g2_3(U8G2_R2, SW_SCL_PIN, SW_SDA_PIN, U8X8_PIN_NONE), u8g2_4(U8G2_R2, SW_SCL_PIN, SW_SDA_PIN, U8X8_PIN_NONE) {
    screens[0] = &u8g2_1; screens[1] = &u8g2_2; screens[2] = &u8g2_3; screens[3] = &u8g2_4;
}

void DisplayManager::begin() {
    applyFlip();
    main_task_handle = xTaskGetCurrentTaskHandle();
    for (int i = 0; i < NUM_SCREENS; i++) screens[i]->getU8g2()->tile_buf_ptr = u8g2_buffers[i];
    
    // 1. I2C 플랫폼 초기화
    i2cPlatform.begin();
    
    u8g2_1.getU8x8()->byte_cb = u8x8_byte_esp32_idf_0; u8g2_1.begin();
    u8g2_2.getU8x8()->byte_cb = u8x8_byte_esp32_idf_1; u8g2_2.setI2CAddress(I2C_ADDR_HW_1 * 2); u8g2_2.begin();
    u8g2_3.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast; u8g2_3.begin();
    u8g2_4.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast; u8g2_4.setI2CAddress(I2C_ADDR_HW_1 * 2); u8g2_4.begin();
    
    // 2. 설정 매니저 초기화 및 로드
    configManager.begin();
    
    // HW 태스크 생성은 i2cPlatform.begin()에서 처리됨
    
    for (int i = 0; i < 4; i++) {
        screens[i]->setFlipMode(configManager.get().is_flipped);
        screens[i]->setBitmapMode(1);
        screens[i]->clearBuffer();
        lastTexts[i] = "";
    }
    pushParallel();

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    buzzerTimer = xTimerCreate("BuzzerTimer", pdMS_TO_TICKS(50), pdFALSE, (void*)0, buzzerTimerCallback);
}

void DisplayManager::setFlipDisplay(bool flip) {
    configManager.get().is_flipped = flip;
    configManager.setDirty();
    for (int i = 0; i < 4; i++) {
        screens[i]->setFlipMode(flip);
    }
    setForceUpdate(true);
}

void DisplayManager::applyFlip() {
    screens[0] = &u8g2_1; screens[1] = &u8g2_2; screens[2] = &u8g2_3; screens[3] = &u8g2_4;
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

void DisplayManager::setFontName(String name) {
    configManager.get().font_name = name;
    configManager.setDirty();
    setForceUpdate(true);
}

// saveConfig()은 ConfigManager로 대체됨

void DisplayManager::loadBitmapCache() {
    addLog("Bitmap Loading");
    if (!LittleFS.exists("/")) {
        updateLastLog("Bitmap Loading: [FS Error]");
        return;
    }

    File root = LittleFS.open("/");
    if (!root) return;
    
    // 로딩 실패 시 기존 캐시 유지를 위해 임시 컨테이너 사용
    std::vector<CachedChar> tempCache;
    std::map<String, int> tempIndex;

    File file = root.openNextFile();
    while (file) {
        String name = file.name();
        if (name.startsWith("c_") && name.endsWith(".bin")) {
            size_t fileSize = file.size();
            if (fileSize > 0 && fileSize <= MAX_BITMAP_SIZE) {
                CachedChar cc;
                cc.hex = name.substring(2, name.length() - 4);
                try {
                    cc.data.resize(fileSize);
                    if (file.read(cc.data.data(), fileSize) == fileSize) {
                        tempIndex[cc.hex] = tempCache.size();
                        tempCache.push_back(std::move(cc));
                    }
                } catch (...) {
                    Serial.println("[ERROR] Memory allocation failure during cache load: " + name);
                }
            }
        }
        file.close();
        file = root.openNextFile();
        
        // 로딩 중 진행 표시 (점 애니메이션)
        static int dotTicker = 0;
        if (++dotTicker % 10 == 0) {
            String dots = "Bitmap Loading";
            for (int j = 0; j <= (dotTicker / 10) % 5; j++) dots += ".";
            updateLastLog(dots);
        }
    }

    // 성공적으로 읽어들인 경우에만 실제 캐시 교체 (원자적 교체 시뮬레이션)
    if (!tempCache.empty()) {
        bitmapCache = std::move(tempCache);
        cacheIndex = std::move(tempIndex);
        char logBuf[64];
        sprintf(logBuf, "Bitmap Loaded: %d", bitmapCache.size());
        updateLastLog(logBuf);
    } else {
        updateLastLog("Bitmap: Empty");
    }
}

String DisplayManager::getHexKey(const String& s) {
    String hexStr = "";
    for (int k = 0; k < s.length(); k++) {
        char buf[3]; sprintf(buf, "%02X", (unsigned char)s[k]); hexStr += buf;
    }
    return hexStr;
}

const CachedChar* DisplayManager::findChar(const String& s) {
    String key = getHexKey(s);
    if (cacheIndex.count(key)) return &bitmapCache[cacheIndex[key]];
    return nullptr;
}

void DisplayManager::clearAll() {
    for (int i = 0; i < NUM_SCREENS; i++) {
        screens[i]->clearBuffer();
        lastTexts[i] = "";
    }
    pushParallel(); // 비동기 전송으로 경합 방지
    setForceUpdate(true); // 즉시 다음 프레임 그리기 예약
}

void DisplayManager::beep(int duration, int freq) {
    if (buzzerTimer == NULL) return;
    tone(BUZZER_PIN, freq);
    xTimerChangePeriod(buzzerTimer, pdMS_TO_TICKS(duration), 0);
    xTimerStart(buzzerTimer, 0);
}

void DisplayManager::setYieldCallback(void (*cb)()) {
    on_yield_callback = cb;
}

void DisplayManager::drawDitheredChar(int idx, const String& charStr, int x, int density) {
    if (density <= 0) return;
    const CachedChar* cc_ptr = findChar(charStr);
    if (!cc_ptr) return;
    if (density >= 16) { drawSingleChar(idx, charStr, x, 0); return; }
    
    U8G2* u8g2 = screens[idx];
    int bw = (cc_ptr->data.size() <= 256) ? 4 : 8;
    int bx = (cc_ptr->data.size() <= 256) ? x : x - 16;
    for (int r = 0; r < 64; r++) {
        uint8_t row_mask = 0;
        for (int px = 0; px < 8; px++) {
            if (bayer_matrix[r % 4][(bx + px) % 4] < density) row_mask |= (1 << px);
        }
        for (int b = 0; b < bw; b++) {
            uint8_t original = cc_ptr->data[r * bw + b];
            uint8_t masked = original & row_mask;
            if (masked) u8g2->drawBitmap(bx + (b * 8), r, 1, 1, &masked);
        }
    }
}

void DisplayManager::drawZoomedChar(int idx, const String& charStr, int x, int scale_percent) {
    if (scale_percent <= 0) return;
    if (scale_percent == 100) { drawSingleChar(idx, charStr, x, 0); return; }
    
    const CachedChar* cc_ptr = findChar(charStr);
    if (!cc_ptr) return;

    U8G2* u8g2 = screens[idx];
    int bw = (cc_ptr->data.size() <= 256) ? 4 : 8;
    int orig_w = bw * 8;
    int target_w = (orig_w * scale_percent) / 100;
    int target_h = (64 * scale_percent) / 100;
    if (target_w <= 0 || target_h <= 0) return;

    int bx = (cc_ptr->data.size() <= 256) ? x : x - 16;
    int start_x = bx + (orig_w - target_w) / 2;
    int start_y = (64 - target_h) / 2;

    for (int r = 0; r < target_h; r++) {
        int src_y = (r * 64) / target_h;
        for (int c = 0; c < target_w; c++) {
            int src_x = (c * orig_w) / target_w;
            int byte_pos = (src_y * bw) + (src_x / 8);
            int bit_pos = src_x % 8;
            if (cc_ptr->data[byte_pos] & (0x80 >> bit_pos)) {
                u8g2->drawPixel(start_x + c, start_y + r);
            }
        }
    }
}

void DisplayManager::drawSingleChar(int idx, const String& charStr, int x, int y_offset) {
    const CachedChar* cc_ptr = findChar(charStr);
    if (cc_ptr) {
        U8G2* u8g2 = screens[idx];
        if (cc_ptr->data.size() <= 256) u8g2->drawBitmap(x, y_offset, 4, 64, cc_ptr->data.data());
        else u8g2->drawBitmap(x - 16, y_offset, 8, 64, cc_ptr->data.data());
    } else {
        U8G2* u8g2 = screens[idx];
        u8g2->setFont(HANGEUL_FONT);
        u8g2->drawUTF8(x + (32 - u8g2->getUTF8Width(charStr.c_str())) / 2, TEXT_Y_POS + y_offset, charStr.c_str());
    }
}

void DisplayManager::drawScaledChar(int idx, const String& charStr, int x, int h) {
    if (h <= 0) return;
    if (h == 64) { drawSingleChar(idx, charStr, x, 0); return; }
    
    const CachedChar* cc_ptr = findChar(charStr);
    U8G2* u8g2 = screens[idx];
    if (cc_ptr) {
        int bw = (cc_ptr->data.size() <= 256) ? 4 : 8;
        int bx = (cc_ptr->data.size() <= 256) ? x : x - 16;
        int start_y = (64 - h) / 2;
        for (int i = 0; i < h; i++) {
            int src_y = (i * 64) / h;
            u8g2->drawBitmap(bx, start_y + i, bw, 1, cc_ptr->data.data() + (src_y * bw));
        }
    } else {
        u8g2->setFont(HANGEUL_FONT);
        u8g2->drawUTF8(x + (32 - u8g2->getUTF8Width(charStr.c_str())) / 2, TEXT_Y_POS, charStr.c_str());
    }
}

void DisplayManager::getCharData(const String& text, CharData outChars[8], int& count, bool centered, int screenIdx) {
    count = 0;
    if (text == "") return;
    int i = 0;
    while (i < text.length() && count < 8) {
        int len = 1; unsigned char c = (unsigned char)text[i];
        if (c < 0x80) len = 1; else if ((c & 0xE0) == 0xC0) len = 2; else if ((c & 0xE0) == 0xE0) len = 3; else if ((c & 0xF8) == 0xF0) len = 4;
        outChars[count].c = text.substring(i, i + len);
        i += len; count++;
    }
    
    if (centered) {
        int totalW = count * CHAR_WIDTH;
        int startX = (SCREEN_WIDTH - totalW) / 2;
        for (int j = 0; j < count; j++) outChars[j].x = startX + (j * CHAR_WIDTH);
    } else {
        outChars[count - 1].x = 96;
        int numChars = count - 1;
        if (numChars > 0) {
            int startX = (96 - (numChars * CHAR_WIDTH)) / 2;
            for (int j = 0; j < numChars; j++) {
                outChars[j].x = startX + (j * CHAR_WIDTH);
            }
        }
    }
}

void DisplayManager::drawCenterText(int idx, const String& text, bool centered) {
    screens[idx]->clearBuffer();
    CharData chars[8]; int count;
    getCharData(text, chars, count, centered, idx);
    for (int j = 0; j < count; j++) {
        drawSingleChar(idx, chars[j].c, chars[j].x, 0);
    }
    if (centered && configManager.get().chime_enabled) {
        screens[idx]->drawXBM(0, 0, 8, 8, bell_icon);
    }
}

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
                bool isTitleScreen = configManager.get().is_flipped ? (i == 3) : (i == 0);
                drawCenterText(i, texts[i], isTitleScreen); 
                lastTexts[i] = texts[i]; 
            }
        }
        pushParallel();
        return;
    }

    CharData oldChars[4][8], newChars[4][8];
    int oldCount[4], newCount[4];
    for (int i = 0; i < 4; i++) {
        if (changed[i]) {
            bool isCentered = configManager.get().is_flipped ? (i == 3) : (i == 0);
            getCharData(lastTexts[i], oldChars[i], oldCount[i], isCentered, i);
            getCharData(texts[i], newChars[i], newCount[i], isCentered, i);
        }
    }

    for (int step = 0; step <= 16; step++) {
        for (int i = 0; i < 4; i++) {
            if (!changed[i]) continue;
            screens[i]->clearBuffer();
            
            for (int j = 0; j < newCount[i]; j++) {
                String nc = newChars[i][j].c; int nx = newChars[i][j].x;
                String oc = ""; bool isStatic = false;
                for (int k = 0; k < oldCount[i]; k++) {
                    if (oldChars[i][k].x == nx) {
                        oc = oldChars[i][k].c;
                        if (oc == nc) isStatic = true;
                        break;
                    }
                }

                if (isStatic) {
                    drawSingleChar(i, nc, nx, 0);
                } else {
                    switch (configManager.get().anim_mode) {
                        case ANIMATION_TYPE_SCROLL_UP:
                            if (step < 16 && oc != "") drawSingleChar(i, oc, nx, -(step * 4));
                            drawSingleChar(i, nc, nx, 64 - (step * 4));
                            break;
                        case ANIMATION_TYPE_SCROLL_DOWN:
                            if (step < 16 && oc != "") drawSingleChar(i, oc, nx, (step * 4));
                            drawSingleChar(i, nc, nx, -64 + (step * 4));
                            break;
                        case ANIMATION_TYPE_VERTICAL_FLIP:
                            if (step <= 8) { if (oc != "") drawScaledChar(i, oc, nx, ((8 - step) * 64) / 8); }
                            else { drawScaledChar(i, nc, nx, ((step - 8) * 64) / 8); }
                            break;
                        case ANIMATION_TYPE_DITHERED_FADE:
                            if (step <= 8) { if (oc != "") drawDitheredChar(i, oc, nx, 16 - (step * 2)); }
                            else { drawDitheredChar(i, nc, nx, (step - 8) * 2); }
                            break;
                        case ANIMATION_TYPE_ZOOM:
                            if (step <= 8) { if (oc != "") drawZoomedChar(i, oc, nx, ((8 - step) * 100) / 8); }
                            else {
                                int sc = (step <= 12) ? ((step - 8) * 150 / 4) : (150 - (step - 12) * 50 / 4);
                                drawZoomedChar(i, nc, nx, sc);
                            }
                            break;
                    }
                }
            }

            for (int k = 0; k < oldCount[i]; k++) {
                int ox = oldChars[i][k].x; String oc = oldChars[i][k].c;
                bool stillHasPos = false;
                for (int j = 0; j < newCount[i]; j++) { if (newChars[i][j].x == ox) { stillHasPos = true; break; } }
                if (stillHasPos) continue;

                switch (configManager.get().anim_mode) {
                    case ANIMATION_TYPE_SCROLL_UP:   if (step < 16) drawSingleChar(i, oc, ox, -(step * 4)); break;
                    case ANIMATION_TYPE_SCROLL_DOWN: if (step < 16) drawSingleChar(i, oc, ox, (step * 4)); break;
                    case ANIMATION_TYPE_VERTICAL_FLIP: if (step <= 8) drawScaledChar(i, oc, ox, ((8 - step) * 64) / 8); break;
                    case ANIMATION_TYPE_DITHERED_FADE: if (step <= 8) drawDitheredChar(i, oc, ox, 16 - (step * 2)); break;
                    case ANIMATION_TYPE_ZOOM:          if (step <= 8) drawZoomedChar(i, oc, ox, ((8 - step) * 100) / 8); break;
                }
            }

            bool isIconScreen = configManager.get().is_flipped ? (i == 3) : (i == 0);
            if (isIconScreen && configManager.get().chime_enabled) screens[i]->drawXBM(0, 0, 8, 8, bell_icon);
        }
        pushParallel();
        if (step < 16) {
            unsigned long startDelay = millis();
            while(millis() - startDelay < ANIMATION_STEP_DELAY_MS) {
                if (on_yield_callback) on_yield_callback();
                delay(1);
            }
        }
    }
    for (int i = 0; i < 4; i++) if (changed[i]) lastTexts[i] = texts[i];
}

void DisplayManager::pushParallel() {
    i2cPlatform.waitForSync(I2C_SYNC_TIMEOUT_MS);

    bool any_hw_dirty = false;
    for (int s = 0; s < 2; s++) {
        uint8_t* buf = screens[s]->getBufferPtr();
        for (int p = 0; p < PAGES_PER_SCREEN; p++) {
            bool page_dirty = false; int first_tile = -1, last_tile = -1;
            for (int t = 0; t < TILES_PER_PAGE; t++) {
                bool tile_dirty = false;
                for (int tx = 0; tx < 8; tx++) {
                    int idx = p * SCREEN_WIDTH + t * 8 + tx;
                    if (buf[idx] != i2cPlatform.getShadowData(s, idx)) { 
                        tile_dirty = true; 
                        i2cPlatform.setShadowData(s, idx, buf[idx]); 
                    }
                }
                if (tile_dirty) { if (first_tile == -1) first_tile = t; last_tile = t; page_dirty = true; }
            }
            if (page_dirty) { 
                i2cPlatform.preparePageUpdate(s, p, first_tile, last_tile - first_tile + 1);
                any_hw_dirty = true; 
            }
        }
    }
    if (any_hw_dirty) { i2cPlatform.notifyTransmission(); }
    
    for (int s = 2; s < 4; s++) {
        uint8_t* buf = screens[s]->getBufferPtr();
        for (int p = 0; p < PAGES_PER_SCREEN; p++) {
            int first_tile = -1, last_tile = -1; bool page_dirty = false;
            for (int t = 0; t < TILES_PER_PAGE; t++) {
                bool tile_dirty = false;
                for (int tx = 0; tx < 8; tx++) {
                    int idx = p * SCREEN_WIDTH + t * 8 + tx;
                    if (buf[idx] != i2cPlatform.getShadowData(s, idx)) { 
                        tile_dirty = true; 
                        i2cPlatform.setShadowData(s, idx, buf[idx]); 
                    }
                }
                if (tile_dirty) { if (first_tile == -1) first_tile = t; last_tile = t; page_dirty = true; }
            }
            if (page_dirty) screens[s]->updateDisplayArea(first_tile, p, last_tile - first_tile + 1, 1);
        }
    }
}

// i2c_hw_task 구현은 i2c_platform.cpp로 이관됨

void DisplayManager::recoverI2CBus() {
    Serial.println("[I2C] Recovering HW Bus and Screens...");
    // 하위 레벨 소프트 초기화 루틴 호출 (필요 시 i2cPlatform.recoverBus() 등)
    
    // OLED 기기 재설정 및 재시작 (주소 및 콜백 필수 재할당)
    u8g2_1.getU8x8()->byte_cb = u8x8_byte_esp32_idf_0; 
    u8g2_1.begin();

    u8g2_2.getU8x8()->byte_cb = u8x8_byte_esp32_idf_1; 
    u8g2_2.setI2CAddress(I2C_ADDR_HW_1 * 2); 
    u8g2_2.begin();

    u8g2_3.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast; 
    u8g2_3.begin();

    u8g2_4.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast; 
    u8g2_4.setI2CAddress(I2C_ADDR_HW_1 * 2); 
    u8g2_4.begin();
    
    // 강제 업데이트 예약
    setForceUpdate(true);
}

void DisplayManager::playStartupMelody() {
    int melody[] = {2093, 2637, 3136, 4186}; // Do-Mi-Sol-Do
    for (int i = 0; i < 4; i++) {
        tone(BUZZER_PIN, melody[i], 100);
        delay(120);
    }
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW); 
}

void DisplayManager::addLog(const String& msg) {
    if (log_count < 4) {
        log_lines[log_count++] = msg;
    } else {
        for (int i = 0; i < 3; i++) log_lines[i] = log_lines[i+1];
        log_lines[3] = msg;
    }
    
    U8G2* u8g2 = screens[0];
    u8g2->clearBuffer();
    u8g2->setFont(STATUS_FONT);
    for (int i = 0; i < log_count; i++) {
        u8g2->drawStr(0, 12 + (i * 14), log_lines[i].c_str());
    }
    pushParallel();
    Serial.println("[LOG] " + msg);
}

void DisplayManager::updateLastLog(const String& msg) {
    if (log_count > 0) {
        log_lines[log_count - 1] = msg;
        
        U8G2* u8g2 = screens[0];
        u8g2->clearBuffer();
        u8g2->setFont(STATUS_FONT);
        for (int i = 0; i < log_count; i++) {
            u8g2->drawStr(0, 12 + (i * 14), log_lines[i].c_str());
        }
        pushParallel();
    }
}

void DisplayManager::showLargeIP(IPAddress ip) {
    for (int i = 0; i < 4; i++) {
        screens[i]->clearBuffer();
        int ip_idx = configManager.get().is_flipped ? (3 - i) : i;
        String segment = String(ip[ip_idx]);
        int charCount = segment.length();
        int totalW = charCount * 32;
        int startX = (128 - totalW) / 2;
        for (int j = 0; j < charCount; j++) {
            drawSingleChar(i, segment.substring(j, j + 1), startX + (j * 32), 0);
        }
        if (i < 3) screens[i]->drawBox(120, 56, 4, 4);
        bool isTitleScreen = configManager.get().is_flipped ? (i == 3) : (i == 0);
        if (isTitleScreen) {
            screens[i]->setFont(u8g2_font_4x6_tf);
            screens[i]->drawStr(0, 7, "SETTING ADDR");
        }
    }
    pushParallel();
}

void DisplayManager::showButtonHelp() {
    const char* titles[4] = {"BTN 1", "BTN 2", "BTN 3", "BTN 4"};
    char chimeStr[20], animStr[32];
    sprintf(chimeStr, "S:CHIME(%s)", configManager.get().chime_enabled ? "ON" : "OFF");
    sprintf(animStr, "S:ANIMATION MODE %d", configManager.get().anim_mode);
    const char* shorts[4] = {chimeStr, "S:NUM <> HAN", animStr, "S:NEXT PAGE"};
    char flipStr[16];
    sprintf(flipStr, "L:FLIP (%s)", configManager.get().is_flipped ? "ON" : "OFF");
    const char* longs[4]  = {flipStr,  "L:12/24", "L:-",  "L:-"};
    for (int i = 0; i < 4; i++) {
        screens[i]->clearBuffer();
        screens[i]->setFont(u8g2_font_7x14_tf);
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

// 하위 레벨 I2C 전송 콜백은 i2c_platform.cpp로 이관됨
