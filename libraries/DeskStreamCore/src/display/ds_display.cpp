// worynim@gmail.com
/**
 * @file ds_display.cpp
 * @brief 4-OLED 화면 엔진 구현
 * @note 출처: Hangeul_Clock/display_manager.cpp (v2.10.0)
 */
#include "ds_display.h"
#include "ds_sw_i2c_fast.h"
#include "ds_canvas.h"

namespace ds {

// U8G2 생성자의 회전은 begin()에서 cfg.rotation으로 다시 설정한다.
//   화면 0·1은 핀이 의미 없다(byte_cb를 HW 패킷 콜백으로 바꿔 끼운다).
DisplayCore::DisplayCore()
    : _u8g2_0(U8G2_R0, U8X8_PIN_NONE, U8X8_PIN_NONE, U8X8_PIN_NONE),
      _u8g2_1(U8G2_R0, U8X8_PIN_NONE, U8X8_PIN_NONE, U8X8_PIN_NONE),
      _u8g2_2(U8G2_R0, kSwSclPin, kSwSdaPin, U8X8_PIN_NONE),
      _u8g2_3(U8G2_R0, kSwSclPin, kSwSdaPin, U8X8_PIN_NONE) {
    _screens[0] = &_u8g2_0; _screens[1] = &_u8g2_1; _screens[2] = &_u8g2_2; _screens[3] = &_u8g2_3;
}

void DisplayCore::begin(const DisplayConfig& cfg) {
    _cfg = cfg;
    for (int i = 0; i < kNumScreens; i++) {
        _screens[i]->getU8g2()->tile_buf_ptr = _buffers[i];
        _screens[i]->setDisplayRotation(cfg.rotation);
    }

    // 1. HW 버스 + 전송 태스크
    _bus.begin(cfg);

    // 2. 화면 0·1: HW 패킷 콜백 (초기화·명령만 이 경로, 프레임은 HW 태스크가 셰도에서 보낸다)
    _u8g2_0.getU8x8()->byte_cb = ds_u8x8_byte_esp32_idf_0;
    _u8g2_0.begin();
    _u8g2_1.getU8x8()->byte_cb = ds_u8x8_byte_esp32_idf_1;
    _u8g2_1.setI2CAddress(kOledAddr[1] * 2);
    _u8g2_1.begin();

    // 3. 화면 2·3: 고속 SW I2C (타이밍은 첫 begin()의 BYTE_INIT에서 보정된다)
    swI2cFastConfigure(cfg.swHalfBitNs, cfg.swSclSpinMax, cfg.swStretchTimeoutUs);
    _u8g2_2.getU8x8()->gpio_and_delay_cb = ds_u8x8_gpio_and_delay_esp32_c3_fast;
    _u8g2_2.getU8x8()->byte_cb = ds_u8x8_byte_sw_i2c_fast;
    _u8g2_2.begin();
    _u8g2_3.getU8x8()->gpio_and_delay_cb = ds_u8x8_gpio_and_delay_esp32_c3_fast;
    _u8g2_3.getU8x8()->byte_cb = ds_u8x8_byte_sw_i2c_fast;
    _u8g2_3.setI2CAddress(kOledAddr[1] * 2);
    _u8g2_3.begin();

    for (int i = 0; i < kNumScreens; i++) _screens[i]->clearBuffer();
    push();
}

/**
 * @brief 한 화면의 U8g2 버퍼를 셰도 버퍼와 비교해, 바뀐 페이지 구간만 전송 경로에 넘긴다
 * @details HW 버스(0·1)와 SW 버스(2·3)가 **똑같이** 도는 diff다. 전송만 버스마다 다르다.
 *          페이지마다 바뀐 타일의 첫~끝 구간 하나를 보낸다.
 * @note 셰도 버퍼는 **비교 직후** 갱신한다. HW 전송은 태스크가 나중에 하므로,
 *       여기서 갱신하지 않으면 같은 프레임이 두 번 전송된다.
 */
void DisplayCore::diffScreen(uint8_t idx, bool hwBus, bool& anyHwDirty) {
    const uint8_t* buf = _screens[idx]->getBufferPtr();
    uint8_t* shadow = _bus.shadow(idx);
    for (int p = 0; p < kPagesPerScreen; p++) {
        int first_tile = -1, last_tile = -1;
        for (int t = 0; t < kTilesPerPage; t++) {
            bool tile_dirty = false;
            const int base = p * kScreenWidth + t * 8;
            for (int tx = 0; tx < 8; tx++) {
                if (buf[base + tx] != shadow[base + tx]) {
                    tile_dirty = true;
                    shadow[base + tx] = buf[base + tx];
                }
            }
            if (tile_dirty) { if (first_tile == -1) first_tile = t; last_tile = t; }
        }
        if (first_tile == -1) continue;
        const int count = last_tile - first_tile + 1;
        if (hwBus) {
            _bus.preparePageUpdate(idx, p, first_tile, count);   // HW 태스크로 미룬다
            anyHwDirty = true;
        } else {
            _screens[idx]->updateDisplayArea(first_tile, p, count, 1);   // SW는 여기서 바로 보낸다
        }
    }
}

void DisplayCore::push() {
    _bus.waitForSync();

    bool anyHwDirty = false;
    for (int s = 0; s < 2; s++) diffScreen(s, true, anyHwDirty);
    // 새 diff뿐 아니라 **이전 회차에 전송에 실패해 남은 페이지**가 있어도 알린다.
    //   실패한 페이지는 셰도 버퍼가 이미 갱신된 상태라 위 diff가 잡지 못하므로,
    //   이 조건이 없으면 남은 dirty 비트가 다음 글자 변경 때까지 방치된다.
    if (anyHwDirty || _bus.hasPendingHwUpdate()) _bus.notifyTransmission();

    for (int s = 2; s < kNumScreens; s++) diffScreen(s, false, anyHwDirty);
}

void DisplayCore::pushCanvas(const uint8_t* canvas, bool reverseOrder) {
    if (!canvas) return;
    for (int s = 0; s < kNumScreens; s++) {
        const uint8_t column = reverseOrder ? (uint8_t)(kNumScreens - 1 - s) : (uint8_t)s;
        canvasToScreenPages(canvas, kScreenWidth * kNumScreens, column, _buffers[s]);
    }
    push();
}

void DisplayCore::clearAll() {
    for (int i = 0; i < kNumScreens; i++) {
        _screens[i]->clearBuffer();
        _bus.invalidateShadow(i);   // 모든 0픽셀을 강제로 전송하게 한다
    }
    push();
}

void DisplayCore::setBrightness(uint8_t contrast) {
    for (int i = 0; i < kNumScreens; i++) _screens[i]->setContrast(contrast);
}

void DisplayCore::setInversion(bool invert) {
    for (int i = 0; i < kNumScreens; i++) _screens[i]->sendF("c", invert ? 0xA7 : 0xA6);
}

void DisplayCore::setFlip(bool flip) {
    for (int i = 0; i < kNumScreens; i++) _screens[i]->setFlipMode(flip);
}

// ─────────────────────────────────────────────────────────────────────────────
// FPS 벤치마크 — 호출하지 않으면 링커가 제거한다 (--gc-sections)
// ─────────────────────────────────────────────────────────────────────────────

// SW 버스 통계 — 벤치마크 동안만 화면 2·3의 byte_cb를 이 래퍼로 바꿔 끼운다.
static uint64_t g_swBits = 0, g_swUs = 0;
static uint32_t g_swTx = 0, g_swT0 = 0, g_swCur = 0;
static u8x8_msg_cb g_swInner = nullptr;   // 감싸는 실제 SW byte_cb

/** SW I2C byte_cb를 감싸 트랜잭션별 바이트 수·시간을 센다 */
static uint8_t benchSwByteCb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    if (msg == U8X8_MSG_BYTE_START_TRANSFER) { g_swT0 = micros(); g_swCur = 0; }
    else if (msg == U8X8_MSG_BYTE_SEND)      { g_swCur += arg_int; }
    const uint8_t r = g_swInner(u8x8, msg, arg_int, arg_ptr);
    if (msg == U8X8_MSG_BYTE_END_TRANSFER) {
        g_swUs   += (uint32_t)(micros() - g_swT0);
        g_swBits += 2u + 9u * (1u + g_swCur);   // START + (주소+페이로드)×9 + STOP
        g_swTx++;
    }
    return r;
}

