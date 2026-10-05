// worynim@gmail.com
/**
 * @file renderer_geometry.h
 * @brief 글자 셀 기하 판별 (Arduino/U8g2 의존성 없음)
 * @details renderer.cpp는 U8g2·LittleFS에 의존해 네이티브 테스트가 불가능하다.
 *          AGENTS.md 테스트 규칙을 지키기 위해 순수 기하 로직만 이 TU로 분리했다.
 * @note [SYNC] 구조는 ENG_Clock/renderer_geometry.h를 따른다. 기하 테이블 값은 전부 교체 (PLAN §6.5)
 */
#ifndef RENDERER_GEOMETRY_H
#define RENDERER_GEOMETRY_H

#include <stdint.h>

/**
 * @brief 글자 셀의 렌더링 기하
 * @details drawBitmap()은 row-major 바이트 배열을 받으므로 픽셀 폭(glyphW)과
 *          저장 행 바이트 수(bytesPerRow)는 서로 다를 수 있다.
 *          중국어판은 drawBitmap(x − 8, ...)로 잉크를 32px 피치 안에 중앙 정렬한다.
 */
struct CellGeometry {
    uint8_t glyphW;        // 셀 폭 (실제 글자 영역) — 32
    uint8_t glyphH;        // 픽셀 높이 (래스터 행 수) — 48
    uint8_t bytesPerRow;   // 저장 시 1행 바이트 수 (drawBitmap의 3번째 인자) — 6
    uint8_t drawW;         // bytesPerRow * 8 — drawBitmap이 덮는 폭 — 48
    int8_t  xOffset;       // drawBitmap(x + xOffset, ...) 기준점 보정 — −8
    uint8_t maxPerLine;    // 이 기하에서 1줄에 수용되는 최대 글자 수 — 4
};

/**
 * @brief 파일 크기로부터 셀 기하를 판별
 * @param size c_XX.bin 파일의 바이트 크기
 * @return 대응하는 기하. 알 수 없는 크기면 nullptr (호출자가 건너뛸 것)
 * @details 중국어판은 **288B 단일 크기**만 쓴다.
 *          48px 래스터(6 bytes × 48 rows)는 ENG판 v4(좌우 클리핑)·v5(상하 클리핑)
 *          실패를 처음부터 배제하기 위해 두 교훈을 이미 반영한 값이다 (PLAN §5.4).
 *
 * @note ENG판의 64/192/256/384/512B 항목은 **이 폴더에서 제거**했다.
 *       기존 슬롯에 그 크기의 폰트가 남아 있어도 기하를 확정할 수 없으므로 nullptr을
 *       돌려 호출자가 건너뛴다 — 잘못된 기하로 그리는 것보다 나음이다.
 */
const CellGeometry* geometryForSize(uint32_t size);

/** @brief 현재 표시 모드의 기본 기하 (캐시 미적용 시 폴백 렌더링용) */
const CellGeometry& defaultGeometry();

/**
 * @brief 비트맵 1글자의 실제 잉크 폭 (픽셀)
 *
 * @param data        row-major 비트맵. U8g2 drawBitmap 규약(MSB 우선) — 0x80이 가장 왼쪽 열
 * @param bytesPerRow 1행당 바이트 수 (CellGeometry::bytesPerRow)
 * @param glyphH      행 수 (CellGeometry::glyphH)
 * @return 잉크가 차지하는 열 수. 잉크가 없거나 인자가 잘못되면 0
 *
 * @details 순회는 **행 우선(row-major)**이라 "처음/마지막으로 만난 픽셀"이 곧
 *          최좌/최우열이 아니다. 양쪽 모두 명시적 min/max로 갱신해야 한다.
 * @note 잉크는 래스터 안에서 가로 중앙 정렬된다(웹 Font Studio의 glyphCenterX).
 */
uint8_t inkWidthOf(const uint8_t* data, uint8_t bytesPerRow, uint8_t glyphH);

#endif