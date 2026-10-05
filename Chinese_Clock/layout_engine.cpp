// worynim@gmail.com
/**
 * @file layout_engine.cpp
 * @brief 단일 줄 레이아웃 계산 구현
 * @note [신규] ENG판의 어절 2줄 엔진(260 LOC)을 **복사하지 않았다** (PLAN §6.4).
 *       중국어판에 필요한 것은 "고정 피치 + 가로 중앙 정렬 + 공백 한 칸" 뿐이다.
 *       어절 줄바꿈·피치 사다리·잉크 상한은 전부 죽은 코드이므로 옮기지 않았다.
 */
#include "layout_engine.h"
// [참고] utf8_len.h를 쓰지 않는다. utf8CharLen()은 "잘림 방지"가 목적이라
//       멀티바이트가 잘리면 길이를 줄여 돌려준다(돌려받은 값으로 그리는 것이
//       오히려 위험). 이 모듈은 **깨진 입력을 그리지 않기로** 했으므로
//       declaredLenOf()로 원래 길이를 직접 판정한다 (PLAN §6.4 규칙 4).

namespace {

/** 빈 셀이 가리키는 문자열 — 원본 문자열 밖의 리터럴이므로 수명이 끊기지 않는다 */
const char BLANK_CELL[] = " ";

/** @brief 한 셀의 종류 */
enum CellKind {
    CELL_GLYPH,    // 그릴 글자
    CELL_SPACE,    // 공백 — 뒤에 글자가 올 때만 한 칸이 된다
    CELL_BROKEN,   // 깨진 바이트 — **무조건** 한 칸을 비운다
    CELL_END       // 문자열 끝
};

/**
 * @brief 리딩 바이트가 요구하는 전체 길이 (연속 바이트 검증 없음 — 범위만 판정)
 * @return 1~4. 리딩 바이트로 쓸 수 없는 값(0x80~0xBF 컨티뉴레이션, 0xF8~0xFF)이면 0
 */
int declaredLenOf(unsigned char lead) {
    if (lead < 0x80) return 1;          // ASCII
    if (lead < 0xC0) return 0;          // 0x80~0xBF — 앞 문자의 꼬리, 리딩으로 오면 깨진 입력
    if (lead < 0xE0) return 2;          // 0xC0~0xDF
    if (lead < 0xF0) return 3;          // 0xE0~0xEF — 한자가 여기
    if (lead < 0xF8) return 4;
    return 0;                           // 0xF8~0xFF — 정의되지 않은
}

/**
 * @brief 한 셀을 판정한다
 * @param p        현재 위치 (리딩 바이트를 가리켜야 함)
 * @param remaining p부터 끝까지 남은 바이트 수
 * @return 셀 종류와 소비할 바이트 수. 문자열 끝이면 바이트 수가 0
 *
 * @details **깨진 입력을 빈 칸 한 칸으로 바꾼다** (PLAN §6.4 규칙 4).
 *          utf8CharLen()은 안전을 위해 길이를 잘라 주지만(반환값 0 금지 규칙),
 *          그러면 깨진 바이트가 **글자 셀**로 배치되어 U8g2가 래스터 밖을 읽는다.
 *          세 경우를 모두 CELL_BROKEN으로 내보낸다:
 *            - 문자열 **도중**의 NUL — 길이 1짜리 셀로 세면 문자열 끝을 가리킨다.
 *            - 컨티뉴레이션 바이트가 리딩으로 등장 — declaredLenOf가 0을 준다.
 *            - 멀티바이트 잘림(리딩 0xE0인데 1~2바이트만 남음).
 *
 * @note **잘린 멀티바이트는 남은 바이트를 한꺼번에 소비한다.** 규칙 4는 "그 문자를
 *       공백 **1칸**으로"라 했으므로, 3바이트 한자가 2바이트만 남았으면 2칸이 아니라
 *       1칸이어야 한다. 바이트별로 따로 세면 좌표가 1칸씩 밀린다.
 *
 * @note **공백과 깨진 바이트를 구분한다.** 공백은 "다음 글자가 올 때만 한 칸"이 되는
 *       대기 상태로 두어 줄 끝의 공백이 Phantom 칸이 되지 않게 한다. 반면 깨진 바이트는
 *       **입력에 존재하는 칸**이므로 즉시 빈 칸을 낸다 — 그러지 않으면 규칙 4와 어긋난다.
 *
 * @note **`textLen` 경계의 NUL은 문자열 끝으로 본다**(CELL_END). 호출자가 준 길이가
 *       권위다 — 그 NUL은 C 문자열의 종료자일 뿐 입력 칸이 아니다. 도중의 NUL과 다르다.
 */
struct CellStep {
    CellKind kind;
    int bytes;
};

CellStep nextCell(const char* p, int remaining) {
    if (remaining <= 0) return { CELL_END, 0 };

    const unsigned char c = (unsigned char)*p;
    if (c == ' ')  return { CELL_SPACE, 1 };
    if (c == 0x00) {
        // 종료자는 입력 칸이 아니다. 도중에 나온 NUL만 깨진 셀로 센다.
        return (remaining == 1) ? CellStep{ CELL_END, 0 } : CellStep{ CELL_BROKEN, 1 };
    }

    const int declared = declaredLenOf(c);
    if (declared == 0)   return { CELL_BROKEN, 1 };   // 컨티뉴레이션 — 1바이트만 소비
    if (declared > remaining) return { CELL_BROKEN, remaining };   // 잘림 — 덩어리로 소비

    return { CELL_GLYPH, declared };
}

/**
 * @brief 셀 하나를 결과 배열에 추가한다
 * @param placed 현재까지 배치된 셀 수 (증가시키며 반환)
 * @return 용량이 부족하면 false
 * @details x는 여기서 정하지 않는다 — 좌표는 전체 셀 수를 알아야 계산되므로
 *          layoutLine()이 마지막에 한 번에 채운다(두 번 순회하지 않기 위함).
 */
bool pushCell(LayoutChar* out, int& placed, int outCapacity, const char* text, uint8_t len) {
    if (placed >= outCapacity) return false;
    out[placed].text = text;
    out[placed].len  = len;
    out[placed].x    = 0;
    out[placed].line = 0;
    placed++;
    return true;
}

/**
 * @brief 셀 하나를 결과 배열에 추가한다
 *
 * @param pendingBlank 어절 사이에 공백이 대기 중인지 (참조로 전달 — 지우면 대기 해제)
 * @param asBlank      true면 원본을 가리키지 않고 빈 칸 리터럴을 쓴다 (깨진 바이트)
 * @return 용량 초과면 false
 *
 * @details 대기 중인 공백을 먼저 비운 뒤 이번 셀을 넣는다 — 깨진 칸 앞의 공백도
 *          어절 사이 공백과 마찬가지로 한 칸이므로, 글자 셀과 빈 칸이 같은 경로를 탄다.
 */
bool placeCell(LayoutChar* out, int& placed, int outCapacity,
               const char* text, uint8_t len, bool& pendingBlank, bool asBlank) {
    if (pendingBlank && !pushCell(out, placed, outCapacity, BLANK_CELL, 1)) return false;
    pendingBlank = false;
    // 깨진 바이트는 소비한 바이트 수가 2 이상일 수 있지만(잘린 멀티바이트 덩어리),
    // **그리는 셀**은 언제나 공백 1바이트다 — len은 글자 셀에서만 원본 길이를 쓴다.
    if (asBlank) return pushCell(out, placed, outCapacity, BLANK_CELL, 1);
    return pushCell(out, placed, outCapacity, text, len);
}

/**
 * @brief 문자열을 한 번 순회하며 셀을 뽑아낸다
 * @return 배치된 셀 수. 용량 초과면 -1
 * @details **공백 규칙**: 첫 어절 앞 공백은 버리고, 어절 사이 공백은
 *          빈 칸 한 칸이 된다. 복수 공백("13  时")은 한 칸으로 합쳐진다.
 *          pendingBlank는 "공백을 봤다"는 사실만 기억하고, 다음 글자가 실제로
 *          나올 때 한 칸을 낸다 — 그래야 줄 끝의 공백이 Phantom 칸이 되지 않는다.
 *          깨진 바이트(CELL_BROKEN)는 대기하지 않고 **즉시** 한 칸을 낸다.
 */
int scanCells(const char* text, int textLen, LayoutChar* out, int outCapacity) {
    int placed = 0;
    bool pendingBlank = false;
    for (int i = 0; i < textLen; ) {
        const CellStep step = nextCell(text + i, textLen - i);
        if (step.kind == CELL_END) break;

        if (step.kind == CELL_SPACE) {
            if (placed > 0) pendingBlank = true;   // 첫 어절 앞 공백은 버린다
        } else {
            const bool asBlank = (step.kind == CELL_BROKEN);
            if (!placeCell(out, placed, outCapacity, text + i, (uint8_t)step.bytes,
                           pendingBlank, asBlank)) return -1;
        }
        i += step.bytes;
    }
    return placed;
}

} // namespace