/** 한 케이스의 측정 결과 */
struct FpsBenchResult {
    uint32_t frames;
    uint32_t totalUs;           // N프레임 전체 (마지막 HW 전송 완료까지)
    uint64_t hwUs, hwBits;      // HW 버스 (I2C 태스크)
    uint64_t swUs, swBits;      // SW 버스 (메인 루프)
    uint64_t waitUs;            // 메인이 HW 완료를 기다린 시간
    uint32_t hwTx, swTx;
};

/** screenMask에 속한 화면만 매 프레임 전부 바뀌게 그리고 push()를 잰다 (마지막 HW 전송까지 포함) */
static FpsBenchResult fpsBenchCase(DisplayCore& dc, uint8_t screenMask, uint32_t frames) {
    for (int s = 0; s < kNumScreens; s++) dc.screen(s).clearBuffer();
    dc.push();
    dc.waitIdle();

    dc.bus().resetStats();
    g_swBits = 0; g_swUs = 0; g_swTx = 0;

    FpsBenchResult r = {};
    r.frames = frames;
    const uint32_t t0 = micros();
    for (uint32_t f = 0; f < frames; f++) {
        const uint8_t pat = (f & 1u) ? 0x55 : 0xAA;
        for (int s = 0; s < kNumScreens; s++) {
            if (screenMask & (1u << s)) memset(dc.screen(s).getBufferPtr(), pat, kScreenBufBytes);
        }
        dc.push();
    }
    dc.waitIdle();   // 마지막 HW 전송 완료까지
    r.totalUs = (uint32_t)(micros() - t0);

    r.hwUs = dc.bus().statHwUs(); r.hwBits = dc.bus().statHwBits(); r.hwTx = dc.bus().statHwTx();
    r.swUs = g_swUs;              r.swBits = g_swBits;              r.swTx = g_swTx;
    r.waitUs = dc.bus().statWaitUs();
    return r;
}

