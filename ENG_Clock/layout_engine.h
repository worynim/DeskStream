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
 * @brief [사용자 지정 사다리] n 글자 줄의 총 폭을 (n+1) 글자분으로 맞춘다
 * @details 9자 → 126(변화 없음) · 8자 → 9자폭 · 7자 → 8자폭 … 5자 이하를 16~21px로 눌러 준다.
 *          잉크는 이 산식의 목표가 아니라 겹침·잘림을 막는 제한으로만 쓰인다.
 */
#define PITCH_SUM_OFFSET 1

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
 * @brief 한 글자의 잉크 폭을 조회하는 함수 포인터 (줄별 상한 계산용)
 * @param ctx     호출자가 정한 컨텍스트 (예: Renderer*)
 * @param text    글자의 UTF-8 시작 포인터
 * @param len     바이트 길이
 * @return 잉크 폭(픽셀). 모르면 0을 돌려도 되고 — 그 경우 linePitch가 폰트 최대로 대체한다
 *
 * @details [§6.16b] layoutWrap()이 **그 줄에 실제로 있는 글자들만** 재서 상한을 걸려면
 *          글자별 잉크가 필요하다. 캐시를 직접 들여다보면 순수성이 깨지고(JS 미러와
 *          대칭을 잃는다), 조회 함수를 주입하면 layoutWrap은 계속 순수 함수로 남는다.
 *          잉크는 캐시에만 있고 화면 좌표 계산은 어디에서도 가능해야 하므로 이쪽이 맞다.
 */
typedef uint8_t (*InkWidthFn)(void* ctx, const char* text, uint8_t len);

/**
 * @brief 한 줄의 가로 피치(셀 간격) — **사용자 지정 사다리**
 *
 * @param charCount   이 줄에 놓일 셀 수 (공백 셀 포함)
 * @param geom        사용할 셀 기하 (glyphW, maxPerLine)
 * @param screenWidth 화면 폭
 * @param lineInk     **이 줄**의 최대 잉크 폭. 0이면 알 수 없음 → fontInk로 대체
 * @param fontInk     폰트 전체 최대 잉크 폭. 0이면 캐시 없음(확장 없음)
 * @param lineFloor   이 줄의 겹침 없는 최소 피치. 0이면 알 수 없음 → lineInk로 대체
 * @return 셀 간격(픽셀). 항상 glyphW 이상
 *
 * @details **규칙: n 글자 줄의 총 폭을 (n+1) 글자분으로 맞춘다.**
 *
 *   | n | 총폭  | pitch |        | n | 총폭 | pitch |
 *   |---|-------|-------|        |---|------|-------|
 *   | 9 |  126  |  14   |        | 5 |  84  |  16   |
 *   | 8 |  126  |  15   |        | 4 |  70  |  17   |
 *   | 7 |  112  |  16   |        | 3 |  56  |  18   |
 *   | 6 |   98  |  16   |        | 2 |  42  |  21   |
 *
 *   - 9자는 요구대로 **변경 없다**(126 = 9 × glyphW).
 *   - 8자는 9자와 같은 폭, 7자는 8자폭, … 으로 한 글자씩 좁혀진다.
 *   - 5자 이하는 25·31·42·63까지 치솟던 것을 16·17·18·21로 눌러 준다.
 *
 * @details **잉크는 목표가 아니라 두 개의 제한으로만 쓴다.**
 *          measureLineFloor()는 "이만큼은 벌려야 안 겹친다"는 **하한**이지
 *          목표가 아니다. 한때 완화를 이 값 쪽으로 되돌려 간격을 **경계에 딱 붙였는데**
 *          그게 "AM·ONE·FORTY가 너무 붙어"라는 보고의 원인이었다(실제로 배포됨).
 *          잉크는 여기서 딱 두 가지만 막는다 — 겹침(아래로)과 화면 밖 잘림(위로).
 *
 *          잉크가 래스터 안에서 중앙 정렬되므로 두 글자 사이의 간격은
 *          `pitch − (inkA + inkB) / 2`이고, 겹치지 않으려면 `pitch ≥ measureLineFloor()`.
 */
int linePitch(int charCount, const CellGeometry& geom, int screenWidth,
              int lineInk, int fontInk, int lineFloor = 0);

/**
 * @brief 한 줄의 첫 셀 x 좌표
 *
 * @param charCount 이 줄의 셀 수
 * @param pitch     linePitch()가 준 간격
 * @param geom      셀 기하
 * @param screenWidth 화면 폭
 * @param lineInk   **이 줄**의 최대 잉크 폭 (§6.16b)
 * @return 첫 셀의 x
 *
 * @details 피치가 glyphW와 같으면(9자 · 작은 폰트) **기존과 같은 셀 중앙 정렬**을 쓴다
 *          — 이 경로의 좌표는 한 픽셀도 바뀌지 않는다.
 *          피치가 넓어졌을 때만 잉크 블록 폭 `((charCount-1) × pitch + lineInk)`을
 *          화면 중앙에 둔다. 셀 폭(14) 기준으로 중앙 정렬하면 피치가 커질수록
 *          줄이 `(pitch - glyphW) / 2`만큼 왼쪽으로 쏠려 "AM"이 화면 왼쪽에 붙는다.
 */
