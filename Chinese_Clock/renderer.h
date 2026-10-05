// worynim@gmail.com
/**
 * @file renderer.h
 * @brief 커스텀 비트맵 렌더링 엔진 정의
 * @details LittleFS 비트맵 캐시 관리 및 디더·줌·플립 애니메이션의 픽셀 연산
 * @note [SYNC] 원본: ENG_Clock/renderer.h — 1줄 고정 피치에 맞춰 불필요한 부분을 제거했다.
 *       제거 근거는 각 멤버의 주석에 적었다 (PLAN §6.4와 같은 원칙).
 */
#ifndef RENDERER_H
#define RENDERER_H

#include <U8g2lib.h>
#include <vector>
#include <map>
#include "config.h"
#include "renderer_geometry.h"   // CellGeometry, geometryForSize, defaultGeometry

/** 캐시된 글리프 1개 */
struct CachedChar {
    String hex;                    // 파일명에서 온 대문자 16진 (캐시 키)
    uint32_t offset;               // flatBuffer 내 시작 바이트
    uint32_t size;                 // c_XX.bin 크기 (288)
    const CellGeometry* geom;      // loadBitmapCache()에서 판별해 채움
};

/**
 * @brief 정적 텍스트(시계·IP)의 한 줄 세로 위치 — (64 − 48) / 2 = 8
 * @details **애니메이션 프레임에는 이 상수를 쓰면 안 된다.**
 *          스크롤(상/하)은 글자를 LINE_HEIGHT만큼 세로로 이동시키므로,
 *          그리기 함수들은 y를 인자로 받아야 한다. 애니메이션은 LINE_TOP_Y에서
 *          시작한 상대 이동량을 직접 계산해 넘긴다.
 * @note ENG판의 CharData::y는 "줄 수에 따라 달라지는 값"이었다. 중국어판은 1줄이라
 *       그 역할은 없어졌지만, **애니메이션 이동량**이라는 역할은 남는다.
 *       (PLAN §6.5가 y_offset 제거를 정당화한 논리는 정적 배치에 대해서만 성립한다)
 */
#define LINE_TOP_Y ((SCREEN_HEIGHT - LINE_HEIGHT) / 2)

class Renderer {
public:
    Renderer();
    ~Renderer();   // flatBuffer 해제

    // === 초기화 및 리소스 관리 ===
    void setScreens(U8G2** screens);

    /**
     * @brief 슬롯의 비트맵 폰트를 읽어 캐시에 올린다
     * @param slot -1이면 설정(configManager)의 슬롯을 쓴다
     * @details **288B만 수용한다.** 다른 크기는 geometryForSize()가 nullptr을 주므로
     *          조용히 건너뛴다 — 기하를 확정할 수 없는 폰트를 추정으로 그리지 않는다.
     */
    void loadBitmapCache(int slot = -1);

    /** @brief 캐시된 글자 조회 (변경 없음) */
    const CachedChar* findChar(const String& s) const;

    /**
     * @brief 지금 캐시에 이 글자의 글리프가 있는가
     * @details **캐시를 바꾸지 않는다.** 슬롯을 옮기지 않고도 "이 슬롯 폰트로 이 문자판을
     *          그릴 수 있는가"를 판정하기 위한 조회다. 문자집합 상수도, FS 스캔도 필요 없다 —
     *          지금 화면에 그리려 하는 글자가 실제로 있는지만 보면 된다.
     *          캐시가 비어 있으면 false (아무것도 그릴 수 없다).
     */
    bool hasGlyph(const String& s) const;

    /** @brief 캐시된 글자가 실제로 쓰는 기하. 캐시에 없으면 nullptr */
    const CellGeometry* geometryOf(const String& s);

    /** @brief 문자열을 대문자 16진 캐시 키로 바꾼다 ("时" → "E697B9") */
    String getHexKey(const String& s) const;

    // === 그리기 프리미티브 ===
    //
    // [제거 — ENG판의 bandTop/bandH 인자]
    //   ENG판은 2줄이라 "밴드"(한 줄의 세로 영역)를 인자로 넘기고 이웃 줄로의
    //   새감을 막았다(64px 래스터 × 150% 줌 = 96px). 중국어판은 1줄이라 밴드가
    //   화면 전체이며, 화면 바깥으로 나가는 부분은 U8g2가 디스플레이 버퍼에서 자른다.
    //   따라서 밴드 인자를 둘 이유가 없다.
    //
    // [유지 — y 인자]
    //   정적 배치는 항상 LINE_TOP_Y지만, 스크롤 애니메이션은 글자를 세로로 이동시킨다.
    //   y를 고정하면 이전 글자와 새 글자가 같은 자리에 겹쳐져 애니메이션이 무동작이 되고,
    //   웹 미리보기(JS: off = animStep * (LINE_HEIGHT / 16))와 어긋난다.
    //
    // y는 **래스터 상단**이다 (래스터를 세로 중앙에 놓는 오프셋이 필요 없다 —
    // rasterTopY(y,g) = y + (LINE_HEIGHT − glyphH)/2 = y + 0, §6.5).
    // 결과적으로 아래 시그니처는 모두 파라미터 5개 이하 (AGENTS.md 파라미터 ≤ 5).

    /**
     * @brief 캐시 글리프 1자를 원래 크기로 그린다
     * @param x 셀 왼쪽 좌표 (래스터는 xOffset −8로 그 안에서 중앙 정렬된다)
     * @param y 래스터 상단 Y. 정적 배치는 LINE_TOP_Y, 애니메이션은 이동량을 더한 값
     */
    void drawSingleChar(int screenIdx, const String& charStr, int x, int y);

    /**
     * @brief 디더 페이드 — density 0(투명)~16(불투명)으로 글자를 드러낸다
     * @details 4×4 Bayer 행렬로 픽셀을 골라 칠한다. density ≥ 16은 원래 그리기.
     */
    void drawDitheredChar(int screenIdx, const String& charStr, int x, int y, int density);

    /** @brief 등비 확대 — scale_percent(%)로 래스터를 늘린다. 100이면 원래 그리기 */
    void drawZoomedChar(int screenIdx, const String& charStr, int x, int y, int scale_percent);

    /**
     * @brief 세로 스쿼시 — 래스터의 가시 높이만 h행으로 눌러 세로 중앙 정렬한다
     * @param h 보일 높이(픽셀). 0이면 아무것도 안 그린다. LINE_HEIGHT면 원래 그리기
     * @details 세로 플립 애니메이션용. 래스터가 48px라 창(window)은 래스터 전체다.
     */
    void drawScaledChar(int screenIdx, const String& charStr, int x, int y, int h);

    // === 캐시 접근 ===
    size_t getCacheSize() const { return bitmapCache.size(); }
    bool isCacheLoaded() const { return flatBuffer != nullptr; }
    void clearCache();

private:
    U8G2** _screens = nullptr;
    std::vector<CachedChar> bitmapCache;
    uint8_t* flatBuffer = nullptr;
    std::map<String, int> cacheIndex;

    /** @brief 캐시된 글자의 래스터 시작 포인터 (캐시 없으면 nullptr) */
    const uint8_t* getCharDataPtr(const CachedChar* cc) const;

    const uint8_t bayer_matrix[4][4] = {
        { 0,  8,  2, 10},
        {12,  4, 14,  6},
        { 3, 11,  1,  9},
        {15,  7, 13,  5}
    };
};

extern Renderer renderer;

#endif