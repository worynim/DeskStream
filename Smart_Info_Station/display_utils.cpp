#include "display_utils.h"
#include "config.h"
#include "soc/gpio_struct.h"
#include "soc/gpio_reg.h"

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
  u8g2.sendBuffer();
}

/**
 * 🚀 극한의 고속 SW I2C 커스텀 콜백 (Direct Register Access)
 * ESP32-C3 전용 레지스터 제어를 통해 기본 API보다 약 5배 빠른 속도를 제공합니다.
 */
extern "C" uint8_t u8x8_gpio_and_delay_esp32_c3_fast(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT: // 핀 초기화 및 풀업 활성화
      pinMode(u8x8->pins[U8X8_PIN_I2C_CLOCK], OUTPUT_OPEN_DRAIN);
      pinMode(u8x8->pins[U8X8_PIN_I2C_DATA], OUTPUT_OPEN_DRAIN);
      gpio_pullup_en((gpio_num_t)SW_SCL_PIN);
      gpio_pullup_en((gpio_num_t)SW_SDA_PIN);
      break;
    case U8X8_MSG_DELAY_100NANO: break; // 지연 제거
    case U8X8_MSG_DELAY_10MICRO: break; // 지연 제거
    case U8X8_MSG_DELAY_MILLI:   break; // 지연 제거
    case U8X8_MSG_GPIO_I2C_CLOCK:
      if (arg_int) GPIO.out_w1ts.val = (1 << SW_SCL_PIN);
      else         GPIO.out_w1tc.val = (1 << SW_SCL_PIN);
      break;
    case U8X8_MSG_GPIO_I2C_DATA:
      if (arg_int) GPIO.out_w1ts.val = (1 << SW_SDA_PIN);
      else         GPIO.out_w1tc.val = (1 << SW_SDA_PIN);
      break;
    default:
      u8x8_SetGPIOResult(u8x8, 1);
      break;
  }
  return 1;
}
