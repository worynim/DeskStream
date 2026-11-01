// worynim@gmail.com
/**
 * @file renderer_layout.cpp
 * @brief 글자 x 좌표 배치 구현
 */
#include "renderer_layout.h"
#include <string.h>   // strlen/memcmp — Arduino String 없이 접미사 판정

// 단위 "일"의 UTF-8 바이트 표현. 접미사 비교는 String(Arduino) 없이
// memcmp로 해야 이 모듈이 호스트(g++)에서 그대로 테스트된다.
static const char DAY_UNIT_UTF8[] = "일";

/**
 * @brief 이 텍스트가 날짜("...일")인가
 * @details 판정 근거는 "마지막 글자가 단위 '일'"이다. 제목 화면에 올 수 있는 텍스트 중
 *          "일"로 끝나는 것은 날짜(= getDay()/getNumericDay())뿐이다 — 자세한 이유와
 *          주의사항은 renderer_layout.h의 선언 주석을 참조.
 */
bool layoutIsDayText(const char* text) {
    if (!text) return false;
    const size_t textLen = strlen(text);
    const size_t unitLen = sizeof(DAY_UNIT_UTF8) - 1;   // 널 문자 제외 = 3
    if (textLen < unitLen) return false;
    return memcmp(text + (textLen - unitLen), DAY_UNIT_UTF8, unitLen) == 0;
}

int layoutCharX(int count, int j, bool centered) {
    if (count <= 0) return 0;   // 방어선: 호출자가 count를 0으로 넘기는 경우

    // count == 1은 명시적으로 가운데 정렬 갈래에 넣는다.
    //   원본은 `if (centered || count == 1)`였는데, else 갈래의 "마지막 글자 = RIGHT_COL_X"
    //   규칙이 j==0인 이 글자에 그대로 적용돼 x=96(화면 오른쪽 끝)에 놓였다.
    //   [리뷰 §1.2] 1자일 때 화면 밖으로 나가는 분기였다.
    if (centered || count == 1) {
        return (LAYOUT_SCREEN_W - count * LAYOUT_CELL_W) / 2 + j * LAYOUT_CELL_W;
    }
    const int startX = (LAYOUT_RIGHT_COL_X - (count - 1) * LAYOUT_CELL_W) / 2;
    return (j == count - 1) ? LAYOUT_RIGHT_COL_X : startX + j * LAYOUT_CELL_W;
}