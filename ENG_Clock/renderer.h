// worynim@gmail.com
/**
 * @file renderer.h
 * @brief 고전 수치 및 커스텀 비트맵 렌더링 엔진 정의
 * @details 비트맵 캐시 관리 및 디더링, 줌, 스케일링 등 고수준 그래픽 효과 처리
 * @note [SYNC] 원본: Hangeul_Clock/renderer.h — 글자 지오메트리 파라미터화
 */
#ifndef RENDERER_H
#define RENDERER_H

#include <U8g2lib.h>
#include <vector>
#include <map>
#include "config.h"
#include "renderer_geometry.h"   // CellGeometry, geometryForSize, defaultGeometry

struct CachedChar {
    String hex;
    uint32_t offset;
    uint32_t size;
    const CellGeometry* geom;   // loadBitmapCache()에서 판별해 채움
};

struct CharData {
    String c;
    int x;
    int y;          // 화면 Y 좌표 — layoutWrap()이 줄 수에 따라 세로 중앙 정렬해 준다
    uint8_t line;   // 0 = 상단, 1 = 하단
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
    // 캐시된 글자가 실제로 쓰는 기하. 캐시에 없으면 nullptr (호출자가 defaultGeometry로 폴백).
    // [v5 — PLAN §6.14] IP 화면 도트 앵커가 실제 숫자 기하를 따르게 하려고 추가했다.
    const CellGeometry* geometryOf(const String& s);

    // 그리기 프리미티브
    // y_offset은 줄 밴드의 위쪽 y(레이아웃 y)다. 래스터는 밴드 중앙에 놓인다 —
    // 그리기 상단 y = y_offset + (LINE_HEIGHT - glyphH)/2 (v5 — PLAN §6.14).
    // glyphH=32(구 형식)면 그대로 y_offset이므로 기존 슬롯 렌더링은 불변이다.
    void drawSingleChar(int screenIdx, const String& charStr, int x, int y_offset = 0);
    /**
     * @brief 디더 페이드 글자. bandTop/bandH로 그릴 세로 구간을 제한한다.
     * @details [v5] 래스터가 밴드보다 높아질 수 있어(48×64) 2줄에서는 이웃 줄 밴드로
     *          새지 않게 행 범위를 자른다. 기본값은 화면 전체 — 1줄 화면은 잘라 내지 않는다.
     */
    void drawDitheredChar(int screenIdx, const String& charStr, int x, int density, int y_offset = 0,
                          int bandTop = 0, int bandH = SCREEN_HEIGHT);
    /**
     * @brief 등비 확대 글자. bandTop/bandH로 그릴 세로 구간을 제한한다.
     * @details [v5] 48×64 래스터 × 150% 줌은 96px로 밴드를 크게 넘으므로 2줄에서는
     *          행 범위를 자른다. 기본값은 화면 전체 — 1줄 화면은 화면 경계까지만 자른다.
     */
    void drawZoomedChar(int screenIdx, const String& charStr, int x, int scale_percent, int y_offset = 0,
                        int bandTop = 0, int bandH = SCREEN_HEIGHT);
    void drawScaledChar(int screenIdx, const String& charStr, int x, int h, int y_offset = 0);

    /**
     * @brief 한 줄의 세로 밴드로 잘라 글자 하나를 그린다 (스크롤 애니메이션 전용)
     * @param bandTop  허용할 세로 구간의 시작 y
     * @param bandH    허용할 세로 구간의 높이 (보통 LINE_HEIGHT)
     * @details [버그 3] 스크롤은 글자를 y ± 한 줄 높이만큼 움직이므로, 그리는 순간의
     *          y만으로는 이웃 줄 밴드로 새어나갈 것을 막을 수 없다. 그래서 여기서 자른다.
     *
     * @note  플립(drawScaledChar)은 설계상 [baseY, baseY+LINE_HEIGHT) 안에만 그린다.
     *        디더·줌은 bandTop/bandH 인자로 2줄에서 밴드를 제한한다 (v5).
     *        웹 미리보기 drawByLine() 은 ctx.clip()으로 같은 결과를 낸다.
     */
    void drawSingleCharClipped(int screenIdx, const String& charStr, int x, int y_offset,
                               int bandTop, int bandH);

    // 텍스트 레이아웃 헬퍼
    // @param singleLine true면 "어절 2개 이상 → 2줄" 규칙을 건너뛴다 (숫자 모드 "02 H").
    //                   이전의 미사용 `centered` 인자 자리를 [수정할 사항 1] 의미로 교체했다.
    // @return true면 문자열 전체가 배치됨, false면 2줄 용량 초과로 잘림
    bool getCharData(const String& text, CharData outChars[], int& count, bool singleLine);

    // 캐시 접근
    size_t getCacheSize() const { return bitmapCache.size(); }
    bool isCacheLoaded() const { return flatBuffer != nullptr; }
    void clearCache();

private:
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
