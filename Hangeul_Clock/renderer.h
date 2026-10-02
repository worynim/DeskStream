// worynim@gmail.com
/**
 * @file renderer.h
 * @brief 고전 수치 및 한글 비트맵 렌더링 엔진 정의
 * @details 비트맵 캐시 관리 및 디더링, 줌, 스케일링 등 고수준 그래픽 효과 처리
 */
#ifndef RENDERER_H
#define RENDERER_H

#include <U8g2lib.h>
#include <vector>
#include <map>
#include "config.h"

struct CachedChar {
    String hex;
    uint32_t offset;
    uint32_t size;
};

struct CharData {
    String c;
    int x;
};

class Renderer {
public:
    Renderer();
    ~Renderer(); // 소멸자 추가 (메모리 해제)
    
    // 초기화 및 리소스 관리
    void setScreens(U8G2** screens);
    void loadBitmapCache(int slot = -1);
    const CachedChar* findChar(const String& s);
    const uint8_t* getCharDataPtr(const CachedChar* cc) const; // 데이터 포인터 획득 유틸리티
    String getHexKey(const String& s);

    // 그리기 프리미티브
    void drawSingleChar(int screenIdx, const String& charStr, int x, int y_offset = 0);
    void drawDitheredChar(int screenIdx, const String& charStr, int x, int density);
    void drawZoomedChar(int screenIdx, const String& charStr, int x, int scale_percent);
    void drawScaledChar(int screenIdx, const String& charStr, int x, int h);

    /**
     * @brief 눈송이가 떨어져 쌓이며 글자를 조립한다
     * @param progress 애니메이션 진행도 (0 ~ ANIM_PROGRESS_FULL)
     * @param seed    눈의 결정적 시드 (같은 값이면 같은 눈이 나온다)
     */
    void drawAssemblingChar(int screenIdx, const String& charStr, int x, uint8_t progress, uint16_t seed);

    /**
     * @brief 글자 픽셀이 아래로 가라앉으며 흩어진다
     * @param progress 애니메이션 진행도 (0 ~ ANIM_PROGRESS_FULL)
     * @param seed    눈의 결정적 시드
     */
    void drawDispersingChar(int screenIdx, const String& charStr, int x, uint8_t progress, uint16_t seed);

    // 텍스트 레이아웃 헬퍼
    void getCharData(const String& text, CharData outChars[8], int& count, bool centered);

    // 캐시 접근
    size_t getCacheSize() const { return bitmapCache.size(); }
    bool isCacheLoaded() const { return flatBuffer != nullptr; }
    void clearCache();

private:
    /** 글자 비트맵의 바이트 폭 (32px 글자=4, 64px 글자=8) */
    int charBitWidth(const CachedChar* cc) const;

    U8G2** _screens = nullptr;
    std::vector<CachedChar> bitmapCache;
    uint8_t* flatBuffer = nullptr;
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
