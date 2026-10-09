// worynim@gmail.com
/**
 * @file renderer_geometry.cpp
 * @brief 글자 셀 기하 판별 구현
 * @note [SYNC] ENG_Clock/renderer_geometry.cpp에서 한글판이 쓰는 두 크기만 남기고 옮김.
 *       원본 `if (cc_ptr->size <= 256) drawBitmap(x, y, 4, 64, data);
 *            else drawBitmap(x - 16, y, 8, 64, data);` 를 그대로 표로 옮긴 것이라
 *       xOffset 값은 원본 동작을 그대로 보존한다 (한글 폰트 회귀 없음).
 */
#include "renderer_geometry.h"

// === 크기 기반 기하 테이블 ===
// size = bytesPerRow × glyphH. 한글판은 256과 512 두 크기만 쓴다.
//   256 = 4B/행 × 64행 = 32px 글자
//   512 = 8B/행 × 64행 = 64px 글자
static const CellGeometry GEOM_256B = { 32, 64, 4, 32,  0, 4 };  // 32px 한글 (구 레거시)
static const CellGeometry GEOM_512B = { 64, 64, 8, 64, -16, 2 };  // 64px 한글 (현대)

const CellGeometry* geometryForSize(uint32_t size) {
    if (size == 256) return &GEOM_256B;
    if (size == 512) return &GEOM_512B;
    return nullptr;   // 기하를 확정할 수 없으므로 호출자가 건너뛴다 (초과 읽음 원천 차단)
}

const CellGeometry& defaultGeometry() {
    return GEOM_256B;   // 한글판 기본 (32px)
}
