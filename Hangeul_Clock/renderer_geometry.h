// worynim@gmail.com
/**
 * @file renderer_geometry.h
 * @brief 글자 셀 기하 판별 (Arduino/U8g2 의존성 없음)
 * @details renderer.cpp는 U8g2·LittleFS에 의존해 네이티브 테스트가 불가능하다.
 *          AGENTS.md 테스트 규칙을 지키기 위해 순수 기하 로직만 이 TU로 분리했다.
 * @note [SYNC] ENG_Clock/renderer_geometry.h에서 한글판이 쓰는 두 크기만 남기고 옮김
 */
#ifndef RENDERER_GEOMETRY_H
#define RENDERER_GEOMETRY_H

#include <stdint.h>

/**
 * @brief 글자 셀의 렌더링 기하
 * @details drawBitmap()은 row-major 바이트 배열을 받으므로 픽셀 폭(glyphW)과
 *          저장 행 바이트 수(bytesPerRow)는 서로 다를 수 있다.
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
 * @details **이 함수가 초과 읽음의 원천 차단 지점이다.**
 *          원본은 `charBitWidth()`가 `(size <= 256) ? 4 : 8`을 돌려줘,
 *          257~511바이트 파일이 검증을 통과하면서 8×64 = 512바이트로 읽혔다
 *          (malloc된 크기는 실제 파일 크기뿐이라 힙 경계를 넘었다).
 *          크기→바이트 수가 배타적 매핑이면, 모르는 크기는 아예 로드되지 않는다.
 *          한글판이 쓰는 크기는 256(32px 한글)과 512(64px 한글) 둘이다.
 */
const CellGeometry* geometryForSize(uint32_t size);

/** @brief 캐시 미적용 시 폴백 렌더링용 기본 기하 (32px 한글) */
const CellGeometry& defaultGeometry();

#endif
