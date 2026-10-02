// worynim@gmail.com
/**
 * @file renderer_geometry.h
 * @brief 글자 셀 기하 판별 (Arduino/U8g2 의존성 없음)
 * @details renderer.cpp는 U8g2·LittleFS에 의존해 네이티브 테스트가 불가능하다.
 *          AGENTS.md 테스트 규칙을 지키기 위해 순수 기하 로직만 이 TU로 분리했다.
 * @note [SYNC] 원본: Hangeul_Clock/renderer.cpp의 `if (size <= 256)` 분기를 표로 옮김
 */
#ifndef RENDERER_GEOMETRY_H
#define RENDERER_GEOMETRY_H

#include <stdint.h>

/**
 * @brief 글자 셀의 렌더링 기하
 * @details drawBitmap()은 row-major 바이트 배열을 받으므로 픽셀 폭(glyphW)과
 *          저장 행 바이트 수(bytesPerRow)는 서로 다를 수 있다.
 *          예) 14px 글리프는 바이트 경계에 맞지 않아 2 bytes/row(16px 폭)로 저장된다.
 */
struct CellGeometry {
    uint8_t glyphW;        // 셀 폭 (실제 글자 영역)
    uint8_t glyphH;        // 픽셀 높이
    uint8_t bytesPerRow;   // 저장 시 1행 바이트 수 (drawBitmap의 3번째 인자)
    uint8_t drawW;         // bytesPerRow * 8 — drawBitmap이 덮는 폭
    int8_t  xOffset;       // drawBitmap(x + xOffset, ...) 기준점 보정
    uint8_t maxPerLine;    // 이 기하에서 1줄에 수용되는 최대 글자 수
};

/**
 * @brief 파일 크기로부터 셀 기하를 판별
 * @param size c_XX.bin 파일의 바이트 크기
 * @return 대응하는 기하. 알 수 없는 크기면 nullptr (호출자가 건너뛸 것)
 * @details 64/192/384/256/512는 서로 배타적이므로 크기 하나로 유일하게 결정된다.
 *          192B는 영어판 확장 래스터 48×32(잉크가 피치를 넘어 겹침) — PLAN §6.13.
 *          384B는 영어판 전체 래스터 48×64(잉크 64px까지, 줄 밴드 중앙 정렬) — PLAN §6.14.
 */
const CellGeometry* geometryForSize(uint32_t size);

/** @brief 현재 표시 모드의 기본 기하 (캐시 미적용 시 폴백 렌더링용) */
const CellGeometry& defaultGeometry();

#endif
