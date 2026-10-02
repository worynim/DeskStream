// worynim@gmail.com
/**
 * @file renderer.cpp
 * @brief 고전 수치 및 커스텀 비트맵 렌더링 엔진 구현
 * @details LittleFS 비트맵 데이터 로딩, 캐싱 및 픽셀 스케일링/디더링 연산 로직 구현
 * @note [SYNC] 원본: Hangeul_Clock/renderer.cpp — 하드코딩된 기하를 테이블로 교체
 */
#include "renderer.h"
#include "LittleFS.h"
#include "config_manager.h"
#include "display_manager.h"
#include "layout_engine.h"
#include "logger.h"

Renderer renderer;

// layout_engine.h는 Arduino에 의존하지 않아 화면 크기를 자기 상수로 갖는다.
// 값이 어긋나면 1줄 세로 중앙(y)이 화면 밖으로 나가므로 컴파일 타점에 막는다.
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
    maxInkWidth = 0;   // PLAN §6.16 — 캐시가 없으므로 잉크도 모른다(간격 확장 없음)
    inkByChar.clear(); // §6.16b — 글자별 잉크 표도 같은 이유로 함께 비운다
    if (flatBuffer) {
        free(flatBuffer);
        flatBuffer = nullptr;
    }
}

void Renderer::measureMaxInkWidth() {
    maxInkWidth = 0;
    inkByChar.clear();
    for (size_t i = 0; i < bitmapCache.size(); i++) {
        const CachedChar& cc = bitmapCache[i];
        if (!cc.geom) continue;
        uint8_t w = inkWidthOf(getCharDataPtr(&cc), cc.geom->bytesPerRow, cc.geom->glyphH);
        if (w > maxInkWidth) maxInkWidth = w;
        inkByChar[cc.hex] = w;   // §6.16b — 줄마다 다른 상한을 쓰려면 글자별 값이 필요하다
    }
}

uint8_t Renderer::inkOf(const char* s, uint8_t len) const {
    if (!s || len == 0) return maxInkWidth;
    String hex = getHexKey(String(s, len));
    std::map<String, uint8_t>::const_iterator it = inkByChar.find(hex);
    return (it == inkByChar.end()) ? maxInkWidth : it->second;
}

/**
 * [§6.16b] InkWidthFn은 extern "C"가 아니라 **비-static 멤버 함수**를 넘길 수 없다.
 * layoutWrap이 Cache를 몰라도 되도록 하는 유일한 통로여서, 여기서만 시그니처를 맞춘다.
 */
uint8_t Renderer::inkOfTrampoline(void* ctx, const char* text, uint8_t len) {
    Renderer* self = static_cast<Renderer*>(ctx);
    return self ? self->inkOf(text, len) : 0;
}

const uint8_t* Renderer::getCharDataPtr(const CachedChar* cc) const {
    if (!cc || !flatBuffer) return nullptr;
    return flatBuffer + cc->offset;
}

