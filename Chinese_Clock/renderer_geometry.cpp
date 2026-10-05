// worynim@gmail.com
/**
 * @file renderer_geometry.cpp
 * @brief 글자 셀 기하 판별 구현
 * @note [SYNC] 구조는 ENG_Clock/renderer_geometry.cpp를 따른다.
 *       ENG판의 5종 기하 테이블을 중국어판 1종(288B)으로 교체했다 (PLAN §6.5).
 */
#include "renderer_geometry.h"

// === 크기 기반 기하 테이블 ===
//
// 288B = 6 bytes/row × 48 rows = 래스터 48×48.
//   glyphW 32 : 한자 한 자의 셀 폭. 4 × 32 = 128 = 화면 폭이므로 1줄 4자가 정확히 들어간다.
//   glyphH 48 : 래스터 높이. 40px 슬라이더 최대치 + 4px씩 여유 → 상하 클리핑 불가.
//   drawW   48 : bytesPerRow × 8. 48px 폭이라 32px 피치에 대해 좌우 8px씩 여유.
//   xOffset −8 : −(48−32)/2. 잉크를 32px 피치 안에 가로 중앙 정렬한다.
//   maxPerLine 4 : 4 × 32 = 128px. §3 전수표의 최장 표현(4자)과 정확히 일치 —
//                  전수 테스트(test_chinese_time)가 이 사실을 확인해 준다.
static const CellGeometry GEOM_288B = { 32, 48, 6, 48, -8, 4 };

const CellGeometry* geometryForSize(uint32_t size) {
    if (size == 288) return &GEOM_288B;
    return nullptr;   // 기하를 확정할 수 없으므로 호출자가 건너뛴다
}

const CellGeometry& defaultGeometry() {
    return GEOM_288B;
}

uint8_t inkWidthOf(const uint8_t* data, uint8_t bytesPerRow, uint8_t glyphH) {
    if (!data || bytesPerRow == 0 || glyphH == 0) return 0;

    // 순회가 행 우선(row-major)이므로 "처음/마지막으로 만난 픽셀"이 곧 최좌/최우열이 아니다.
    //   W처럼 위쪽이 가장 넓은 글리프는 열 14~34가 모두 켜져도, 최하단 잉크 행의 우끝이 29라서
    //   21을 16으로 잘못 잰다. 따라서 양쪽 모두 명시적 min/max로 갱신한다.
    int first = -1, last = -1;
    for (int r = 0; r < glyphH; r++) {
        const uint8_t* row = data + r * bytesPerRow;
        for (int b = 0; b < bytesPerRow; b++) {
            if (row[b] == 0) continue;   // 빈 바이트는 8열을 한 번에 건너뛴다
            for (int p = 0; p < 8; p++) {
                if (!(row[b] & (0x80 >> p))) continue;   // MSB 우선 — drawBitmap/packGlyph 규약
                const int col = b * 8 + p;
                if (first < 0 || col < first) first = col;
                if (col > last) last = col;
            }
        }
    }
    if (first < 0) return 0;   // 완전히 빈 글리프(공백 등)
    return (uint8_t)(last - first + 1);
}