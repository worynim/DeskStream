// worynim@gmail.com
/**
 * @file layout_engine.h
 * @brief 단일 줄 레이아웃 계산 (Arduino/U8g2 의존성 없음)
 * @details renderer.cpp는 U8g2·LittleFS에 의존해 네이티브 테스트가 불가능하다.
 *          AGENTS.md 테스트 규칙을 지키기 위해 배치 규칙을 순수 모듈로 분리했다.
 *
 * @note [신규] ENG_Clock/layout_engine.h를 **복사하지 않았다.**
 *       ENG판 엔진은 2줄 어절 줄바꿈·피치 사다리·잉크 계산을 위해 만들어졌으나
 *       중국어판에서는 셋 다 불필요하다 (PLAN §6.4):
 *         - 중국어 표현에는 공백이 없다 → 어절 = 전체 문자열 → 줄바꿈 불필요
 *         - CJK는 정사각형 그리드다 → 피치는 항상 glyphW(32)
 *         - 잉크 폭으로 간격을 벌릴 필요가 없다
 *       복사하면 죽은 코드 200줄이 그대로 넘어온다. 아래는 1줄 전용 재설계다.
 */
#ifndef LAYOUT_ENGINE_H
#define LAYOUT_ENGINE_H

#include <stdint.h>
#include "renderer_geometry.h"

/**
 * @brief 한 화면에 배치할 최대 셀 수 (1줄 × 4자)
 * @details 셀 = 글자 1칸. 공백 셀도 1칸을 차지한다.
 *          4 × GLYPH_W(32) = 128 = 화면 폭이므로 최대치에서 정확히 맞는다.
 */
#define LAYOUT_MAX_CHARS 4

/** @brief 화면에 배치되는 최대 줄 수 — 중국어판은 항상 1 */
#define LAYOUT_MAX_LINES 1

/**
 * @brief 줄의 세로 시작 Y 좌표
 * @details 1줄뿐이므로 (64 − 48) / 2 = 8. 이 모듈은 Arduino에 의존하지 않으므로
 *          config.h를 include할 수 없다. renderer.cpp의 static_assert가
 *          LAYOUT_LINE_HEIGHT가 config.h의 LINE_HEIGHT와 같은지 막는다.
 */
#ifndef LAYOUT_LINE_HEIGHT
#define LAYOUT_LINE_HEIGHT 48
#endif

/** @brief 화면 높이 — config.h의 SCREEN_HEIGHT와 같아야 한다 */
#ifndef LAYOUT_SCREEN_HEIGHT
#define LAYOUT_SCREEN_HEIGHT 64
#endif

/**
 * @brief 레이아웃된 셀 1개
 * @note [변경] ENG판에는 `y`(줄 수에 따른 세로 중앙 정렬)가 있었으나,
 *       중국어판은 1줄이므로 y는 상수로 고정된다 — LayoutChar에 넣지 않는다.
 *       세로 위치는 renderer가 LAYOUT_LINE_TOP_Y로 계산한다.
 */
struct LayoutChar {
    const char* text;   // 원본 문자열 내의 부분 문자열 (NUL 종료 아님)
    uint8_t len;        // UTF-8 바이트 길이 (CJK = 3)
    int16_t x;          // 화면 X 좌표
    uint8_t line;       // 항상 0 — 2줄 대비 여유
};

/**
 * @brief 단일 줄 배치
 *
 * @param text        원본 문자열 (UTF-8, NUL 종료)
 * @param textLen     text의 바이트 길이
 * @param geom        사용할 셀 기하 (glyphW, maxPerLine)
 * @param out         결과 배열 (LAYOUT_MAX_CHARS개)
 * @param outCapacity out 배열의 용량
 * @param screenWidth 화면 폭 (기본 128)
 * @param outCount    배치된 셀 수
 * @return true면 전부 배치됨, false면 잘림(너무 많음) 또는 용량 초과
 *
 * @details **배치 규칙**
 *   1. 셀 수 = 글자 수(**공백 포함**). `"13 时"` → 1,3,공백,时 = 4셀.
 *   2. 셀 수가 `geom.maxPerLine`(4)를 넘으면 **잘림으로 보고 false.**
 *      어절 쪼개기가 없다 — "三点零" 같은 반쪽 표현을 만드는 것보다 안 그리는 게 낫다.
 *      (실제 시계 표현은 최장 4자이므로 §3 전수표에서 확인된 값이다)
 *   3. `pitch = geom.glyphW`(32). `startX = (screenWidth − 셀수 × pitch) / 2`.
 *      4자면 startX=0, 3자면 16, 2자면 32, 1자면 48 — 항상 정수다.
 *   4. **문자열 끝 규칙**: 문자열 도중에 NUL을 만나거나, 끝에서 3바이트 한자가
 *      잘려 남아 있으면 그 자리는 **공백 셀 한 칸**으로 소비한다.
 *      깨진 바이트를 글자로 배치하면 U8g2가 래스터 밖을 읽는다.
 *      (ENG판 layoutWrap이 `*stop`에서 멈추던 관용 규칙의 방어적 형태)
 *   5. 용량(`outCapacity`)을 넘으면 그 시점에서 멈추고 false.
 *
 * @note [공백 규칙] **앞 공백은 버리고, 어절 사이 공백은 빈 셀 한 칸이 된다.**
 *       복수 공백("13  时")은 한 칸으로 합친다. 빈 셀은 그리지 않으므로
 *       시각적으로는 한 칸 간격이 된다.
 *       "13 时" → [1][3][빈칸][时] = 4셀 = 128px.
 *
 * @note [규칙 예외] 인자가 7개다. AGENTS.md의 "파라미터 ≤ 5"를 어긴다.
 *       ENG판의 layoutWrap()은 11개였다 — 7개라도 큰 개선이지만 여전히 어긴다.
 *       (out, outCapacity, outCount) 묶음화는 호출부·테스트까지 함께 바꾸는
 *       리팩터라 PLAN §6.4가 지정한 시그니처를 그대로 유지하고 여기서만 기록한다.
 */
bool layoutLine(const char* text, int textLen,
                const CellGeometry& geom,
                LayoutChar* out, int outCapacity, int screenWidth,
                int& outCount);

/**
 * @brief 특정 줄에 배치된 셀 수
 * @param out   layoutLine() 결과
 * @param count 전체 셀 수
 * @param line  확인할 줄 번호 — 중국어판은 0만 유효
 * @details display_manager가 애니메이션 대상 셀을 고를 때 쓴다.
 *          1줄 전용이지만 ENG판 호출부 구조를 유지하려고 남겨 둔다.
 */
int layoutLineCount(const LayoutChar* out, int count, uint8_t line);

#endif
