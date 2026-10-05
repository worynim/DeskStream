// worynim@gmail.com
/**
 * @file renderer.cpp
 * @brief 커스텀 비트맵 렌더링 엔진 구현
 * @details LittleFS 비트맵 로딩·캐싱 및 디더·줌·플립 픽셀 연산
 * @note [SYNC] 원본: ENG_Clock/renderer.cpp — 1줄 고정 피치 구조로 재작성.
 */
#include "renderer.h"
#include "LittleFS.h"
#include "config_manager.h"
#include "layout_engine.h"   // LAYOUT_SCREEN_HEIGHT / LAYOUT_LINE_HEIGHT (static_assert용)
#include "logger.h"

Renderer renderer;

// layout_engine.h는 Arduino에 의존하지 않아 화면 크기를 자기 상수로 갖는다.
// 값이 어긋나면 1줄 세로 위치(LINE_TOP_Y)가 화면 밖으로 나가므로 컴파일 타점에 막는다.
static_assert(SCREEN_HEIGHT == LAYOUT_SCREEN_HEIGHT, "SCREEN_HEIGHT와 LAYOUT_SCREEN_HEIGHT 불일치");
static_assert(LINE_HEIGHT == LAYOUT_LINE_HEIGHT, "LINE_HEIGHT와 LAYOUT_LINE_HEIGHT 불일치");

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

String Renderer::getHexKey(const String& s) const {
    String hexStr = "";
    for (int k = 0; k < s.length(); k++) {
        char buf[3]; sprintf(buf, "%02X", (unsigned char)s[k]); hexStr += buf;
    }
    return hexStr;
}

const CachedChar* Renderer::findChar(const String& s) const {
    if (!flatBuffer || bitmapCache.empty()) return nullptr;
    const String key = getHexKey(s);
    std::map<String, int>::const_iterator it = cacheIndex.find(key);
    return (it == cacheIndex.end()) ? nullptr : &bitmapCache[it->second];
}

bool Renderer::hasGlyph(const String& s) const {
    return findChar(s) != nullptr;
}

const CellGeometry* Renderer::geometryOf(const String& s) {
    const CachedChar* cc = findChar(s);
    return (cc && cc->geom) ? cc->geom : nullptr;
}

/**
 * @brief Stage 1 — 슬롯 디렉터리를 훑어 글리프 목록과 총 바이트 수를 만든다
 * @param path  슬롯 디렉터리 ("/fN")
 * @param out   수집된 글리프 목록 (offset이 이 함수에서 채워진다)
 * @param totalSize out의 총 바이트 수
 * @return 디렉터리를 열 수 있었으면 true
 * @details 파일명 규칙(c_XX.bin), 크기 범위, **기하를 확정할 수 있는 크기**를 모두 거른다.
 *          파일은 열기만 하고 내용은 읽지 않는다 — 총 크기를 알아야 한 번에 malloc할 수 있다.
 */
