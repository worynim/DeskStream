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

void Renderer::setScreens(U8G2** screens) {
    _screens = screens;
}

void Renderer::clearCache() {
    bitmapCache.clear();
    cacheIndex.clear();
}

void Renderer::loadBitmapCache() {
    logger.addLog("Bitmap Loading");
    if (!LittleFS.exists("/")) {
        logger.updateLastLog("Bitmap Loading: [FS Error]");
        return;
    }

    File root = LittleFS.open("/");
    if (!root) return;
    
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
                    Serial.println("[ERROR] Cache load fail: " + name);
                }
            }
        }
        file.close();
        file = root.openNextFile();
        
        static int dotTicker = 0;
        if (++dotTicker % 10 == 0) {
            String dots = "Bitmap Loading";
            for (int j = 0; j <= (dotTicker / 10) % 5; j++) dots += ".";
            logger.updateLastLog(dots);
        }
    }

    if (!tempCache.empty()) {
        bitmapCache = std::move(tempCache);
        cacheIndex = std::move(tempIndex);
        char logBuf[64];
        sprintf(logBuf, "Bitmap Loaded: %d", (int)bitmapCache.size());
        logger.updateLastLog(logBuf);
    } else {
        logger.updateLastLog("Bitmap: Empty");
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
        if (cc_ptr->data.size() <= 256) u8g2->drawBitmap(x, y_offset, 4, 64, cc_ptr->data.data());
        else u8g2->drawBitmap(x - 16, y_offset, 8, 64, cc_ptr->data.data());
    } else {
        u8g2->setFont(HANGEUL_FONT);
        u8g2->drawUTF8(x + (32 - u8g2->getUTF8Width(charStr.c_str())) / 2, TEXT_Y_POS + y_offset, charStr.c_str());
    }
}

void Renderer::drawDitheredChar(int screenIdx, const String& charStr, int x, int density) {
    if (!_screens || screenIdx >= NUM_SCREENS || density <= 0) return;
    const CachedChar* cc_ptr = findChar(charStr);
    if (!cc_ptr) return;
    if (density >= 16) { drawSingleChar(screenIdx, charStr, x, 0); return; }
    
    U8G2* u8g2 = _screens[screenIdx];
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

void Renderer::drawZoomedChar(int screenIdx, const String& charStr, int x, int scale_percent) {
    if (!_screens || screenIdx >= NUM_SCREENS || scale_percent <= 0) return;
    if (scale_percent == 100) { drawSingleChar(screenIdx, charStr, x, 0); return; }
    
    const CachedChar* cc_ptr = findChar(charStr);
    if (!cc_ptr) return;

    U8G2* u8g2 = _screens[screenIdx];
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

void Renderer::drawScaledChar(int screenIdx, const String& charStr, int x, int h) {
    if (!_screens || screenIdx >= NUM_SCREENS || h <= 0) return;
    if (h == 64) { drawSingleChar(screenIdx, charStr, x, 0); return; }
    
    const CachedChar* cc_ptr = findChar(charStr);
    U8G2* u8g2 = _screens[screenIdx];
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
