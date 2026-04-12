#ifndef RENDERER_H
#define RENDERER_H

#include <U8g2lib.h>
#include <vector>
#include <map>
#include "config.h"

struct CachedChar {
    String hex;
    std::vector<uint8_t> data;
};

struct CharData {
    String c;
    int x;
};

class Renderer {
public:
    Renderer();
    
    // 초기화 및 리소스 관리
    void setScreens(U8G2** screens);
    void loadBitmapCache();
    const CachedChar* findChar(const String& s);
    String getHexKey(const String& s);

    // 그리기 프리미티브
    void drawSingleChar(int screenIdx, const String& charStr, int x, int y_offset = 0);
    void drawDitheredChar(int screenIdx, const String& charStr, int x, int density);
    void drawZoomedChar(int screenIdx, const String& charStr, int x, int scale_percent);
    void drawScaledChar(int screenIdx, const String& charStr, int x, int h);
    
    // 텍스트 레이아웃 헬퍼
    void getCharData(const String& text, CharData outChars[8], int& count, bool centered);

    // 캐시 접근
    size_t getCacheSize() const { return bitmapCache.size(); }
    void clearCache();

private:
    U8G2** _screens = nullptr;
    std::vector<CachedChar> bitmapCache;
    std::map<String, int> cacheIndex;

    const uint8_t bayer_matrix[4][4] = {
        { 0,  8,  2, 10},
        {12,  4, 14,  6},
        { 3, 11,  1,  9},
        {15,  7, 13,  5}
    };
};

extern Renderer renderer;

#endif
