// worynim@gmail.com
/**
 * @file renderer_layout.cpp
 * @brief 글자 x 좌표 배치 구현
 */
#include "renderer_layout.h"

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