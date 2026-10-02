// worynim@gmail.com
/**
 * @file layout_dump.cpp
 * @brief layout_engine의 layoutWrap()를 표준 입력으로 받아 `line,x,...|상태`를 출력한다
 * @details layout_crosscheck.mjs가 이 출력을 JS 결과와 비교한다.
 *
 * @note 화면 폭과 기하는 헤더에서 읽는다 (하드코딩하면 펌웨어 값이 바뀔 때
 *       테스트가 조용히 옛 값과 비교해 통과해 버린다).
 */
#include <cstdio>
#include <iostream>
#include <string>
#include "layout_engine.h"

/** 화면 폭 — 렌더러가 실제로 사용하는 값 (SCREEN_WIDTH)을 그대로 쓴다 */
static const int SCREEN_WIDTH = 128;

int main() {
    std::string text;
    {   // 첫 줄 전체를 읽는다 (공백 포함, 개행 미포함)
        std::string line;
        if (!std::getline(std::cin, line)) return 2;
        text = line;
    }

    LayoutChar out[LAYOUT_MAX_CHARS];
    int count = 0;
    bool complete = layoutWrap(text.c_str(), (int)text.size(), defaultGeometry(),
                               out, LAYOUT_MAX_CHARS, SCREEN_WIDTH, count);

    std::string result;
    for (int i = 0; i < count; i++) {
        result += std::to_string((int)out[i].line);
        result += ",";
        result += std::to_string((int)out[i].x);
        result += ",";
        result += std::to_string((int)out[i].y);   // 세로 중앙 정렬까지 대조한다
        result += ",";
    }
    result += complete ? "|OK" : "|TRUNC";
    printf("%s\n", result.c_str());
    return 0;
}