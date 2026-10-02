// worynim@gmail.com
/**
 * @file renderer_geometry.cpp
 * @brief 글자 셀 기하 판별 구현
 * @note [SYNC] 원본: Hangeul_Clock/renderer.cpp
 *       원본 `if (cc_ptr->size <= 256) drawBitmap(x, y, 4, 64, data);
 *            else drawBitmap(x - 16, y, 8, 64, data);` 를 그대로 표로 옮긴 것.
 *       xOffset 값은 원본 동작을 그대로 보존한다 (한글 폰트 회귀 없음).
 */
#include "renderer_geometry.h"

// === 크기 기반 기하 테이블 ===
// size는 bytesPerRow × glyphH 로 유일하게 결정되며, 64/192/384/256/512는 서로 배타적.
static const CellGeometry GEOM_64B  = { 14, 32, 2, 16,  0, 9 };  // 영어 2줄 (구 형식 — 기존 슬롯 호환)
static const CellGeometry GEOM_192B = { 14, 32, 6, 48, -17, 9 }; // 영어 확장 래스터 48×32 (v4 — PLAN §6.13)
// [여백 수정 v5 — PLAN §6.14] 48×64 래스터. 잉크 합집합이 32px를 넘는 큰 폰트에서
// 위가 잘리지 않도록 래스터를 화면 전체 높이로 늘렸다. 래스터는 줄 밴드 중앙에
// 놓이므로(렌더러 yTop 공식) 1줄 화면에서 잉크 64px까지 온전히 표시된다.
static const CellGeometry GEOM_384B = { 14, 64, 6, 48, -17, 9 };
static const CellGeometry GEOM_256B = { 32, 64, 4, 32,  0, 4 };  // 구 레거시
static const CellGeometry GEOM_512B = { 64, 64, 8, 64, -16, 2 };  // 현대 한글

const CellGeometry* geometryForSize(uint32_t size) {
    if (size == 64)  return &GEOM_64B;
    if (size == 192) return &GEOM_192B;
    if (size == 384) return &GEOM_384B;
    if (size == 256) return &GEOM_256B;
    if (size == 512) return &GEOM_512B;
    return nullptr;   // 기하를 확정할 수 없으므로 호출자가 건너뛴다
}

const CellGeometry& defaultGeometry() {
    return GEOM_64B;   // 영어판 기본 (PLAN §3.1)
}
