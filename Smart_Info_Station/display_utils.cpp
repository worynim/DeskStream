// worynim@gmail.com
/**
 * @file display_utils.cpp
 * @brief 공통 디스플레이 헬퍼 및 드로잉 유틸리티 구현
 * @details 텍스트 정렬, 진행 바 드로잉, 시스템 아이콘 렌더링 등 공통 UI 요소 처리
 */
#include "display_utils.h"
#include "config.h"

bool containsKorean(String s) {
  for (int i = 0; i < s.length(); i++) {
    if (s[i] & 0x80) return true;
  }
  return false;
}

void drawCenteredText(U8G2 &u8g2, String text, int y) {
  const char *str = text.c_str();
  int textWidth = u8g2.getUTF8Width(str);
  int x = (u8g2.getDisplayWidth() - textWidth) / 2;
  if (x < 0) x = 0;

  if (containsKorean(text)) u8g2.drawUTF8(x, y + 4, str); // 한글 전용 세로 오프셋 보정
  else u8g2.drawStr(x, y, str);
}

void u8g2_prepare(U8G2 &u8g2) {
  u8g2.setBitmapMode(1);           // 투명 비트맵 모드
  u8g2.setDrawColor(1);            // 픽셀 출력 색상
  u8g2.setFontMode(1);             // 투명 폰트 모드 (박스 위에 글자 겹칠 때 필수)
  u8g2.setFontPosTop();            // 좌표 기준을 상단으로 설정
}

void showUpdateMessage(U8G2 &u8g2, String msg) {
  u8g2_prepare(u8g2);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 5, msg.c_str());  // 호출자가 점 애니메이션 포함한 완성된 메시지를 전달
  display.push();
}