int lineStartX(int charCount, int pitch, const CellGeometry& geom,
               int screenWidth, int lineInk);

/**
 * @brief [§6.16b] 한 줄의 최대 잉크 폭을 잰다
 *
 * @param text   원본 문자열
 * @param start  이 줄의 첫 글자 시작 위치 (바이트 오프셋)
 * @param count  이 줄에 놓일 셀 수 (공백 셀 포함)
 * @param inkFn  글자별 잉크 조회 (nullptr이면 0)
 * @param ctx    inkFn의 컨텍스트
 * @param fallback 조회 실패·공백 셀로 잉크를 못 얻을 때 대체할 값 (보통 폰트 최대)
 * @return 이 줄의 최대 잉크 폭
 *
 * @details 글자마다 잉크가 다르면 **이 줄의 최대**가 간격 상한이 된다.
 *          "TWO"는 W 때문에 32, "SEVEN"은 28 — 둘 다 48px 폰트다.
 */
int measureLineInk(const char* text, int start, int count,
                   InkWidthFn inkFn, void* ctx, int fallback);

/**
 * @brief [§6.16c] 이 줄에서 **글자가 겹치지 않는 최소 피치**를 잰다
 *
 * @param text   원본 문자열
 * @param start  이 줄의 첫 글자 시작 위치 (바이트 오프셋)
 * @param count  이 줄에 놓일 셀 수 (공백 셀 포함)
 * @param inkFn  글자별 잉크 조회 (nullptr이면 0)
 * @param ctx    inkFn의 컨텍스트
 * @param fallback 조회 실패 시 대체할 값 (보통 폰트 최대)
 * @return 인접한 두 글자의 잉크 평균 중 최댓값 (= 필요한 최소 피치)
 *
 * @details 잉크는 **래스터 안에서 중앙 정렬**되어 있으므로(글자 폭만큼 좌우 여백이
 *          같게 붙는다) 두 글자 사이의 빈틈은 `피치 − (inkA + inkB) / 2`다.
 *          따라서 겹치지 않으려면 `피치 ≥ (inkA + inkB) / 2`여야 하고, 줄 전체의
 *          하한은 **인접 쌍들의 최댓값**이다.
 *
 *          줄 최대 잉크(width of widest single glyph)를 하한으로 쓰면 과하다 —
 *          "TWO"는 줄 최대 38이지만 T(20)+W(38)→29, W(38)+O(32)→35이므로 하한은 35다.
 *          38을 쓰면 "SEVEN"(하한 24)처럼 여유가 큰 줄이 한 번도 줄어들지 않는다.
 */
int measureLineFloor(const char* text, int start, int count,
                     InkWidthFn inkFn, void* ctx, int fallback);

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
 *   4. 각 줄은 가로 중앙 정렬 — 간격과 시작 x는 linePitch()/lineStartX()가 계산한다
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
 * @param inkWidth   폰트 최대 잉크 폭. 0(기본)이면 간격을 늘리지 않는다 —
 *                   캐시 미적용 상태에서는 잉크를 알 수 없으므로 기존 동작을 유지한다.
 * @param inkFn      [§6.16b] 글자별 잉크 조회. nullptr이면 inkWidth(폰트 최대)만 쓴다.
 * @param inkCtx     inkFn의 컨텍스트
 *
 * @note [§6.16b] inkFn이 주어지면 **줄마다** 그 줄의 실제 최대 잉크로 간격을 정한다.
 *       어절 배정(1단계)이 끝난 뒤 줄별로 재야 "이 줄에 없는 글자"가 상한에 끼지 않는다.
 *
 * @note [규칙 예외] 인자가 11개다. AGENTS.md의 "파라미터 ≤ 5"는 이 함수가 이미
 *       7개로 어기고 있었다. 파라미터 묶음(옵션 구조체)化는 호출부·테스트까지
 *       함께 바꾸는 큰 리팩터라 별도 작업으로 분리하고, 새 로직은 linePitch()/
 *       lineStartX()/measureLineInk()로 빼내 이 함수의 본체는 늘리지 않는다.
 */
bool layoutWrap(const char* text, int textLen,
                const CellGeometry& geom,
                LayoutChar* out, int outCapacity, int screenWidth,
                int& outCount, bool singleLine = false, int inkWidth = 0,
                InkWidthFn inkFn = nullptr, void* inkCtx = nullptr);

/**
 * @brief 특정 줄에 배치된 글자 수
 * @param out     layoutWrap() 결과
 * @param count   전체 글자 수
 * @param line    확인할 줄 번호 (0 또는 1)
 */
int layoutLineCount(const LayoutChar* out, int count, uint8_t line);

#endif
