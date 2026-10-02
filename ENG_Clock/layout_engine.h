// worynim@gmail.com
/**
 * @file layout_engine.h
 * @brief 어절 단위 2줄 레이아웃 계산 (Arduino 의존성 없음)
 * @details renderer.cpp는 U8g2·LittleFS에 의존해 네이티브 테스트가 불가능하다.
 *          AGENTS.md 테스트 규칙을 지키기 위해 줄바꿈 규칙을 순수 모듈로 분리했다.
 * @note [SYNC] 원본: Hangeul_Clock/renderer.cpp의 getCharData()를 어절 단위로 확장
 */
#ifndef LAYOUT_ENGINE_H
#define LAYOUT_ENGINE_H

#include <stdint.h>
#include "renderer_geometry.h"

/** 한 화면에 배치할 최대 글자 수 (2줄 × 9자) */
#define LAYOUT_MAX_CHARS 18

/**
 * @brief 줄 높이 — config.h의 LINE_HEIGHT와 같아야 한다
 * @details 이 모듈은 Arduino에 의존하지 않으므로 config.h를 include할 수 없다.
 *          값이 어긋나면 renderer.cpp의 static_assert가 컴파일을 막는다.
 */
#ifndef LAYOUT_LINE_HEIGHT
#define LAYOUT_LINE_HEIGHT 32
#endif

/**
 * @brief 화면 높이 — config.h의 SCREEN_HEIGHT와 같아야 한다
 * @details 1줄 텍스트의 세로 중앙 정렬에 사용된다 ((64 - 32) / 2 = 16).
 */
#ifndef LAYOUT_SCREEN_HEIGHT
#define LAYOUT_SCREEN_HEIGHT 64
#endif

/** 화면에 배치되는 최대 줄 수 */
#define LAYOUT_MAX_LINES 2

/**
 * @brief 레이아웃된 글자 1개
 */
struct LayoutChar {
    const char* text;   // 원본 문자열 내의 부분 문자열 (NUL 종료 아님)
    uint8_t len;        // UTF-8 바이트 길이
    int16_t x;          // 화면 좌표 (픽셀)
    int16_t y;          // 화면 Y 좌표 — 줄 수에 따라 세로 중앙 정렬된 값
    uint8_t line;       // 0 = 상단, 1 = 하단
};

/**
 * @brief 어절 단위 줄바꿈으로 글자를 배치
 *
 * @param text        원본 문자열 (UTF-8)
 * @param textLen     text의 바이트 길이
 * @param geom        사용할 셀 기하 (glyphW, maxPerLine)
 * @param out         결과 배열 (최대 LAYOUT_MAX_CHARS개)
 * @param outCapacity out 배열의 용량
 * @param screenWidth 화면 폭 (기본 128)
 * @param outCount    배치된 글자 수
 * @return true면 전부 배치됨, false면 용량 초과로 잘림
 *
 * @details 줄바꿈 규칙 (Greedy):
 *   1. 공백 기준 어절 분리
 *   2. **어절이 2개 이상이면 첫 어절만 줄 0에 두고, 나머지는 줄 1부터** 배치한다.
 *      (길이가 아니라 어절 경계가 줄을 가른다 — "FORTY ONE"(9자)도 2줄이 된다)
 *      단, singleLine=true이면 이 규칙을 건너뛴다 — [수정할 사항 1] 숫자 모드의
 *      "02 H"는 어절이 2개지만 한 줄이 의도다.
 *   3. 어절 자체가 maxPerLine보다 길면 **쪼개지 않고** 배치 실패로 보고한다.
 *      (쪼개면 "THIRTYSEV" 같은 비가독 문자열이 된다)
 *   4. 각 줄은 가로 중앙 정렬: startX = (screenWidth - lineCount * glyphW) / 2
 *   5. 전체는 세로 중앙 정렬: baseY = (LAYOUT_SCREEN_HEIGHT - lines * LAYOUT_LINE_HEIGHT) / 2
 *      → 1줄이면 y=16, 2줄이면 y=0. 결과는 각 글자의 `y` 필드에 담긴다.
 *
 * @note 공백 문자는 줄 경계에서는 버려진다. 같은 줄에 이어지는 어절 사이의 공백만
 *       빈 셀 한 칸(text=" ", 길이 1)으로 배치된다 — 그리는 쪽은 공백 셀을
 *       그리지 않으므로(폴백 폰트도 빈 글리프) 시각적으로 한 칸의 간격이 된다.
 *       복수 공백("A  B")은 한 칸으로 합친다.
 *       "TWENTY SEVEN" → 줄1 "TWENTY", 줄2 "SEVEN" (경계 공백 없음)
 *       "2/10 FRIDAY" → 줄1 "2/10", 줄2 "FRIDAY" (날짜+요일 화면)
 *       "02 H" (singleLine) → 한 줄에 "02", 빈 셀, "H" [수정할 사항 1]
 *
 *       3어절 이상이 한 줄에 이어질 때(실제 시계 문자열에는 없음) 어절 사이에도
 *       공백 셀이 들어가므로 셀 수가 늘어난다 — 그래서 들어가지 못한 어절은
 *       예전(공백 없이 붙여 쓰기)보다 일찍 잘릴 수 있다.
 *
 * @param singleLine true면 "어절 2개 이상 → 무조건 2줄" 규칙을 건너뛴다.
 *                   숫자 모드의 "숫자 단위" 화면 전용. 기본(false)은 기존 규칙.
 */
bool layoutWrap(const char* text, int textLen,
                const CellGeometry& geom,
                LayoutChar* out, int outCapacity, int screenWidth,
                int& outCount, bool singleLine = false);

/**
 * @brief 특정 줄에 배치된 글자 수
 * @param out     layoutWrap() 결과
 * @param count   전체 글자 수
 * @param line    확인할 줄 번호 (0 또는 1)
 */
int layoutLineCount(const LayoutChar* out, int count, uint8_t line);

#endif
