// worynim@gmail.com
/**
 * @file bb_diagnostic.cpp
 * @brief BitBang I2C 하드웨어 진단 — 배선/패널/컨트롤러/코드를 가른다
 * @details **BTN4를 누른 채 부팅**하면 실행된다 (config.h의 BB_DIAG_BUTTON_PIN).
 *
 *          ## v1.0.2에서 고친 진단 자체의 버그 (중요)
 *
 *          진단 3(참조 구현)이 **구조적으로 아무것도 그릴 수 없는 상태**였다.
 *          `bitBang.begin()`은 핀을 Open-Drain + **출력 래치 1**(=릴리스)로 잡는다.
 *          그런데 Multi_BitBang은 `pinMode(OUTPUT)`만 하고 `digitalWrite()`를 하지 않아
 *          **래치 값이 그대로 출력 레벨이 된다** — 래치가 1이면 `SCL_LOW()`가 LOW 대신
 *          HIGH를 민다. 즉 두 비트뱅 구현이 같은 핀을 서로 다른 규약으로 잡아
 *          통신이 성립하지 않았다.
 *
 *          → 교훈: **핀 규약이 다른 두 드라이버를 같은 핀에서 번갈아 쓰면 안 된다.**
 *            쓰기 전에 반드시 그 드라이버의 init을 다시 불러 핀을 다시 잡아야 한다.
 *            이 파일은 단계마다 `claimPinsForMultiBitBang()` / `claimPinsForOwnDriver()`로
 *            소유권을 명시적으로 넘긴다.
 *
 *          ## 시험 순서
 *            진단 1  Multi_BitBang 버스 스캔                → 배선이 살아 있는가
 *            진단 2  내 드라이버 버스별 probe                → ACK 판독이 되는가
 *            진단 3  **컨트롤러 스윕** (SH1106/SSD1315/SSD1306) → 패널이 무엇인가
 *            진단 4  내 4버스 동시 전송 경로                 → 프레임 경로가 맞는가
 */
#include "config.h"

#include <Arduino.h>
#include "bb_diagnostic.h"
#include "bitbang_i2c.h"
#include "i2c_platform.h"   // u8x8_byte_bb_0..3 선언 (우리 U8g2 콜백)
#include "oled_panel.h"
#include "Multi_BitBang.h"

static const uint8_t kSda[NUM_SCREENS] = BB_SDA_PINS;

/** U8g2가 쓰는 콜백 (i2c_platform.cpp에 구현) — 화면 인덱스 = 버스 번호 */
typedef uint8_t (*bb_byte_cb_t)(u8x8_t *, uint8_t, uint8_t, void *);
static bb_byte_cb_t const kBusCb[NUM_SCREENS] = {
    u8x8_byte_bb_0, u8x8_byte_bb_1, u8x8_byte_bb_2, u8x8_byte_bb_3
};

/** 스윕용 프레임 버퍼 — 컨트롤러 종류가 달라도 한 벌만 쓰면 된다(동시에 하나만 활성) */
static uint8_t gSweepBuf[NUM_SCREENS][SCREEN_WIDTH * PAGES_PER_SCREEN];

// ─────────────────────────────────────────────────────────────────────────────
// 핀 소유권 — 두 비트뱅 구현이 같은 핀을 쓰므로 단계마다 명시적으로 넘긴다
// ─────────────────────────────────────────────────────────────────────────────

/** Multi_BitBang 규약으로 핀을 다시 잡는다 (출력 래치를 0으로 되돌리는 것이 핵심) */
static void claimPinsForMultiBitBang() {
    static uint8_t sda[NUM_SCREENS];
    static uint8_t scl[NUM_SCREENS];
    static int32_t clk[NUM_SCREENS];
    for (uint8_t i = 0; i < NUM_SCREENS; i++) {
        sda[i] = kSda[i];
        scl[i] = BB_SCL_PIN;
        clk[i] = BB_I2C_SPEED_HZ;
    }
    Multi_I2CInit(sda, scl, clk, NUM_SCREENS);   // ⚠ 래치 0 + 핀 모드 재설정
}

/** 내 드라이버 규약으로 핀을 잡는다 (Open-Drain + 입력 활성) */
static void claimPinsForOwnDriver() {
    bitBang.begin(kSda, BB_SCL_PIN, NUM_SCREENS, BB_HALF_BIT_NS);
}

// ─────────────────────────────────────────────────────────────────────────────
// 진단 1·2
// ─────────────────────────────────────────────────────────────────────────────

