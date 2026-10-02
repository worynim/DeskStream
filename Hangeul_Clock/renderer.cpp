// worynim@gmail.com
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

int Renderer::charBitWidth(const CachedChar* cc) const {
    // 256바이트 이하는 32px(4바이트/행), 그 위는 64px(8바이트/행) 글자다.
    return (cc->size <= 256) ? 4 : 8;
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
    int bw = charBitWidth(cc_ptr);
    int bx = (bw == 4) ? x : x - 16;
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
    int bw = charBitWidth(cc_ptr);
    int orig_w = bw * 8;
    int target_w = (orig_w * scale_percent) / 100;
    int target_h = (64 * scale_percent) / 100;
    if (target_w <= 0 || target_h <= 0) return;

    int bx = (bw == 4) ? x : x - 16;
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
        int bw = charBitWidth(cc_ptr);
        int bx = (bw == 4) ? x : x - 16;
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

// ── 눈 조립 픽셀 운동학 ────────────────────────────────────────────────
// 부동소수점을 쓰지 않는다. ESP32-C3에서 프레임마다 부동소수점 연산을 하지 않고,
// 정수 나눗셈만으로 같은 궤적을 재현한다.
/** 눈송이 출발 높이. 화면 위(음수)여서 진행도 0에서는 보이지 않는다. */
#define ANIM_SPAWN_Y (-2)
/** 도착 시각의 이론적 최댓값. 이 값 이상이면 전 픽셀이 정착한 것이다. */
#define ANIM_ARRIVAL_MAX 240
/** 도착 시각 분산폭의 절반 (같은 행의 픽셀들이 한꺼번에 도착하지 않게 흩뿌리는 폭) */
#define ARRIVAL_JITTER 40
/** 소멸 애니메이션에서 픽셀마다 다른 출발 지연 (0~47) */
#define DISPERSE_DELAY_SPAN 48

/**
 * @brief 눈 조립 애니메이션의 단일 픽셀 상태
 * @note 좌표는 "확정된(settled) 위치"를 기준으로 한다. p.on이 0이면 화면 밖(비행 전/후)이라 그리지 않는다.
 */
struct AnimPixel {
    int16_t x;    /**< 화면 x 좌표 (화면 밖일 수 있음) */
    int16_t y;    /**< 화면 y 좌표 (화면 밖일 수 있음) */
    uint8_t on;   /**< 1: 그리기, 0: 화면 밖이므로 건너뛴다 */
};

/**
 * @brief 좌표 기반 결정적 해시
 * @details 같은 입력이면 언제나 같은 값을 낸다. 눈송이는 매 프레임 새로 뽑지 않고
 *          이 해시로 재계산하므로, 프레임 간에 위치가 깜빡이지 않는다.
 */
static uint16_t animPixelHash(uint16_t seed, int16_t x, int16_t y) {
    uint32_t h = (uint32_t)seed * 2654435761u;
    h ^= (uint32_t)(uint16_t)(x * 40503) + 0x9E3779B9u;
    h ^= (uint32_t)(uint16_t)(y * 42137) + 0x85EBCA6Bu;
    h ^= (h >> 15); h *= 2246822519u; h ^= (h >> 13);
    return (uint16_t)(h >> 8);
}

/**
 * @brief 픽셀이 제자리에 도착하는 진행도(0~255)
 * @details 글자 아래쪽(y 클수록)일수록 늦게 도착하고, 같은 높이는 해시로 흩뿌려
 *          눈송이가 한꺼번에 쏟아지지 않게 한다. 값은 항상 [0, ANIM_ARRIVAL_MAX] 범위다.
 */
static uint8_t animArrivalTick(uint16_t seed, int16_t px, int16_t py) {
    // 하단일수록 늦게 도착하는 기본 경사와 해시 기반 분산을 합친다.
    int32_t base = ((int32_t)py * 200) / SCREEN_HEIGHT;
    int32_t jitter = (int32_t)(animPixelHash(seed, px, py) % (ARRIVAL_JITTER * 2 + 1)) - ARRIVAL_JITTER;
    int32_t tick = base + jitter;
    if (tick < 0) tick = 0;
    if (tick > ANIM_ARRIVAL_MAX) tick = ANIM_ARRIVAL_MAX;
    return (uint8_t)tick;
}

/**
 * @brief 위에서 떨어져 조립되는 픽셀의 현재 좌표
 * @details progress >= 도착 시각이면 제자리에 정착한다. 아직 비행 중이면 e = t² 가속으로
 *          낙하하며, 좌우로는 출발점과 도착점 사이를 선형 이동하며 흔들린다.
 */
static AnimPixel animAssemblingPixel(uint16_t seed, int16_t px, int16_t py, uint8_t progress) {
    AnimPixel p;
    uint8_t arrive = animArrivalTick(seed, px, py);

    p.x = px; p.y = py; p.on = 1;
    if (progress >= arrive) return p; // 도착 시각이 0이면 여기서 정착한다

    // t = num/den (0 < t < 1), e = t² 로 중력 가속을 만든다.
    int32_t num = progress;
    int32_t den = arrive;
    int32_t e_num = num * num;
    int32_t e_den = den * den;

    int32_t spawn = animPixelHash((uint16_t)(seed ^ 0x5A5A), px, py) % SCREEN_WIDTH;
    int32_t sway = (int32_t)(animPixelHash((uint16_t)(seed ^ 0xA5A5), px, py) % 9) - 4; // -4..+4

    p.x = (int16_t)(spawn + ((int32_t)px - spawn) * num / den
                    + sway * num * (den - num) / (den * 48));
    p.y = (int16_t)(ANIM_SPAWN_Y + ((int32_t)py - ANIM_SPAWN_Y) * e_num / e_den);
    p.on = (uint8_t)(p.y >= 0 && p.y < SCREEN_HEIGHT);
    return p;
}

/**
 * @brief 아래로 가라앉으며 흩어지는 픽셀의 현재 좌표
 * @details 새 글자가 위에서 내려오는 것과 방향을 통일한다. 픽셀마다 출발 지연과
 *          낙하 속도가 달라 눈이 부서지듯 흩어진다. ANIM_PROGRESS_FULL이면 모두 화면 밖이다.
 */
static AnimPixel animDispersingPixel(uint16_t seed, int16_t px, int16_t py, uint8_t progress) {
    AnimPixel p;
    uint16_t h = animPixelHash(seed, px, py);
    int32_t delay = (int32_t)(h % DISPERSE_DELAY_SPAN);

    p.x = px; p.y = py; p.on = 1;
    if ((int32_t)progress <= delay) return p; // 아직 출발 전이면 제자리에 남는다

    int32_t num = (int32_t)progress - delay;
    int32_t den = ANIM_PROGRESS_FULL - delay;
    int32_t e_num = num * num;
    int32_t e_den = den * den;
    int32_t drift = (int32_t)((h >> 8) % 7) - 3; // -3..+3

    p.y = (int16_t)(py + (SCREEN_HEIGHT - py) * e_num / e_den);
    p.x = (int16_t)(px + drift * num / 128);
    p.on = (uint8_t)(p.y < SCREEN_HEIGHT);
    return p;
}

void Renderer::drawAssemblingChar(int screenIdx, const String& charStr, int x, uint8_t progress, uint16_t seed) {
    if (!_screens || screenIdx >= NUM_SCREENS) return;

    const CachedChar* cc_ptr = findChar(charStr);
    if (!cc_ptr) {
        // 커스텀 비트맵이 없으면 픽셀 단위 조립이 불가능하므로 시스템 폰트로 대체한다.
        if (progress >= ANIM_SNOW_FALLBACK_THRESHOLD) drawSingleChar(screenIdx, charStr, x, 0);
        return;
    }
    // 전 픽셀이 정착한 뒤에는 비트 순회 없이 정적 렌더링으로 넘긴다.
    if (progress >= ANIM_ARRIVAL_MAX) { drawSingleChar(screenIdx, charStr, x, 0); return; }

    const uint8_t* data = getCharDataPtr(cc_ptr);
    U8G2* u8g2 = _screens[screenIdx];
    int bw = charBitWidth(cc_ptr);
    int bx = (bw == 4) ? x : x - 16;

    for (int r = 0; r < 64; r++) {
        for (int c = 0; c < bw * 8; c++) {
            if (!(data[r * bw + (c >> 3)] & (0x80 >> (c & 7)))) continue;
            AnimPixel p = animAssemblingPixel(seed, (int16_t)(bx + c), (int16_t)r, progress);
            if (p.on) u8g2->drawPixel(p.x, p.y);
        }
    }
}

void Renderer::drawDispersingChar(int screenIdx, const String& charStr, int x, uint8_t progress, uint16_t seed) {
    if (!_screens || screenIdx >= NUM_SCREENS) return;

    const CachedChar* cc_ptr = findChar(charStr);
    if (!cc_ptr) {
        if (progress < ANIM_SNOW_FALLBACK_THRESHOLD) drawSingleChar(screenIdx, charStr, x, 0);
        return;
    }

    const uint8_t* data = getCharDataPtr(cc_ptr);
    U8G2* u8g2 = _screens[screenIdx];
    int bw = charBitWidth(cc_ptr);
    int bx = (bw == 4) ? x : x - 16;

    for (int r = 0; r < 64; r++) {
        for (int c = 0; c < bw * 8; c++) {
            if (!(data[r * bw + (c >> 3)] & (0x80 >> (c & 7)))) continue;
            AnimPixel p = animDispersingPixel(seed, (int16_t)(bx + c), (int16_t)r, progress);
            if (p.on) u8g2->drawPixel(p.x, p.y);
        }
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