bool layoutLine(const char* text, int textLen,
                const CellGeometry& geom,
                LayoutChar* out, int outCapacity, int screenWidth,
                int& outCount) {
    outCount = 0;
    if (!out || outCapacity <= 0) return false;
    if (geom.glyphW == 0 || geom.maxPerLine == 0) return false;   // 기하 미확정
    if (!text || textLen <= 0) return true;                       // 빈 문자열 = 빈 화면

    const int placed = scanCells(text, textLen, out, outCapacity);
    if (placed < 0) return false;                    // 용량 초과
    if (placed > geom.maxPerLine) return false;      // 잘림 — 반쪽 표현을 만들지 않는다

    // 가로 중앙 정렬. 4자면 startX=0, 3자면 16, 2자면 32, 1자면 48 — 항상 정수다.
    const int pitch = geom.glyphW;
    const int startX = (screenWidth - placed * pitch) / 2;
    for (int i = 0; i < placed; i++) {
        out[i].x = (int16_t)(startX + i * pitch);
    }

    outCount = placed;
    return true;
}

int layoutLineCount(const LayoutChar* out, int count, uint8_t line) {
    if (!out || count <= 0) return 0;
    int n = 0;
    for (int i = 0; i < count; i++) {
        if (out[i].line == line) n++;
    }
    return n;
}
