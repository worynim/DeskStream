// worynim@gmail.com
/**
 * @file ds_canvas.h
 * @brief 512x64 한 장 캔버스 → 화면별 SSD1306 페이지 버퍼 전치 (순수 함수 — Arduino 의존 없음)
 * @details 캔버스 형식: 1bpp, **행 우선 · MSB 먼저** — LovyanGFX `LGFX_Sprite`(setColorDepth(1))의
 *          버퍼와 Game_Of_Life 엔진 버퍼가 이 형식이다. 바이트 (y * canvasWidth/8 + x/8)의 비트 (7 - x%8)가 픽셀 (x, y).
 *          SSD1306 페이지 형식: 바이트 (page * 128 + x)의 비트 b가 픽셀 (x, page*8 + b).
 *          호스트 단위 테스트 대상(test/test_canvas.cpp).
 * @note 출처: Sound_Visualizer_4_OLEDs/DisplayEngine.cpp extractAndCheckDirty, Game_Of_Life mapEngineToBuffers
 */
#ifndef DS_CANVAS_H
#define DS_CANVAS_H

#include <stdint.h>

namespace ds {

/**
 * @brief 캔버스의 column번째 128px 열(= 논리 화면)을 페이지 형식 1024바이트로 전치한다
 * @param canvas      행 우선·MSB 먼저 1bpp 캔버스 (높이 64)
 * @param canvasWidth 캔버스 폭 (px, 8의 배수 — 보통 512)
 * @param column      0부터 시작하는 128px 열 번호 (x 오프셋 = column * 128)
 * @param out         출력 1024바이트 (U8g2 풀 버퍼와 같은 배치)
 */
void canvasToScreenPages(const uint8_t* canvas, uint16_t canvasWidth, uint8_t column, uint8_t* out);

}  // namespace ds

#endif
