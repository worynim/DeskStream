// worynim@gmail.com
/**
 * @file renderer_layout.h
 * @brief 글자 x 좌표 배치를 담당하는 순수 모듈 (Arduino/U8g2 의존성 없음)
 * @details renderer.cpp는 U8g2·LittleFS에 의존해 네이티브 테스트가 불가능하다.
 *          AGENTS.md 테스트 규칙("새 코드에는 새 테스트")을 지키기 위해,
 *          배치 계산처럼 값만 있고 I/O가 없는 부분을 이 TU로 분리했다.
 * @note [SYNC] renderer_geometry.h와 같은 분리 기준 — 기하 판별과 좌표 배치는
 *       서로 다른 책임이므로 한 파일에 묶지 않는다.
 */
#ifndef RENDERER_LAYOUT_H
#define RENDERER_LAYOUT_H

// 배치에 필요한 상수의 **유일한 정의처**다. config.h가 이 헤더를 포함해
// GLYPH_CELL_W / RIGHT_COL_X를 여기서 유도하므로 값이 갈라질 수 없다.
// (config.h는 Arduino.h를 포함하므로 이 헤더는 그것을 포함하지 않는다.)
#define LAYOUT_SCREEN_W    128  // 화면 폭 (config.h의 SCREEN_WIDTH와 같아야 함)
#define LAYOUT_CELL_W      32   // 한 글자의 가로 피치 (한글 32px 래스터)
#define LAYOUT_RIGHT_COL_X 96   // 우측 고정 열의 시작 x
// 한 화면에 나란히 놓을 수 있는 최대 글자 수.
//   128px ÷ 32px 칸 = 4칸이 화면 전체다. HangeulTimeConverter가 만들 수 있는
//   문자열을 전수 확인한 결과 최대가 4자("이십일초")이므로 여유분 없이 4로 잡는다.
//   (과대하게 잡으면 배열이 커질 뿐 아니라, 화면 밖 좌표가 조용히 계산된다.)
//   CharData 배열 크기와 getCharData()의 상한이 **이 값 하나**로 묶여야 한다.
#define LAYOUT_MAX_CHARS 4

/**
 * @brief count개의 글자를 배치했을 때 j번째 글자의 x 좌표를 돌려준다
 * @param count 배치할 글자 수 (1 이상)
 * @param j    구할 글자의 인덱스 (0 ~ count-1)
 * @param centered true면 화면 가운데 정렬, false면 우측 고정 열 기준 배치
 * @details [리뷰 §1.2] 원본은 `if (centered || count == 1)`과 `else` 두 갈래를 탔고,
 *          else 갈래의 마지막 글자를 계산과 무관하게 RIGHT_COL_X로 고정했다.
 *          count==1이면 그 글자가 j==0이면서 동시에 "마지막"이라 가운데 정렬(startX=48)을
 *          무시하고 x=96에 놓였다 — 1자일 때 화면 밖으로 나가는 분기였다.
 *          여기서는 count==1을 가운데 정렬 갈래에 명시적으로 포함시켜 해결한다.
 * @note count >= 2일 때 이 함수의 반환값은 원본과 **동일**하다 (표준 배치 보존).
 */
int layoutCharX(int count, int j, bool centered);

#endif