static void diagBusScan() {
    claimPinsForMultiBitBang();
    Serial.println("[진단 1] Multi_BitBang(순차) 버스 스캔 — 배선 확인");
    for (uint8_t b = 0; b < NUM_SCREENS; b++) {
        uint8_t map[16];
        Multi_I2CScan(b, map);
        const bool found = (map[OLED_I2C_ADDR >> 3] & (1u << (OLED_I2C_ADDR & 7))) != 0;
        Serial.printf("        bus %u (SDA %u): 0x%02X %s\n",
                      (unsigned)b, (unsigned)kSda[b], (unsigned)OLED_I2C_ADDR,
                      found ? "OK" : "**NOT FOUND** (배선/전원)");
    }
}

static void diagProbe() {
    claimPinsForOwnDriver();
    Serial.println("[진단 2] 이 펌웨어 드라이버(bitBang) 단독 probe — ACK 판독 확인");
    for (uint8_t b = 0; b < NUM_SCREENS; b++) {
        const uint8_t ok = bitBang.probe(b, OLED_I2C_ADDR);
        Serial.printf("        bus %u (SDA %u): probe %s\n",
                      (unsigned)b, (unsigned)kSda[b], ok ? "ACK" : "**NACK**");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 진단 3 — 컨트롤러 스윕
//
// I2C에는 읽기 경로가 없어 컨트롤러 ID를 되읽을 수 없다. 유일한 방법은
// **후보마다 올바른 초기화 시퀀스로 그려 보고 사람이 고르는 것**이다.
// U8g2의 드라이버를 쓰므로 각 컨트롤러의 초기화와 열 오프셋(+2)이 자동으로 맞는다.
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 한 컨트롤러로 4화면에 식별용 패턴을 그린다
 * @tparam T U8g2 드라이버 클래스
 * @param name 시리얼에 찍을 이름
 * @param colOffset 이 컨트롤러의 열 오프셋 (정보 출력용)
 * @param holdMs 패턴을 유지할 시간
 * @note U8g2의 `sendBuffer()`는 CAD를 거치므로 **제어바이트가 트랜잭션마다 하나**다.
 *       즉 이 경로는 펌웨어의 캡처·재생 경로와 **독립적으로** 검증된다.
 */
template <typename T>
static void sweepTry(const char *name, uint8_t colOffset, uint32_t holdMs) {
    static T d[NUM_SCREENS] = {
        T(U8G2_R2, 255, 255, U8X8_PIN_NONE), T(U8G2_R2, 255, 255, U8X8_PIN_NONE),
        T(U8G2_R2, 255, 255, U8X8_PIN_NONE), T(U8G2_R2, 255, 255, U8X8_PIN_NONE)
    };

    Serial.printf("        > %-8s (열 오프셋 +%u) ...", name, (unsigned)colOffset);

    for (uint8_t i = 0; i < NUM_SCREENS; i++) {
        d[i].getU8g2()->tile_buf_ptr = gSweepBuf[i];
        d[i].getU8x8()->byte_cb = kBusCb[i];   // 우리 4버스 BitBang 콜백
        d[i].begin();                          // <- 컨트롤러별 초기화 시퀀스

        d[i].clearBuffer();
        d[i].drawFrame(0, 0, 128, 64);         // 테두리 — 밀림/감김을 눈으로 본다
        d[i].drawBox(3, 3, 18, 18);            // 좌상단
        d[i].drawBox(107, 43, 18, 18);         // 우하단
        d[i].drawBox(3, 43, 18, 18);           // 좌하단
        d[i].drawBox(107, 3, 18, 18);          // 우상단
        d[i].setFont(u8g2_font_6x10_tf);
        d[i].drawStr(28, 28, name);
        d[i].drawStr(28, 40, "1.3 OLED");
        d[i].sendBuffer();
    }

    Serial.println(" 화면을 보세요");
    delay(holdMs);
}

static void diagControllerSweep() {
    claimPinsForOwnDriver();
    Serial.println("[진단 3] 컨트롤러 스윕 — 제대로 나오는 것을 찾으세요");
    Serial.println("        (각 6초. 화면 4개 모두 같은 패턴이 나와야 정상)");

    // SH1106 계열이 1.3"에서 가장 흔하다
    sweepTry<U8G2_SH1106_128X64_NONAME_F_SW_I2C>("SH1106", 2, 6000);
    sweepTry<U8G2_SH1106_128X64_WINSTAR_F_SW_I2C>("WINSTAR", 2, 6000);
    sweepTry<U8G2_SSD1315_128X64_NONAME_F_SW_I2C>("SSD1315", 0, 6000);
    sweepTry<U8G2_SSD1306_128X64_NONAME_F_SW_I2C>("SSD1306", 0, 6000);

    // 마지막에 다시 SH1106으로 돌려 두어 "아무것도 못 봤다"와 구분되게 한다
    Serial.println("        (확인용으로 SH1106을 다시 한 번 그립니다)");
    sweepTry<U8G2_SH1106_128X64_NONAME_F_SW_I2C>("SH1106", 2, 6000);
}

// ─────────────────────────────────────────────────────────────────────────────
// 진단 4 — 펌웨어가 실제로 쓰는 두 전송 경로를 **같은 내용**으로 그려 비교한다
//
//  A) U8g2 CAD (`sendBuffer`)          — 기준(가장 단순하고 확실)
//  B) 캡처 후 재생 (BB_FRAME_PATH_CONCURRENT) — U8g2가 만든 바이트를 4버스에 동시 전송
//  C) U8g2 CAD 페이지별 (BB_FRAME_PATH_U8G2)  — 버스별 순차
//
// 셋이 같은 그림을 내야 한다. B만 다르면 동시 재생 로직 결함,
// A와 C가 다르면 diff/페이지 경로 결함이다.
// ─────────────────────────────────────────────────────────────────────────────

/** 식별용 내용을 버퍼에 그린다 (테두리 + 모서리 + 이름) */
static void drawIdentContent(OledPanel& d, const char* label) {
    d.clearBuffer();
    d.drawFrame(0, 0, 128, 64);
    d.drawBox(3, 3, 18, 18);
    d.drawBox(107, 43, 18, 18);
    d.drawBox(3, 43, 18, 18);
    d.drawBox(107, 3, 18, 18);
    d.setFont(u8g2_font_6x10_tf);
    d.drawStr(28, 28, label);
    d.drawStr(28, 40, "1.3 OLED");
}

static void diagOwnFramePath() {
    claimPinsForOwnDriver();
    Serial.printf("[진단 4] 두 전송 경로를 같은 내용으로 비교 (%s, 열 오프셋 +%u)\n",
                  OLED_DRIVER_NAME, (unsigned)OLED_COL_OFFSET);

    static OledPanel d[NUM_SCREENS] = {
        OledPanel(U8G2_R2, 255, 255, U8X8_PIN_NONE), OledPanel(U8G2_R2, 255, 255, U8X8_PIN_NONE),
        OledPanel(U8G2_R2, 255, 255, U8X8_PIN_NONE), OledPanel(U8G2_R2, 255, 255, U8X8_PIN_NONE)
    };
    for (uint8_t i = 0; i < NUM_SCREENS; i++) {
        d[i].getU8g2()->tile_buf_ptr = gSweepBuf[i];
        d[i].getU8x8()->byte_cb = kBusCb[i];
        d[i].begin();
    }

    // ── A) 기준: U8g2 CAD로 전체 버퍼 전송 ──
    for (uint8_t i = 0; i < NUM_SCREENS; i++) {
        drawIdentContent(d[i], "A: U8g2");
        d[i].sendBuffer();
    }
    Serial.println("        A) U8g2 기준        — 4화면 모두 반듯하면 정상");
    delay(6000);

    // ── B) 펌웨어의 동시 경로: 캡처 후 4버스 동시 재생 ──
    for (uint8_t i = 0; i < NUM_SCREENS; i++) drawIdentContent(d[i], "B: concurrent");
    for (uint8_t p = 0; p < PAGES_PER_SCREEN; p++) {
        i2cPlatform.captureBegin();
        for (uint8_t i = 0; i < NUM_SCREENS; i++) {
            d[i].updateDisplayArea(0, p, TILES_PER_PAGE, 1);   // 캡처만 된다(전송 안 함)
        }
        i2cPlatform.captureEnd();
        const uint8_t ok = i2cPlatform.replayCaptured(0x0F);
        if (p == 0) {
            Serial.printf("        B) 캡처 후 동시 재생 — replay %s, err=%u\n",
                          ok == 0x0F ? "OK" : "**FAIL**", (unsigned)i2cPlatform.errorCount());
        }
    }
    Serial.println("           A와 똑같아야 정상. 다르면 동시 재생 로직 결함");
    delay(6000);

    // ── C) 펌웨어의 순차 경로: U8g2 CAD 페이지별 ──
    for (uint8_t i = 0; i < NUM_SCREENS; i++) {
        drawIdentContent(d[i], "C: sequential");
        for (uint8_t p = 0; p < PAGES_PER_SCREEN; p++) {
            d[i].updateDisplayArea(0, p, TILES_PER_PAGE, 1);
        }
    }
    Serial.println("        C) 버스별 순차      — B와 똑같아야 정상");
    delay(6000);

    // 정리 — C 경로로 지운다
    for (uint8_t i = 0; i < NUM_SCREENS; i++) {
        d[i].clearBuffer();
        for (uint8_t p = 0; p < PAGES_PER_SCREEN; p++) {
            d[i].updateDisplayArea(0, p, TILES_PER_PAGE, 1);
        }
    }
    Serial.println("        (화면 지움)");
}

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 부팅 시 BTN4가 눌려 있으면 진단 모드로 들어간다
 * @details `BB_DIAG_HOLD_MS` 동안 계속 눌려 있어야 한다 — 기계 접점 채터링과
 *          우연한 접촉을 배제하기 위함이다. 진단을 원하지 않으면 이 함수는
 *          수 ms 안에 false로 빠져나가므로 부팅이 느려지지 않는다.
 */
bool bbDiagnosticRequested() {
    if (digitalRead(BB_DIAG_BUTTON_PIN) != LOW) return false;   // 눌리지 않았다

    const uint32_t t0 = millis();
    while (digitalRead(BB_DIAG_BUTTON_PIN) == LOW) {
        if (millis() - t0 >= BB_DIAG_HOLD_MS) {
            Serial.println("[DIAG] BTN4 hold detected -> diagnostic mode");
            return true;
        }
        delay(10);
    }
    Serial.println("[DIAG] BTN4 released early -> normal boot");
    return false;
}

void runBitBangDiagnostics() {
    Serial.begin(115200);
    delay(600);

    Serial.println();
    Serial.println("==================================================");
    Serial.println(" Hangeul_Clock_bitbang_i2c — BitBang 진단 모드");
    Serial.printf (" v%s / BTN4 트리거 진단\n", FW_VERSION);
    Serial.println("==================================================");
    Serial.printf(" SCL(공유)=GPIO%u   SDA=GPIO%u/%u/%u/%u\n",
                  (unsigned)BB_SCL_PIN, (unsigned)kSda[0], (unsigned)kSda[1],
                  (unsigned)kSda[2], (unsigned)kSda[3]);
    if (BB_HALF_BIT_NS == 0) {
        Serial.println(" 반주기 지연: 없음 (최대 속도)");
    } else {
        Serial.printf(" 반주기 지연: 목표 %u ns / 실측 %u ns -> 약 %u kHz\n",
                      (unsigned)BB_HALF_BIT_NS, (unsigned)bitBang.measuredHalfBitNs(),
                      (unsigned)(bitBang.measuredHalfBitNs() ? (500000UL / bitBang.measuredHalfBitNs()) : 0));
    }
    Serial.printf(" 설정된 컨트롤러=%s (열 오프셋 +%u)\n", OLED_DRIVER_NAME, (unsigned)OLED_COL_OFFSET);
    Serial.println("--------------------------------------------------");

    diagBusScan();     Serial.println();
    diagProbe();       Serial.println();
    delay(300);

    diagControllerSweep();  Serial.println();

    diagOwnFramePath();     Serial.println();

    Serial.println("--------------------------------------------------");
    Serial.println(" 판정 가이드");
    Serial.println("  1) 진단1에서 NOT FOUND");
    Serial.println("       -> 배선/전원 (SDA, SCL, VCC, GND, 모듈 풀업)");
    Serial.println("  2) 진단3에서 네 컨트롤러가 **모두** 안 보임");
    Serial.println("       -> 패널/전원. USB 전류 부족이 흔하다 — OLED 4개는 합계 100mA 이상.");
    Serial.println("          외부 3.3V 전원으로 시험하거나 화면을 1개만 연결해 본다.");
    Serial.println("  3) 진단3에서 특정 컨트롤러만 제대로 보임");
    Serial.println("       -> config.h 의 OLED_DRIVER 를 그 값으로 바꾸고 다시 부팅.");
    Serial.println("          값: SH1106 / SSD1315 / SSD1306  (WINSTAR는 SH1106 계열)");
    Serial.println("  4) 진단4의 A / B / C 를 비교");
    Serial.println("       · 셋 다 같음        -> 모든 전송 경로 정상");
    Serial.println("       · B만 다름          -> 캡처 후 동시 재생 로직 결함");
    Serial.println("                              (BB_FRAME_PATH_U8G2 로 두면 정상 동작)");
    Serial.println("       · C만 다름          -> 페이지 diff/구간 계산 결함");
    Serial.println();
    Serial.println(" ※ 진단3에서 '테두리가 삐뚤거나 한쪽이 잘림'이면 열 오프셋 문제입니다.");
    Serial.println("    테두리가 반듯한 컨트롤러를 고르세요.");
    Serial.println("==================================================");
    Serial.println(" 진단 모드 — 무한 대기 (재부팅하려면 EN 버튼)");

    while (1) {
        delay(1000);
    }
}
