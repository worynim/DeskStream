// worynim@gmail.com
/**
 * @file ds_display_config.h
 * @brief 화면 엔진 런타임 설정 (DisplayCore::begin()에 넘긴다)
 * @details 라이브러리는 스케치의 config.h를 볼 수 없으므로 프로젝트별 값은 이 구조체로 받는다.
 *          기본값은 Hangeul_Clock v2.10.0 실측 채택값이다 (4화면 풀 프레임 22.4ms / 44.6fps).
 */
#ifndef DS_DISPLAY_CONFIG_H
#define DS_DISPLAY_CONFIG_H

#include <U8g2lib.h>

namespace ds {

struct DisplayConfig {
    /** 화면 회전. U8G2_R2 + setFlip(false)가 이 보드의 정방향이다 (DisplayCore::setFlip 주석 참조) */
    const u8g2_cb_t* rotation = U8G2_R2;

    /**
     * HW I2C(화면 0·1) SCL 설정 주파수.
     * @details 실효값은 설정보다 약 18% 낮게 나온다(풀업 상승 시간). C3 HW I2C 보증은 800kHz까지.
     *          실측 (HW 1화면 풀 프레임):
     *            800000  → 실효 710kHz, 14.3ms
     *            1000000 → 실효 826kHz, 12.35ms
     *            1100000 → 실효 881kHz, 11.62ms (+ 병합 전송 904kHz, 11.40ms) ← 기본값
     *            1200000 → ✗ 화면 0 NACK (0x103) — 패널이 못 따라옴
     *          오류·노이즈가 보이면 낮춘다.
     */
    uint32_t hwI2cHz = 1100000;

    /**
     * HW 버스 페이지 전송을 한 트랜잭션으로 합친다 (SSD1306 제어 바이트 Co 비트).
     * @details 화면당 트랜잭션 16 → 8회 (회당 ~27us 절약). 화면이 깨지면 false로 되돌린다.
     */
    bool hwMergedPageTx = true;

    /**
     * SW I2C(화면 2·3) 반주기 지연 (ns). 0 = 지연 없음.
     * @details 150 → 실효 1044kHz, 100 → 1189kHz (1화면). 화면이 깨지면 키운다.
     */
    uint32_t swHalfBitNs = 100;
    uint32_t swSclSpinMax = 32;          ///< SCL 상승을 레지스터로 다시 읽는 횟수 (넘으면 micros() 경로)
    uint32_t swStretchTimeoutUs = 100;   ///< SCL이 이 시간 넘게 LOW면 트랜잭션 중단 (stuck)

    uint32_t txTimeoutMs = 50;     ///< HW 데이터 전송 타임아웃
    uint32_t cmdTimeoutMs = 10;    ///< HW 명령 전송 타임아웃
    uint32_t syncTimeoutMs = 100;  ///< 메인이 HW 태스크 완료를 기다리는 최대 시간
    uint32_t errorThreshold = 50;  ///< 누적 오류 점수가 이를 넘으면 버스 자가 복구

    uint32_t hwTaskStack = 4096;
    uint32_t hwTaskPrio = 10;
    int32_t  hwTaskCore = 0;
};

}  // namespace ds

#endif
