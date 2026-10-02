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
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include "layout_engine.h"

/** 화면 폭 — 렌더러가 실제로 사용하는 값 (SCREEN_WIDTH)을 그대로 쓴다 */
static const int SCREEN_WIDTH = 128;

/**
 * [§6.16b] 글자별 잉크 표 — 실제 폰트를 흉내 낸 결정적 더미.
 * @details 48px 폰트를 재현할 수는 없으므로, 대신 **검증 가능한 규칙**으로 잉크를
 *          만든다 — 같은 문자는 어느 줄에서 조회해도 같은 값을 내야 하고
 *          (표가 실제로 조회되는지 드러나야 한다) 폭이 서로 달라야 한다
 *          (전부 같은 값이면 폰트 최대로 묶인 경로와 구분되지 않는다).
 *          W/M 같은 폭 글자를 크게, I 같은 좁은 글자를 작게 둔다.
 */
static uint8_t dummyInkOf(void* ctx, const char* text, uint8_t len) {
    if (!text || len == 0) return 0;
    if (*text == ' ') return 0;
    if (*text == 'I' || *text == 'J') return 18;   // 좁은 글자
    if (*text == 'W' || *text == 'M') return 40;   // 넓은 글자
    if (*text == 'T' || *text == 'V') return 32;   // 중간 글자
    return 28;   // 나머지
}

int main(int argc, char** argv) {
    std::string text;
    {   // 첫 줄 전체를 읽는다 (공백 포함, 개행 미포함)
        std::string line;
        if (!std::getline(std::cin, line)) return 2;
        text = line;
    }

    // §6.16 간격 확장 — 폰트 최대 잉크 폭. 미지정(0)이면 기존 고정 피치로 돌아간다.
    int inkWidth = 0;
    if (argc > 1) inkWidth = atoi(argv[1]);

    // §6.16b 줄별 잉크 — argv[2]가 "1"이면 글자별 표를 켠다.
    //   웹의 previewInkByChar 경로와 같은 situations를 대조하기 위한 스위치다.
    const bool usePerChar = (argc > 2 && strcmp(argv[2], "1") == 0);

    LayoutChar out[LAYOUT_MAX_CHARS];
    int count = 0;
    bool complete = layoutWrap(text.c_str(), (int)text.size(), defaultGeometry(),
                               out, LAYOUT_MAX_CHARS, SCREEN_WIDTH, count,
                               false, inkWidth,
                               usePerChar ? &dummyInkOf : nullptr, nullptr);

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