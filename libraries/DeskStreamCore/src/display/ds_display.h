// worynim@gmail.com
/**
 * @file ds_display.h
 * @brief 4-OLED 화면 엔진 (DisplayCore)
 * @details U8G2 4개(화면별 개별 1KB 버퍼)를 만들고, push()가 셰도 버퍼와 diff한 페이지 구간만
 *          전송한다. 화면 0·1은 HW I2C 태스크가, 화면 2·3은 호출한 태스크가 고속 SW I2C로
 *          **동시에** 보낸다.
 *
 *          사용 예:
 *          @code
 *          ds::DisplayCore disp;
 *          disp.begin();                       // 기본값 = 1.1MHz·병합 전송·SW 100ns
 *          disp.screen(0).drawStr(0, 10, "hi");
 *          disp.push();
 *          @endcode
 * @note 출처: Hangeul_Clock/display_manager.cpp 의 begin() 배선·diffAndForEachPage·pushParallel·
 *       clearAll·runFpsBenchmark (v2.10.0)
 */
#ifndef DS_DISPLAY_H
#define DS_DISPLAY_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "../ds_board.h"
#include "ds_display_config.h"
#include "ds_i2c_platform.h"

namespace ds {

class DisplayCore {
public:
    DisplayCore();

    /** I2C 버스·HW 태스크·U8G2 4개를 초기화하고 화면을 지운다 */
    void begin(const DisplayConfig& cfg = DisplayConfig());

    U8G2& screen(uint8_t idx) { return *_screens[idx]; }
    /** U8G2* 배열 (렌더러 등이 인덱스로 접근할 때) */
    U8G2** screens() { return _screens; }

    /**
     * @brief 바뀐 페이지만 4화면에 전송한다 (HW 0·1은 태스크로, SW 2·3은 이 태스크에서 병렬)
     * @details 직전 HW 전송이 끝날 때까지 먼저 기다린다. 그래서 반환 직후 버퍼를 다시 그려도 된다.
     *          셰도를 우회하는 `sendBuffer()`를 직접 부르면 셰도와 화면이 어긋난다 — 항상 이 함수를 쓴다.
     */
    void push();

    /**
     * @brief 512x64 한 장 캔버스(행 우선·MSB 먼저 1bpp)를 4화면 버퍼로 전치한 뒤 push()한다
     * @param canvas       LovyanGFX 1bit 스프라이트 버퍼 등 (ds_canvas.h 형식, 폭 512)
     * @param reverseOrder true면 화면 0에 캔버스 오른쪽 끝 열을 보낸다 (180° 장착 시 화면 순서 뒤집기)
     * @details 전치 결과를 U8g2 버퍼에 **직접** 쓰므로 소프트웨어 회전(DisplayConfig::rotation)은
     *          적용되지 않는다. 방향은 setFlip()·패널 명령으로 맞춘다.
     */
    void pushCanvas(const uint8_t* canvas, bool reverseOrder = false);

    /** 4화면 버퍼를 지우고 셰도를 무효화해 **전 화면을 강제 전송**한다 (잔상 제거) */
    void clearAll();

    /** 한 화면을 다음 push()에서 통째로 다시 보내게 한다 */
    void invalidate(uint8_t idx) { _bus.invalidateShadow(idx); }

    /** 진행 중인 HW 전송이 끝날 때까지 기다린다 */
    void waitIdle() { _bus.waitForSync(); }

    void setBrightness(uint8_t contrast);
    void setInversion(bool invert);   ///< SSD1306 0xA7(반전) / 0xA6(정상)

    /**
     * @brief 하드웨어 flip 명령 (setFlipMode)
     * @details U8g2 초기화가 패널을 0xa1/0xc8(두 축 미러)로 두고 U8G2_R2가 버퍼를 180° 돌려
     *          둘이 상쇄되므로 **false가 정방향**이다. true면 초기 미러가 취소되어 180° 뒤집힌다.
     */
    void setFlip(bool flip);

    /**
     * @brief 풀 프레임 전송 FPS를 측정해 시리얼과 화면 0에 보고한다
     * @details 화면 내용을 덮어쓰므로 부팅 중에만 부른다. 0xAA/0x55를 번갈아 채워 모든 바이트가
     *          바뀌게 해 풀 프레임 전송을 강제한다.
     *            · A: 4화면 (HW 2 + SW 2 병렬)  · B: 화면 0만 (HW)  · C: 화면 2만 (SW)
     */
    void runFpsBenchmark(uint32_t frames = 50, uint32_t showMs = 8000);

    I2CPlatform& bus() { return _bus; }

private:
    void diffScreen(uint8_t idx, bool hwBus, bool& anyHwDirty);

    DisplayConfig _cfg;
    I2CPlatform _bus;
    U8G2_SSD1306_128X64_NONAME_F_SW_I2C _u8g2_0, _u8g2_1, _u8g2_2, _u8g2_3;
    U8G2* _screens[kNumScreens];

    /**
     * U8g2 F(풀 버퍼) 생성자는 인스턴스끼리 **같은 정적 1KB 버퍼**를 공유한다.
     * 화면마다 따로 그려 두려면 tile_buf_ptr를 개별 버퍼로 바꿔 끼워야 한다.
     */
    uint8_t _buffers[kNumScreens][kScreenBufBytes];
};

}  // namespace ds

#endif
