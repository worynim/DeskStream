/**
 * @file renderer.cpp
 * @brief 고전 수치 및 한글 비트맵 렌더링 엔진 구현
 * @details LittleFS 비트맵 데이터 로딩, 캐싱 및 픽셀 스케일링/디더링 연산 로직 구현
 */
#include "renderer.h"
#include "LittleFS.h"
#include "display_manager.h"
#include "logger.h"

Renderer renderer;

Renderer::Renderer() {}

Renderer::~Renderer() {
    if (flatBuffer) free(flatBuffer);
}

void Renderer::setScreens(U8G2** screens) {
    _screens = screens;
}

void Renderer::clearCache() {
    bitmapCache.clear();
    cacheIndex.clear();
    if (flatBuffer) {
        free(flatBuffer);
        flatBuffer = nullptr;
    }
}

const uint8_t* Renderer::getCharDataPtr(const CachedChar* cc) const {
    if (!cc || !flatBuffer) return nullptr;
    return flatBuffer + cc->offset;
}

void Renderer::loadBitmapCache(int slot) {
    // 슬롯이 -1이면 설정에서 가져옴
    #include "config_manager.h"
    if (slot == -1) slot = configManager.get().font_slot;

    logger.addLog("Bitmap Pre-scanning (Slot " + String(slot) + ")");

    // --- 마이그레이션 로직: 루트의 파일을 /f0으로 이동 ---
    if (LittleFS.exists("/c_30.bin") && !LittleFS.exists("/f0/c_30.bin")) {
        logger.addLog("Migrating fonts to /f0...");
        LittleFS.mkdir("/f0");
        File r = LittleFS.open("/");
        File f = r.openNextFile();
        while (f) {
            String n = f.name();
            if (n.startsWith("c_") && n.endsWith(".bin")) {
                String oldP = "/" + n;
                String newP = "/f0/" + n;
                f.close(); // 닫아야 이동 가능할 수도 있음
                LittleFS.rename(oldP, newP);
                f = r.openNextFile();
            } else {
                f.close();
                f = r.openNextFile();
            }
        }
        logger.updateLastLog("Migration Done.");
    }

    String path = "/f" + String(slot);
    if (!LittleFS.exists(path)) {
        logger.updateLastLog("Slot " + String(slot) + ": Empty");
        clearCache();
        return;
    }

    File root = LittleFS.open(path);
    if (!root || !root.isDirectory()) {
        logger.updateLastLog("Slot " + String(slot) + ": Not a Dir");
        return;
    }
    
    std::vector<CachedChar> tempIndexList;
    size_t totalBufferSize = 0;

    // Stage 1: 스캔 및 총 크기 계산
    File file = root.openNextFile();
    if (DEBUG_MODE) Serial.println("[FS] --- Listing Files in " + path + " ---");
    while (file) {
        String name = file.name();
        size_t fileSize = file.size();
        
        if (name.startsWith("c_") && name.endsWith(".bin")) {
            if (fileSize > 0 && fileSize <= MAX_BITMAP_SIZE) {
                CachedChar cc;
                cc.hex = name.substring(2, name.length() - 4);
                cc.size = fileSize;
                cc.offset = totalBufferSize;
                totalBufferSize += fileSize;
                tempIndexList.push_back(cc);
                if (DEBUG_MODE) Serial.printf("[FS] Found: %s (%d bytes)\n", name.c_str(), (int)fileSize);
            }
        }
        file.close();
        file = root.openNextFile();
    }

    if (tempIndexList.empty()) {
        logger.updateLastLog("Slot " + String(slot) + ": No Bitmaps");
        clearCache();
        return;
    }

    // Stage 2: 단일 메모리 할당
    if (flatBuffer) free(flatBuffer);
    flatBuffer = (uint8_t*)malloc(totalBufferSize);
    if (!flatBuffer) {
        logger.updateLastLog("Bitmap: Malloc Fail");
        return;
    }

    // Stage 3: 데이터 로드
    logger.updateLastLog("Loading Space: " + String(totalBufferSize) + "B");
    cacheIndex.clear();
    bitmapCache.clear();
    
    for (size_t i = 0; i < tempIndexList.size(); i++) {
        String fileName = path + "/c_" + tempIndexList[i].hex + ".bin";
        File f = LittleFS.open(fileName, "r");
        if (f) {
            if (f.read(flatBuffer + tempIndexList[i].offset, tempIndexList[i].size) == tempIndexList[i].size) {
                cacheIndex[tempIndexList[i].hex] = bitmapCache.size();
                bitmapCache.push_back(tempIndexList[i]);
            }
            f.close();
        }
    }
    logger.updateLastLog("Font Cache Loaded (" + String(bitmapCache.size()) + ")");

    if (DEBUG_MODE) {
        // 메모리 사용량 리포트 출력
        Serial.println("\n[MEMORY] --- Memory Usage Report ---");
        Serial.printf("[MEMORY] Slot Path: %s\n", path.c_str());
        Serial.printf("[MEMORY] Flash (LittleFS) Total: %d bytes\n", (int)LittleFS.totalBytes());
        Serial.printf("[MEMORY] Flash (LittleFS) Used:  %d bytes\n", (int)LittleFS.usedBytes());
        Serial.printf("[MEMORY] RAM (Font Cache): %d bytes\n", (int)totalBufferSize);
        Serial.printf("[MEMORY] RAM (Free Heap):  %d bytes\n", (int)ESP.getFreeHeap());
        Serial.println("[MEMORY] ---------------------------\n");
    }
}