static float benchKHz(uint64_t bits, uint64_t us) { return us ? (float)bits * 1000.0f / (float)us : 0.0f; }

static void fpsBenchPrint(const char* name, const FpsBenchResult& r) {
    const float n = (float)(r.frames ? r.frames : 1);
    const float msPerFrame = (float)r.totalUs / 1000.0f / n;
    const float fps = r.totalUs ? (1e6f * n / (float)r.totalUs) : 0.0f;
    Serial.printf("[FPS] %s\n", name);
    Serial.printf("[FPS]   %u프레임, 평균 %.2f ms/frame -> %.1f FPS\n", (unsigned)r.frames, msPerFrame, fps);
    Serial.printf("[FPS]   HW 버스: %.2f ms/frame, 실효 SCL %.0f kHz (트랜잭션 %u/frame)\n",
                  (float)r.hwUs / 1000.0f / n, benchKHz(r.hwBits, r.hwUs), (unsigned)(r.hwTx / n));
    Serial.printf("[FPS]   SW 버스: %.2f ms/frame, 실효 SCL %.0f kHz (트랜잭션 %u/frame)\n",
                  (float)r.swUs / 1000.0f / n, benchKHz(r.swBits, r.swUs), (unsigned)(r.swTx / n));
    Serial.printf("[FPS]   메인이 HW 완료를 기다린 시간: %.2f ms/frame\n", (float)r.waitUs / 1000.0f / n);
}