void Renderer::loadBitmapCache(int slot) {
    // [§6.16] 잉크 폭은 이번 로드의 결과다. 중간에 return하는 경로(디렉터리 아님,
    // malloc 실패)가 clearCache()를 거치지 않아 이전 값이 남을 수 있으므로
    // **어떤 조기 반환보다 앞에서** 0으로 되돌린다.
    maxInkWidth = 0;
    inkByChar.clear();   // §6.16b — 글자별 표도 같은 이유로 함께 비운다

    // 슬롯이 -1이면 설정에서 가져옴
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
                // 알 수 없는 크기의 폰트는 기하를 확정할 수 없으므로 건너뛴다.
                const CellGeometry* g = geometryForSize((uint32_t)fileSize);
                if (!g) {
                    if (DEBUG_MODE) Serial.printf("[FS] Skipped (unknown size): %s (%d bytes)\n",
                                                 name.c_str(), (int)fileSize);
                } else {
                    CachedChar cc;
                    cc.hex = name.substring(2, name.length() - 4);
                    cc.size = fileSize;
                    cc.geom = g;
                    cc.offset = totalBufferSize;
                    totalBufferSize += fileSize;
                    tempIndexList.push_back(cc);
                    if (DEBUG_MODE) Serial.printf("[FS] Found: %s (%d bytes, %dx%d)\n",
                                                 name.c_str(), (int)fileSize, g->glyphW, g->glyphH);
                }
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

    // [PLAN §6.16] 레이아웃 간격 확장에 쓸 폰트 최대 잉크 폭을 잰다.
    // 래스터 폭(48)으로는 폰트 크기를 알 수 없으므로 실제 픽셀을 훑어야 한다.
    // 38자 × 384B = 14.6KB 스캔이라 업로드 1회에 1회뿐이다.
    measureMaxInkWidth();

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


String Renderer::getHexKey(const String& s) const {
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

const CellGeometry* Renderer::geometryOf(const String& s) {
    const CachedChar* cc = findChar(s);
    return (cc && cc->geom) ? cc->geom : nullptr;
}

/** 캐시가 없으면 기본 기하로 폴백 폰트를 그린다. y는 화면 좌표(0, 16, 32). */
static void drawFallbackChar(U8G2* u8g2, const String& charStr, int x, int y_offset) {
    const CellGeometry& g = defaultGeometry();
    u8g2->setFont(ENGLISH_FONT);
    int cellW = g.glyphW;
    u8g2->drawUTF8(x + (cellW - u8g2->getUTF8Width(charStr.c_str())) / 2,
                   FALLBACK_BASELINE + y_offset, charStr.c_str());
}

/**
 * [여백 수정 v5 — PLAN §6.14] 래스터를 줄 밴드 중앙에 놓는 그리기 상단 y.
 * y_offset은 layoutWrap()이 준 밴드 위쪽 y(0/16/32)다.
 *   glyphH=32(구 형식 64B/192B) → yTop = y_offset      (기존 렌더링 불변)
 *   glyphH=64(신형 384B)        → yTop = y_offset - 16  (밴드 중앙 = 화면 중앙)
 * 잉크는 래스터 안에서 세로 중앙 정렬돼 있으므로(업로드 시 합집합 중앙),
 * 래스터 중앙이 밴드 중앙에 오면 잉크도 밴드 중앙에 온다.
 */
static inline int rasterTopY(int y_offset, const CellGeometry& g) {
    return y_offset + (LINE_HEIGHT - (int)g.glyphH) / 2;
}

void Renderer::drawSingleChar(int screenIdx, const String& charStr, int x, int y_offset) {
    if (!_screens || screenIdx >= NUM_SCREENS) return;
    const CachedChar* cc_ptr = findChar(charStr);
    U8G2* u8g2 = _screens[screenIdx];

    if (cc_ptr && cc_ptr->geom) {
        const uint8_t* data = getCharDataPtr(cc_ptr);
        const CellGeometry& g = *cc_ptr->geom;
        u8g2->drawBitmap(x + g.xOffset, rasterTopY(y_offset, g), g.bytesPerRow, g.glyphH, data);
    } else {
        drawFallbackChar(u8g2, charStr, x, y_offset);
    }
}

void Renderer::drawSingleCharClipped(int screenIdx, const String& charStr, int x, int y_offset,
                                      int bandTop, int bandH) {
    if (!_screens || screenIdx >= NUM_SCREENS) return;
    const CachedChar* cc_ptr = findChar(charStr);
    const CellGeometry& g = (cc_ptr && cc_ptr->geom) ? *cc_ptr->geom : defaultGeometry();

    // 래스터 영역 [rasterTop, rasterTop+glyphH) 와 밴드 [bandTop, bandTop+bandH) 의 교집합
    const int rasterTop = rasterTopY(y_offset, g);
    int top = rasterTop, bottom = rasterTop + g.glyphH;
    if (top < bandTop) top = bandTop;
    if (bottom > bandTop + bandH) bottom = bandTop + bandH;
    if (bottom <= top) return;                       // 완전히 밴드 밖 → 그리지 않는다

    U8G2* u8g2 = _screens[screenIdx];
    if (cc_ptr && cc_ptr->geom) {
        // 캐시 글리프: 행 단위로 잘라서 그린다 (u8g2_drawBitmap 은 시작 y와 행 수를 받는다)
        const uint8_t* data = getCharDataPtr(cc_ptr);
        const int skip = top - rasterTop;
        u8g2->drawBitmap(x + g.xOffset, top, g.bytesPerRow, bottom - top,
                         data + (skip * g.bytesPerRow));
        return;
    }
    // 폴백 폰트: drawUTF8 는 행 단위 제어가 안 된다. 밴드와 조금이라도 겹칠 때만 그린다.
    // (캐시가 없는 초기 상태에서만 발생하는 경로이며, 겹치는 부분만 드는 것은 감수한다)
    drawFallbackChar(u8g2, charStr, x, y_offset);
}

void Renderer::drawDitheredChar(int screenIdx, const String& charStr, int x, int density,
                                int y_offset, int bandTop, int bandH) {
    if (!_screens || screenIdx >= NUM_SCREENS || density <= 0) return;
    const CachedChar* cc_ptr = findChar(charStr);
    if (!cc_ptr || !cc_ptr->geom) { drawSingleChar(screenIdx, charStr, x, y_offset); return; }
    if (density >= 16) { drawSingleChar(screenIdx, charStr, x, y_offset); return; }

    const uint8_t* data = getCharDataPtr(cc_ptr);
    const CellGeometry& g = *cc_ptr->geom;
    U8G2* u8g2 = _screens[screenIdx];
    int bw = g.bytesPerRow;
    int bx = x + g.xOffset;
    const int yTop = rasterTopY(y_offset, g);
    // [v4] GEOM_192B/384B의 xOffset(-17) 때문에 bx가 음수가 될 수 있다(좌측 시작 x=1).
    //   C++ 나머지는 피제수 부호를 따르므로 음수 인덱스로 bayer_matrix를 벗어난다.
    //   Bayer는 4×4 주기 패턴이므로 0..3으로 보정해도 위상은 동일하다.
    int phaseX = ((bx % 4) + 4) % 4;
    // [v5] 래스터가 밴드보다 높다(48×64). 그릴 행을 밴드와의 교집합으로 제한한다 —
    //   2줄에서 이웃 줄 밴드로 잉크가 새는 것을 막는다 (기본 밴드 = 화면 전체).
    int rFirst = 0, rLast = g.glyphH;
    if (rFirst < bandTop - yTop) rFirst = bandTop - yTop;
    if (rLast > bandTop + bandH - yTop) rLast = bandTop + bandH - yTop;
    if (rFirst < 0) rFirst = 0;
    if (rLast > (int)g.glyphH) rLast = g.glyphH;
    if (rLast <= rFirst) return;
    for (int r = rFirst; r < rLast; r++) {
        // yTop은 음수가 될 수 있다(2줄 위줄: 0-16). C++ 나머지 부호 문제를 같은 식으로 보정한다.
        int phaseY = (((r + yTop) % 4) + 4) % 4;
        uint8_t row_mask = 0;
        for (int px = 0; px < 8; px++) {
            if (bayer_matrix[phaseY][(phaseX + px) % 4] < density) row_mask |= (1 << px);
        }
        // 한 행의 마스크를 모든 바이트에 재사용한다: 바이트 b의 실제 열 위상은
        // (bx + b*8 + px) % 4 = (bx + px) % 4 (8b는 4의 배수) — 위상이 유지된다.
        for (int b = 0; b < bw; b++) {
            uint8_t original = data[r * bw + b];
            uint8_t masked = original & row_mask;
            if (masked) u8g2->drawBitmap(bx + (b * 8), yTop + r, 1, 1, &masked);
        }
    }
}

void Renderer::drawZoomedChar(int screenIdx, const String& charStr, int x, int scale_percent,
                              int y_offset, int bandTop, int bandH) {
    if (!_screens || screenIdx >= NUM_SCREENS || scale_percent <= 0) return;
    if (scale_percent == 100) { drawSingleChar(screenIdx, charStr, x, y_offset); return; }

    const CachedChar* cc_ptr = findChar(charStr);
    if (!cc_ptr || !cc_ptr->geom) { drawSingleChar(screenIdx, charStr, x, y_offset); return; }

    const uint8_t* data = getCharDataPtr(cc_ptr);
    const CellGeometry& g = *cc_ptr->geom;
    U8G2* u8g2 = _screens[screenIdx];
    int bw = g.bytesPerRow;
    int orig_w = g.drawW;
    int target_w = (orig_w * scale_percent) / 100;
    int target_h = (g.glyphH * scale_percent) / 100;
    if (target_w <= 0 || target_h <= 0) return;

    int bx = x + g.xOffset;
    int start_x = bx + (orig_w - target_w) / 2;
    // 스케일된 래스터의 중앙을 밴드 중앙(y_offset + LINE_HEIGHT/2)에 맞춘다.
    //   start_y + target_h/2 = y_offset + (LINE_HEIGHT - target_h)/2 + target_h/2
    //                        = y_offset + LINE_HEIGHT/2
    int start_y = y_offset + (LINE_HEIGHT - target_h) / 2;
    // [v5] 줌은 밴드를 넘게 커질 수 있다(64px 래스터 × 150% = 96px). 행 범위를 자른다.
    int rFirst = 0, rLast = target_h;
    if (rFirst < bandTop - start_y) rFirst = bandTop - start_y;
    if (rLast > bandTop + bandH - start_y) rLast = bandTop + bandH - start_y;
    if (rFirst < 0) rFirst = 0;
    if (rLast > target_h) rLast = target_h;
    if (rLast <= rFirst) return;
    for (int r = rFirst; r < rLast; r++) {
        int src_y = (r * g.glyphH) / target_h;
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

void Renderer::drawScaledChar(int screenIdx, const String& charStr, int x, int h, int y_offset) {
    if (!_screens || screenIdx >= NUM_SCREENS || h <= 0) return;
    U8G2* u8g2 = _screens[screenIdx];

    const CachedChar* cc_ptr = findChar(charStr);
    if (!cc_ptr || !cc_ptr->geom) { drawFallbackChar(u8g2, charStr, x, y_offset); return; }

    const uint8_t* data = getCharDataPtr(cc_ptr);
    const CellGeometry& g = *cc_ptr->geom;
    // 대상 높이가 원본 글리프 높이와 같으면 스케일링 없이 그대로 그린다.
    // (원본의 `h == 64` 하드코드는 한글 64px 글리프 전용 가정이었다 —
    //  영어판 32px 글리프에서는 64가 "2배 확대"를 뜻하므로 제거해야 한다)
    if (h == (int)g.glyphH) { drawSingleChar(screenIdx, charStr, x, y_offset); return; }

    int bw = g.bytesPerRow;
    int bx = x + g.xOffset;
    // [v5] 플립 애니메이션의 h(0..LINE_HEIGHT)는 "보일 잉크 높이"다. 래스터가
    //   밴드보다 높아도(48×64) 잉크는 밴드 크기(LINE_HEIGHT) 창 안에 중앙 정렬돼
    //   있으므로, 래스터 중앙의 LINE_HEIGHT 창을 h 높이로 눌러 밴드 중앙에 놓는다.
    //   glyphH=32(구 형식)면 창 = 래스터 전체 — 기존 동작과 동일하다.
    int start_y = y_offset + (LINE_HEIGHT - h) / 2;
    const int winTop = ((int)g.glyphH - LINE_HEIGHT) / 2;   // 창 시작 행 (64 → 16)
    for (int i = 0; i < h; i++) {
        int src_y = winTop + (i * LINE_HEIGHT) / h;
        u8g2->drawBitmap(bx, start_y + i, bw, 1, data + (src_y * bw));
    }
}

bool Renderer::getCharData(const String& text, CharData outChars[], int& count, bool singleLine) {
    count = 0;
    if (!outChars) return false;
    if (text == "") return true;

    const CellGeometry& g = defaultGeometry();

    // 원본 문자열을 유지한 채 부분 문자열 포인터만 얻는다 (복사 비용 없음).
    // maxInkWidth를 넘겨 글자가 적은 줄의 간격이 넓어지도록 한다 (PLAN §6.16).
    // 캐시가 없을 때(=0)는 확장하지 않아 기존 고정 피치로 돌아간다.
    //
    // [§6.16b] inkOfTrampoline을 넘겨 **줄마다 실제 잉크**로 상한을 건다.
    //   layoutWrap은 순수 함수로 유지된다 — 캐시를 직접 아는 쪽은 이 trampoline뿐.
    LayoutChar laid[LAYOUT_MAX_CHARS];
    int laidCount = 0;
    bool complete = layoutWrap(text.c_str(), text.length(), g, laid,
                                LAYOUT_MAX_CHARS, SCREEN_WIDTH, laidCount,
                                singleLine, maxInkWidth,
                                &Renderer::inkOfTrampoline, this);
    count = laidCount;
    for (int i = 0; i < laidCount; i++) {
        outChars[i].c = String(laid[i].text, laid[i].len);   // String 생성
        outChars[i].x = laid[i].x;
        outChars[i].y = laid[i].y;      // 세로 중앙 정렬된 Y (호출부가 재계산하지 않는다)
        outChars[i].line = laid[i].line;
    }
    return complete;
}