String Renderer::getHexKey(const String& s) {
    String hexStr = "";
    for (int k = 0; k < s.length(); k++) {
        char buf[3]; sprintf(buf, "%02X", (unsigned char)s[k]); hexStr += buf;
    }
    return hexStr;
}

const CachedChar* Renderer::findChar(const String& s) {
    String key = getHexKey(s);
    if (cacheIndex.count(key)) return &bitmapCache[cacheIndex[key]];
    return nullptr;
}

void Renderer::drawSingleChar(int screenIdx, const String& charStr, int x, int y_offset) {
    if (!_screens || screenIdx >= NUM_SCREENS) return;
    const CachedChar* cc_ptr = findChar(charStr);
    U8G2* u8g2 = _screens[screenIdx];
    
    if (cc_ptr) {
        const uint8_t* data = getCharDataPtr(cc_ptr);
        if (cc_ptr->size <= 256) u8g2->drawBitmap(x, y_offset, 4, 64, data);
        else u8g2->drawBitmap(x - 16, y_offset, 8, 64, data);
    } else {
        u8g2->setFont(HANGEUL_FONT);
        u8g2->drawUTF8(x + (32 - u8g2->getUTF8Width(charStr.c_str())) / 2, TEXT_Y_POS + y_offset, charStr.c_str());
    }
}

void Renderer::drawDitheredChar(int screenIdx, const String& charStr, int x, int density) {
    if (!_screens || screenIdx >= NUM_SCREENS || density <= 0) return;
    const CachedChar* cc_ptr = findChar(charStr);
    if (!cc_ptr) { drawSingleChar(screenIdx, charStr, x, 0); return; }
    if (density >= 16) { drawSingleChar(screenIdx, charStr, x, 0); return; }
    
    const uint8_t* data = getCharDataPtr(cc_ptr);
    U8G2* u8g2 = _screens[screenIdx];
    int bw = (cc_ptr->size <= 256) ? 4 : 8;
    int bx = (cc_ptr->size <= 256) ? x : x - 16;
    for (int r = 0; r < 64; r++) {
        uint8_t row_mask = 0;
        for (int px = 0; px < 8; px++) {
            if (bayer_matrix[r % 4][(bx + px) % 4] < density) row_mask |= (1 << px);
        }
        for (int b = 0; b < bw; b++) {
            uint8_t original = data[r * bw + b];
            uint8_t masked = original & row_mask;
            if (masked) u8g2->drawBitmap(bx + (b * 8), r, 1, 1, &masked);
        }
    }
}

void Renderer::drawZoomedChar(int screenIdx, const String& charStr, int x, int scale_percent) {
    if (!_screens || screenIdx >= NUM_SCREENS || scale_percent <= 0) return;
    if (scale_percent == 100) { drawSingleChar(screenIdx, charStr, x, 0); return; }
    
    const CachedChar* cc_ptr = findChar(charStr);
    if (!cc_ptr) { drawSingleChar(screenIdx, charStr, x, 0); return; }

    const uint8_t* data = getCharDataPtr(cc_ptr);
    U8G2* u8g2 = _screens[screenIdx];
    int bw = (cc_ptr->size <= 256) ? 4 : 8;
    int orig_w = bw * 8;
    int target_w = (orig_w * scale_percent) / 100;
    int target_h = (64 * scale_percent) / 100;
    if (target_w <= 0 || target_h <= 0) return;

    int bx = (cc_ptr->size <= 256) ? x : x - 16;
    int start_x = bx + (orig_w - target_w) / 2;
    int start_y = (64 - target_h) / 2;

    for (int r = 0; r < target_h; r++) {
        int src_y = (r * 64) / target_h;
        for (int c = 0; c < target_w; c++) {
            int src_x = (c * orig_w) / target_w;
            int byte_pos = (src_y * bw) + (src_x / 8);
            int bit_pos = src_x % 8;
            if (data[byte_pos] & (0x80 >> bit_pos)) {
                u8g2->drawPixel(start_x + c, start_y + r);
            }
        }
    }
}

void Renderer::drawScaledChar(int screenIdx, const String& charStr, int x, int h) {
    if (!_screens || screenIdx >= NUM_SCREENS || h <= 0) return;
    if (h == 64) { drawSingleChar(screenIdx, charStr, x, 0); return; }
    
    const CachedChar* cc_ptr = findChar(charStr);
    U8G2* u8g2 = _screens[screenIdx];
    if (cc_ptr) {
        const uint8_t* data = getCharDataPtr(cc_ptr);
        int bw = (cc_ptr->size <= 256) ? 4 : 8;
        int bx = (cc_ptr->size <= 256) ? x : x - 16;
        int start_y = (64 - h) / 2;
        for (int i = 0; i < h; i++) {
            int src_y = (i * 64) / h;
            u8g2->drawBitmap(bx, start_y + i, bw, 1, data + (src_y * bw));
        }
    } else {
        u8g2->setFont(HANGEUL_FONT);
        u8g2->drawUTF8(x + (32 - u8g2->getUTF8Width(charStr.c_str())) / 2, TEXT_Y_POS, charStr.c_str());
    }
}

void Renderer::getCharData(const String& text, CharData outChars[8], int& count, bool centered) {
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
        int startX = (128 - count * 32) / 2;
        for (int j = 0; j < count; j++) outChars[j].x = startX + j * 32;
    } else {
        int startX = (96 - (count - 1) * 32) / 2;
        for (int j = 0; j < count; j++) {
            if (j == count - 1) outChars[j].x = 96;
            else outChars[j].x = startX + j * 32;
        }
    }
}
