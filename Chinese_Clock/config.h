// worynim@gmail.com
/**
 * @file config.h
 * @brief 프로젝트 전역 설정 및 하드웨어 핀 맵 정의
 * @details I2C 주소, Wi-Fi 초기 접속 정보, 폰트 기하 등 시스템 상수를 관리
 * @note [SYNC] 원본: ENG_Clock/config.h — [1][3][4][5][8][10][11]은 그대로,
 *       [2] 기본 시간대·[6] AP SSID·[7] 폰트 기하·[9] 표시 형식은 중국어판에 맞게 변경
 *
 * @note 기하 설계 근거는 PLAN.md §5를 따른다.
 *   중국어 표현은 공백 없는 한자 문자열이고 최장이 4자다. 한자는 정사각형이므로
 *   4 × 32 = 128px로 화면 폭에 정확히 맞아 줄바꿈·피치 사다리가 모두 불필요해진다.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// === [1] 하드웨어 핀 정의 (ESP32-C3 기반) ===
#define BTN1_PIN 1
#define BTN2_PIN 4
#define BTN3_PIN 10
#define BTN4_PIN 9

#define HW_SDA_PIN 5
#define HW_SCL_PIN 6
#define SW_SDA_PIN 2
#define SW_SCL_PIN 3

#define BUZZER_PIN 7

// === [2] NTP 및 시간 설정 ===
// 고정 오프셋 대신 POSIX TZ 문자열을 쓴다 (DST 자동 처리).
// NTP는 UTC로 수신하고 localtime() 시점에 TZ 환경변수가 적용된다.
#define NTP_SERVER1 "pool.ntp.org"
#define NTP_SERVER2 "time.nist.gov"

// 웹에서 시간대를 설정하기 전까지의 폴백 (config_manager가 NVS에 보관)
#define DEFAULT_TIMEZONE "CST-8"
#define TIMEZONE_MAX_LEN 48   // SystemSettings.timezone 배열 크기와 일치해야 한다

// === [3] 디스플레이 설정 ===
#define NUM_SCREENS 4
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define PAGES_PER_SCREEN 8
#define TILES_PER_PAGE 16

// === [4] I2C 성능 설정 ===
#define I2C_SPEED_HZ 800000
#define I2C_TX_TIMEOUT_MS 50
#define I2C_CMD_TIMEOUT_MS 10
#define I2C_SYNC_TIMEOUT_MS 100
#define I2C_ERROR_THRESHOLD 50   // 누적 오류 횟수 — 초과 시 recoverBus()로 버스 자가 복구
#define HW_I2C_BUF_SIZE 256

// === [5] RTOS 태스크 설정 ===
#define HW_TASK_STACK 4096
#define HW_TASK_PRIO 10
#define HW_TASK_CORE 0

// === [6] 앱 설정 ===
#define WIFI_SSID_AP "ZH_Clock_Setup"
#define UPDATE_INTERVAL_MS 1000  // 1초마다 갱신

// === [7] 폰트 및 설계 설정 ===
// 상태 메시지/도움말은 U8g2 내장 라틴 폰트를 직접 쓴다 (비트맵 캐시 미경유).
#define STATUS_FONT u8g2_font_6x10_tf

// [폴백 폰트] 중국어판은 CJK 폴백 폰트를 도입하지 않는다 (PLAN §6.7 대안 A).
//   ENG판은 Flash 99%(여유 2KB)로 U8g2 CJK 폰트 하나만 추가해도 용량 초과가 확정적이다.
//   커스텀 폰트가 이미 필수다 — btn2_short()가 "Font Required!"를 표시한다.
//   이 상수는 renderer.cpp의 폴백 경로(캐시 미적용 시)가 **라틴** 글리프를 그릴 때 쓴다.
#define CHINESE_FONT u8g2_font_logisoso20_tf

// --- 글자 셀 기하 (PLAN §5.3) ---
// 한자 1자 = 정사각형. 4자 × 32px = 128px = 화면 폭이므로 줄바꿈이 없다.
#define GLYPH_W        32                        // 픽셀 폭 (피치 — 레이아웃 전용)
#define GLYPH_H        48                        // 래스터 높이 (한자 잉크는 32px)

// [래스터 규격] ENG판 v4(좌우 클리핑)·v5(상하 클리핑) 실패를 처음부터 배제한다 (PLAN §5.4).
//   래스터가 피치와 같은 폭/높이여야만 경계 클리핑이 생기지 않는다.
//   48px = 32px 피치 + 좌우 8px씩 여유. 글자당 6 × 48 = 288 B.
#define RASTER_BYTES_PER_ROW 6                    // 48px 행 = 6 bytes

//
// [미사용 — 펌웨어도 JS도 읽지 않음] 삭제 대상. ENG판의 구 형식 잔재였다.
//   (ENG판에 있던 GLYPH_BYTES_PER_ROW = 2 참조)
// [미사용이나 삭제 금지 — test/js/layout_crosscheck.mjs의 parseDefines()가 config.h를
//  파싱해 이 값들을 검증 기준값으로 쓴다(읽지 못하면 "기준값 없음"으로 실패한다)]
#define MAX_CHARS_PER_LINE 4                     // 4 × 32 = 128px — JS 교차검증의 기준값
#define MAX_LAYOUT_CHARS   4                     // 1줄 × 4자 — JS 교차검증의 기준값
#define LINE_COUNT     1                         // 중국어 표현은 항상 1줄

// GLYPH_W / GLYPH_H / RASTER_BYTES_PER_ROW의 미러는 web_pages.h의 동일 상수다.
//   펌웨어 C++는 renderer_geometry의 셀 기하를 대신 쓰므로 직접 참조하지 않는다.
#define LINE_HEIGHT    48                        // 1줄 = 래스터 높이

// IP 주소 화면: 도트는 마지막 숫자 옆("192.")에 숫자 하단에 맞춰 찍는다.
// IP_DOT_SIZE = 도트 한 변(픽셀), IP_DOT_GAP = 마지막 숫자와 도트 사이의 가로 간격.
#define IP_DOT_SIZE    4
#define IP_DOT_GAP     8

// 폴백(라틴) 폰트 baseline — **줄 상단(rasterTopY)에서 아래로 내리는 상대 오프셋**.
//   ENG판은 LINE_HEIGHT=32에서 26을 썼다(실사용 검증된 값). 20px 폴백 폰트의 잉크가
//   대략 [baseline−15, baseline+5]이므로 32px 밴드에서 거의 꽉 차는 값이다.
//   중국어판은 LINE_HEIGHT=48이므로 같은 "밴드 하단 여백(6px)"을 유지해 48−6=42로 정한다.
//   ⚠️ 이 값은 계산으로 얻은 것이지 실측이 아니다 — **검증 게이트에서 눈으로 확인**할 것.
#define FALLBACK_BASELINE 42

// === [8] 애니메이션 설정 ===
#define ANIMATION_TYPE_NONE 0
#define ANIMATION_TYPE_SCROLL_UP 1
#define ANIMATION_TYPE_SCROLL_DOWN 2
#define ANIMATION_TYPE_VERTICAL_FLIP 3
#define ANIMATION_TYPE_DITHERED_FADE 4
#define ANIMATION_TYPE_ZOOM 5
// 애메이션 타입 개수 — BTN3 순환(.ino)과 웹 입력 검증(web_manager)이 **반드시 이 값을 쓴다.
// 애니메이션을 추가할 때 여기를 늘리지 않으면 버튼으로 못 고르는 모드가 조용히 생긴다.
#define ANIMATION_TYPE_COUNT 6
#define ANIMATION_STEP_DELAY_MS 10 // 고속 프레임

// === [9] 표시 형식 설정 ===
// CLOCK_MODE_WORD는 한글판·영어판과 같은 값(0)이다.
// NVS 키 "mode"의 기존 값 호환을 위해 값을 유지한다.
#define CLOCK_MODE_WORD 0
#define CLOCK_MODE_NUMERIC 1
#define HOUR_FORMAT_12H 0
#define HOUR_FORMAT_24H 1

// 문자판 (PLAN §6.8). 간체/번체는 点/時/兩 3글자만 달라 치환 표로 처리한다.
// ⚠️ 이 이름은 매크로이므로 순수 모듈(chinese_time_core.h)에서 **같은 이름을 쓰면 안 된다.**
//    쓰면 열거자가 숫자로 치환되어 컴파일이 깨진다 — 실제로 한 번 깨진 뒤
//    core 쪽 열거자에 CHT_ 접두사를 붙였다. 새 상수를 만들 때 이 충돌을 떠올릴 것.
#define SCRIPT_SIMPLIFIED 0    // 简体
#define SCRIPT_TRADITIONAL 1   // 繁體

// [표시 방식 — 2026-10-05] 사용자가 BTN2 short로 순환시키는 3단계.
//   간체/번체/숫자는 저장상 서로 다른 두 필드의 조합이다:
//     script_type   (간체/번체)  ×  display_mode (한글/숫자)
//   순환 UI는 **하나의 값**으로 보여주므로, 이 조합을 한 값으로 다루는 경계가 필요하다.
//   저장 필드는 그대로 둔다 — NVS 마이그레이션이 필요 없어진다.
#define PRESENTATION_SIMPLIFIED 0   // 简体 — 한자로, 간체
#define PRESENTATION_TRADITIONAL 1  // 繁體 — 한자로, 번체
#define PRESENTATION_NUMERIC 2      // 數字 — 13:05
#define PRESENTATION_COUNT 3

// [삭제 — 2026-10-05] SCRIPT_SLOT_SIMPLIFIED / SCRIPT_SLOT_TRADITIONAL.
//   문자판마다 슬롯을 정해두는 규칙이었다. Font Studio가 슬롯에 **두 문자판의 합집합
//   37자**를 올리게 바뀌면서 이 규칙의 전제가 사라졌다 — 슬롯 하나가 어느 문자판이든
//   되므로 문자판과 슬롯을 서로 독립시킬 수 있다. 상수를 남겨 두면 없는 규칙처럼 읽힌다.

// === [10] 기타 하드코딩 상수 통합 ===
#define I2C_ADDR_HW_0 0x3C
#define I2C_ADDR_HW_1 0x3D
#define MAX_BITMAP_SIZE 512   // 288B(중국어 기본) ≤ 512 — 변경 불필요
#define WEB_PORT 80

// === [11] UI 및 버튼 동작 상수 ===
#define UI_STAGE_COUNT 3
#define FONT_SLOT_COUNT 5         // 폰트 슬롯 수 (경로 "/f0"~"/f4", 버튼 순환·웹 입력 검증 상한)

/**
 * 슬롯 이름표(폰트 파일 이름)의 최대 길이 — 바이트 단위, UTF-8. 이것을 **넘으면 거부**한다.
 * @details [2026-10-05 사용자 보고로 수정] 예전엔 **31바이트**였다(`display_manager` 의 `>= 32`).
 *          그 근거 주석은 "이 값은 … \"/fN/name.txt\" 파일명으로도 쓰인다"였는데 **사실이 아니다** —
 *          경로는 `"/f" + String(slot) + "/name.txt"` 라는 **리터럴**이고, 이름은 그 파일의
 *          **내용**으로만 들어간다. 이름에 `/` 가 있어도 경로가 바뀌지 않는다.
 *          이름이 실제로 흘러가는 곳은 전부 길이 제한이 없다:
 *            · `f.print(name)`        → `name.txt` 의 내용 (LittleFS, 제한 없음)
 *            · `jsonEscape()`         → `/api/config` JSON (web_manager 가 출구에서 이스케이프)
 *            · `SystemSettings::font_name` = **`String`** (고정 버퍼 아님) → NVS `putString`
 *          그래서 31은 **근거 없는 제한**이었고, 35바이트짜리 실제 폰트 이름
 *          (`MFXuanRen_Noncommercial-Regular.ttf`)이 **조용히** 거부돼 슬롯이 "Empty Slot"으로
 *          보였다. 상한을 아예 없애지는 않는다 — 이름은 JSON 응답과 웹 드롭다운에 그대로
 *          실리므로 무한정 받을 이유가 없다. 64바이트면 중문 이름 + 라이선스 접미사가 대부분 들어간다.
 *          ⚠ 넘는 이름은 거부되고 시리얼에 `[WEB] Slot name rejected: … (len=NN)` 로 남는다
 *            (조용히 넘어가지 않는다). 늘리려면 **이 한 곳만** 고치면 된다.
 */
#define FONT_NAME_MAX_LEN 64

#define LONG_PRESS_TIME_MS 1000
#define DEBOUNCE_TIME_MS 50
#define WIFI_CONFIG_TIMEOUT 120
#define DEBUG_MODE 1                             // 1: 디버그 정보 출력 활성화, 0: 비활성화

// Font Studio 슬라이더 범위 (PLAN §5.4). 기본 32px는 피치와 정확히 맞아 겹침이 0이다.
// 40px를 32px 피치에 놓으면 인접 글자와 4px씩 겹쳐 CJK는 판독이 불가능해진다.
#define FONT_SLIDER_MIN 20
#define FONT_SLIDER_MAX 40
#define FONT_SLIDER_DEFAULT 32

#endif