void DisplayCore::runFpsBenchmark(uint32_t frames, uint32_t showMs) {
    Serial.printf("[FPS] 벤치마크 시작 — HW %u Hz(병합 %d), SW 반주기 %u ns(실측 %u), CPU %u MHz\n",
                  (unsigned)_cfg.hwI2cHz, (int)_cfg.hwMergedPageTx, (unsigned)_cfg.swHalfBitNs,
                  (unsigned)swI2cFastMeasuredNs(), (unsigned)getCpuFrequencyMhz());
    const uint32_t swErr0 = swI2cFastErrorCount();

    // SW 버스 통계용 래퍼를 끼운다 (벤치마크가 끝나면 원래 콜백으로 되돌린다)
    const u8x8_msg_cb swCb2 = _u8g2_2.getU8x8()->byte_cb;
    const u8x8_msg_cb swCb3 = _u8g2_3.getU8x8()->byte_cb;
    g_swInner = swCb2;   // 화면 2·3은 같은 콜백을 쓴다
    _u8g2_2.getU8x8()->byte_cb = benchSwByteCb;
    _u8g2_3.getU8x8()->byte_cb = benchSwByteCb;

    const FpsBenchResult a = fpsBenchCase(*this, 0x0F, frames);   // 4화면
    const FpsBenchResult b = fpsBenchCase(*this, 0x01, frames);   // HW 1화면
    const FpsBenchResult c = fpsBenchCase(*this, 0x04, frames);   // SW 1화면

    _u8g2_2.getU8x8()->byte_cb = swCb2;
    _u8g2_3.getU8x8()->byte_cb = swCb3;

    fpsBenchPrint("A) 4화면 풀 프레임 (HW 2 + SW 2 병렬)", a);
    fpsBenchPrint("B) 화면 0만 (HW 1화면)", b);
    fpsBenchPrint("C) 화면 2만 (SW 1화면)", c);
    const uint32_t swErr = swI2cFastErrorCount() - swErr0;
    Serial.printf("[FPS] SW 버스 오류(SCL stuck): %u\n", (unsigned)swErr);

    // 화면에도 띄운다 — USB CDC는 포트를 늦게 열면 부팅 로그를 놓친다
    auto fpsOf = [](const FpsBenchResult& r) { return r.totalUs ? 1e6f * r.frames / r.totalUs : 0.0f; };
    auto msOf  = [](const FpsBenchResult& r) { return r.frames ? r.totalUs / 1000.0f / r.frames : 0.0f; };
    char line[32];
    for (int s = 0; s < kNumScreens; s++) _screens[s]->clearBuffer();
    U8G2* u = _screens[0];
    u->setFont(u8g2_font_6x10_tf);
    u->drawStr(0, 10, "FPS BENCH (HW+SW)");
    snprintf(line, sizeof(line), "4scr %.1fms %.0ffps", msOf(a), fpsOf(a)); u->drawStr(0, 24, line);
    snprintf(line, sizeof(line), "HW1 %.1fms %.0ffps", msOf(b), fpsOf(b));  u->drawStr(0, 36, line);
    snprintf(line, sizeof(line), "SW1 %.1fms %.0ffps", msOf(c), fpsOf(c));  u->drawStr(0, 48, line);
    snprintf(line, sizeof(line), "HW%.0f SW%.0f e%u",
             benchKHz(a.hwBits, a.hwUs), benchKHz(a.swBits, a.swUs), (unsigned)swErr);    u->drawStr(0, 60, line);
    push();
    waitIdle();
    delay(showMs);

    for (int s = 0; s < kNumScreens; s++) _screens[s]->clearBuffer();
    push();
}

}  // namespace ds
