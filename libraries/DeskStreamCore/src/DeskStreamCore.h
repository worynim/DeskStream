// worynim@gmail.com
/**
 * @file DeskStreamCore.h
 * @brief DeskStream 공통 모듈 묶음 헤더
 * @details 스케치는 반드시 이 헤더를 include 한다. Arduino IDE는 src/ 루트의 헤더 이름으로
 *          라이브러리를 찾으므로, 하위 폴더 헤더만 include 하면 라이브러리가 빌드에 포함되지 않는다.
 *          쓰지 않는 모듈의 코드는 링커가 제거한다(--gc-sections).
 */
#ifndef DESKSTREAM_CORE_H
#define DESKSTREAM_CORE_H

#define DS_CORE_VERSION "0.5.0"   // library.properties 의 version 과 일치해야 한다

#include "ds_board.h"

// 순수 모듈 (Arduino 의존 없음 — 호스트 테스트 대상)
#include "display/ds_canvas.h"
#include "display/ds_i2c_retry_policy.h"
#include "input/ds_button_fsm.h"
#include "net/ds_time_sync_policy.h"
#include "net/ds_tz_util.h"
#include "util/ds_json_lite.h"
#include "util/ds_utf8.h"

// 서비스
#include "display/ds_display.h"
#include "input/ds_buttons.h"
#include "sound/ds_buzzer.h"
#include "net/ds_wifi.h"
#include "net/ds_time_sync.h"
#include "net/ds_web_config.h"
#include "store/ds_config.h"
#include "util/ds_logger.h"

#endif