static bool scanSlotFonts(const String& path, std::vector<CachedChar>& out, size_t& totalSize) {
    File root = LittleFS.open(path);
    if (!root || !root.isDirectory()) return false;

    out.clear();
    totalSize = 0;

    File file = root.openNextFile();
    while (file) {
        const String name = file.name();
        const size_t fileSize = file.size();

        if (name.startsWith("c_") && name.endsWith(".bin") && fileSize > 0
            && fileSize <= MAX_BITMAP_SIZE) {
            // [핵심] 알 수 없는 크기의 폰트는 기하를 확정할 수 없으므로 건너뛴다.
            //   중국어판은 288B 하나만 수용한다 — ENG판의 5종 판별 분기가 전부 제거됐다.
            const CellGeometry* g = geometryForSize((uint32_t)fileSize);
            if (g) {
                CachedChar cc;
                cc.hex = name.substring(2, name.length() - 4);
                cc.size = (uint32_t)fileSize;
                cc.geom = g;
                cc.offset = (uint32_t)totalSize;
                totalSize += fileSize;
                out.push_back(cc);
                if (DEBUG_MODE) Serial.printf("[FS] %s (%d bytes, %dx%d)\n",
                                             name.c_str(), (int)fileSize, g->glyphW, g->glyphH);
            } else if (DEBUG_MODE) {
                Serial.printf("[FS] Skipped (unknown size): %s (%d bytes)\n",
                              name.c_str(), (int)fileSize);
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
    return true;
}

/**
 * @brief Stage 3 — 목록에 적힌 순서대로 파일을 읽어 하나의 버퍼에 이어 붙인다
 * @note 목록에 없는 파일(Stage 1과 Stage 3 사이에 사라진 것)은 조용히 건너뛴다.
 *       일부만 들어온 캐시는 findChar()가 nullptr을 돌려줄 뿐 깨지지 않는다.
 */
static void loadSlotFonts(const String& path, const std::vector<CachedChar>& list,
                          uint8_t* buffer, std::map<String, int>& index,
                          std::vector<CachedChar>& loaded) {
    index.clear();
    loaded.clear();
    for (size_t i = 0; i < list.size(); i++) {
        File f = LittleFS.open(path + "/c_" + list[i].hex + ".bin", "r");
        if (!f) continue;
        if (f.read(buffer + list[i].offset, list[i].size) == list[i].size) {
            index[list[i].hex] = (int)loaded.size();
            loaded.push_back(list[i]);
        }
        f.close();
    }
}

void Renderer::loadBitmapCache(int slot) {
    if (slot == -1) slot = configManager.get().font_slot;

    logger.addLog("Font Scan (Slot " + String(slot) + ")");

    const String path = "/f" + String(slot);
    if (!LittleFS.exists(path)) {
        logger.updateLastLog("Slot " + String(slot) + ": Empty");
        clearCache();
        return;
    }

    std::vector<CachedChar> scanned;
    size_t totalBufferSize = 0;
    if (!scanSlotFonts(path, scanned, totalBufferSize)) {
        logger.updateLastLog("Slot " + String(slot) + ": Not a Dir");
        return;
    }

    if (scanned.empty()) {
        logger.updateLastLog("Slot " + String(slot) + ": No Font");
        clearCache();
        return;
    }

    // Stage 2: 단일 메모리 할당 (한 번만malloc해야 포인터 이동이 없다)
    if (flatBuffer) free(flatBuffer);
    flatBuffer = (uint8_t*)malloc(totalBufferSize);
    if (!flatBuffer) {
        logger.updateLastLog("Font: Malloc Fail");
        return;
    }

    logger.updateLastLog("Loading " + String(totalBufferSize) + "B");
    loadSlotFonts(path, scanned, flatBuffer, cacheIndex, bitmapCache);
    logger.updateLastLog("Font Loaded (" + String(bitmapCache.size()) + ")");

    if (DEBUG_MODE) {
        Serial.printf("[MEMORY] Font Cache (RAM): %d bytes / Free Heap: %d bytes\n",
                      (int)totalBufferSize, (int)ESP.getFreeHeap());
    }
}

/**
 * @brief 캐시에 없는 글자는 U8g2 내장 라틴 폰트로 그린다
 * @details [PLAN §6.7 대안 A] 중국어판은 CJK 폴백 폰트를 도입하지 않는다.
 *          한자를 라틴 폰트로 그리면 빈 폭이 나오지만, 이 경로는 "커스텀 폰트를
 *          아직 올리지 않은 초기 상태"와 "숫자만 그리는 IP 화면"에서만 발생한다.
 *          커스텀 폰트는 이미 필수다(btn2_short()가 "Font Required!"를 표시).
 */
static void drawFallbackChar(U8G2* u8g2, const String& charStr, int x, int y) {
    const CellGeometry& g = defaultGeometry();
    u8g2->setFont(CHINESE_FONT);
    u8g2->drawUTF8(x + (g.glyphW - u8g2->getUTF8Width(charStr.c_str())) / 2,
                   y + FALLBACK_BASELINE, charStr.c_str());
}

void Renderer::drawSingleChar(int screenIdx, const String& charStr, int x, int y) {
    if (!_screens || screenIdx >= NUM_SCREENS) return;
    const CachedChar* cc = findChar(charStr);
    if (!cc || !cc->geom) { drawFallbackChar(_screens[screenIdx], charStr, x, y); return; }

    // 래스터는 48px(피치 32 + 좌우 여유 8)이고 xOffset −8로 32px 피치 안에
    // 중앙 정렬된다 → drawW만큼 그려도 이웃 칸을 침범하지 않는다 (PLAN §5.4).
    const CellGeometry& g = *cc->geom;
    _screens[screenIdx]->drawBitmap(x + g.xOffset, y, g.bytesPerRow, g.glyphH,
                                   getCharDataPtr(cc));
}

void Renderer::drawDitheredChar(int screenIdx, const String& charStr, int x, int y, int density) {
    if (!_screens || screenIdx >= NUM_SCREENS) return;
    if (density <= 0) return;                 // 완전 투명 — 아무것도 안 그린다
    if (density >= 16) { drawSingleChar(screenIdx, charStr, x, y); return; }

    const CachedChar* cc = findChar(charStr);
    if (!cc || !cc->geom) { drawSingleChar(screenIdx, charStr, x, y); return; }

    const uint8_t* data = getCharDataPtr(cc);
    const CellGeometry& g = *cc->geom;
    U8G2* u8g2 = _screens[screenIdx];
    const int bx = x + g.xOffset;             // 32px 피치라 음수가 되지 않는다

    // Bayer는 4×4 주기 패턴이므로 0..3으로 보정해도 위상이 그대로다.
    //   bx가 음수면 C++ 나머지가 음수라 그대로 인덱싱하면 배열을 벗어난다.
    const int phaseX = ((bx % 4) + 4) % 4;
    for (int r = 0; r < g.glyphH; r++) {
        const int phaseY = ((r % 4) + 4) % 4;
        uint8_t row_mask = 0;
        for (int px = 0; px < 8; px++) {
            if (bayer_matrix[phaseY][(phaseX + px) % 4] < density) row_mask |= (1 << px);
        }
        // 한 행의 마스크를 모든 바이트에 재사용할 수 있다:
        //   바이트 b의 실제 열 위상 = (bx + b*8 + px) % 4 = (bx + px) % 4 (8은 4의 배수)
        for (int b = 0; b < g.bytesPerRow; b++) {
            const uint8_t masked = data[r * g.bytesPerRow + b] & row_mask;
            if (masked) u8g2->drawBitmap(bx + (b * 8), y + r, 1, 1, &masked);
        }
    }
}

void Renderer::drawZoomedChar(int screenIdx, const String& charStr, int x, int y, int scale_percent) {
    if (!_screens || screenIdx >= NUM_SCREENS) return;
    if (scale_percent <= 0) return;
    if (scale_percent == 100) { drawSingleChar(screenIdx, charStr, x, y); return; }

    const CachedChar* cc = findChar(charStr);
    if (!cc || !cc->geom) { drawSingleChar(screenIdx, charStr, x, y); return; }

    const uint8_t* data = getCharDataPtr(cc);
    const CellGeometry& g = *cc->geom;
    U8G2* u8g2 = _screens[screenIdx];

    const int origW = g.drawW;
    const int targetW = (origW * scale_percent) / 100;
    const int targetH = ((int)g.glyphH * scale_percent) / 100;
    if (targetW <= 0 || targetH <= 0) return;

    // [놀람의 상수 32 아님] 줌의 정지 위치는 **래스터 중앙 = 줄 중앙**이다.
    //   startY + targetH/2 = y + LINE_HEIGHT/2
    const int startX = (x + g.xOffset) + (origW - targetW) / 2;
    const int startY = y + (LINE_HEIGHT - targetH) / 2;

    for (int r = 0; r < targetH; r++) {
        const int srcY = (r * g.glyphH) / targetH;
        for (int c = 0; c < targetW; c++) {
            const int srcX = (c * origW) / targetW;
            // MSB 우선 — packGlyph 미러 규약과 같다 (renderer_geometry.h 참조)
            if (data[srcY * g.bytesPerRow + (srcX / 8)] & (0x80 >> (srcX % 8))) {
                u8g2->drawPixel(startX + c, startY + r);
            }
        }
    }
}

void Renderer::drawScaledChar(int screenIdx, const String& charStr, int x, int y, int h) {
    if (!_screens || screenIdx >= NUM_SCREENS || h <= 0) return;
    const CachedChar* cc = findChar(charStr);
    if (!cc || !cc->geom) { drawFallbackChar(_screens[screenIdx], charStr, x, y); return; }
    if (h >= LINE_HEIGHT) { drawSingleChar(screenIdx, charStr, x, y); return; }

    // 세로 창: 래스터 48px 중 LINE_HEIGHT(48) 높이의 구간.
    //   중국어판은 래스터 높이와 줄 높이가 같아 창이 래스터 전체다.
    //   ENG판의 (glyphH − LINE_HEIGHT)/2 오프셋은 여기서 항상 0이다.
    const uint8_t* data = getCharDataPtr(cc);
    const CellGeometry& g = *cc->geom;
    U8G2* u8g2 = _screens[screenIdx];
    const int startY = y + (LINE_HEIGHT - h) / 2;
    for (int i = 0; i < h; i++) {
        const int srcY = (i * LINE_HEIGHT) / h;
        u8g2->drawBitmap(x + g.xOffset, startY + i, g.bytesPerRow, 1,
                         data + (srcY * g.bytesPerRow));
    }
}