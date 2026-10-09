// worynim@gmail.com
/**
 * @file config.h
 * @brief 프로젝트 전역 설정 및 하드웨어 핀 맵 정의
 * @details I2C 주소, Wi-Fi 초기 접속 정보, 애니메이션 타이밍 등 시스템 상수를 관리
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "renderer_layout.h"   // 배치 상수의 정의처 (GLYPH_CELL_W·RIGHT_COL_X·LAYOUT_MAX_CHARS 유래)

// === [1] 하드웨어 핀 정의 (ESP32-C3 기반 · BitBang I2C) ===
// [BB v1.0.0] 1.3" SSD1315 모듈은 I2C 주소를 바꿀 수 없다(전부 0x3C).
//   그래서 "2버스 × 2주소" 대신 **4개의 독립 버스**를 만들되, SCL 1개를 공유하고
//   SDA만 화면별로 분리한다. 5핀으로 4버스를 얻는 구성이다.
#define BTN1_PIN 0
#define BTN2_PIN 1
#define BTN3_PIN 3
#define BTN4_PIN 4

#define BB_SCL_PIN 10           // 4버스 공유 클럭
#define BB_SDA_PIN_1 5          // 물리 OLED1
#define BB_SDA_PIN_2 6          // 물리 OLED2
#define BB_SDA_PIN_3 7          // 물리 OLED3 (구 부저핀)
#define BB_SDA_PIN_4 8          // 물리 OLED4

/**
 * 부저 연결 여부.
 * @details [BB v1.0.0] 개요.md에서 부저 미연결로 확정. 0이면 beep()/멜로디가
 *          no-op으로 컴파일된다(tone() 호출 자체가 사라진다).
 */
#define HAS_BUZZER 0

#if HAS_BUZZER
/**
 * ⚠ 부저를 켜기 전에 읽을 것 — 이 보드에는 **빈 핀이 없다.**
 *
 * 옛 BUZZER_PIN(GPIO 7)은 지금 **OLED3의 SDA**다. 되돌리면 화면 3이 죽는다.
 * 이 프로젝트가 쓰지 않는 핀은 GPIO 2와 GPIO 9뿐인데,
 *   · GPIO 2 — ESP32-C3 **스트래핑 핀**. 부팅 시 HIGH여야 한다. 부저(능동 부저는
 *     보통 LOW 임피던스)를 달면 부팅이 막힐 수 있다.
 *   · GPIO 9 — 보드의 **BOOT 버튼**. 버튼이 이미 붙어 있다.
 * → 어느 쪽이든 하드웨어 판단이 필요하므로 기본값을 정해 두지 않는다.
 *
 * 방법: config.h에서 BUZZER_PIN을 직접 정의하거나,
 *       `arduino-cli compile --build-property "compiler.cpp.extra_flags=-DBUZZER_PIN=9"`
 *       처럼 빌드 옵션으로 넘긴다.
 */
#ifndef BUZZER_PIN
#error "HAS_BUZZER=1 인데 BUZZER_PIN이 정의되지 않았습니다. GPIO 7은 OLED3 SDA라 쓸 수 없습니다 — 위 주석을 보고 핀을 지정하세요."
#endif
#endif  // HAS_BUZZER

// === [2] NTP 및 시간 설정 ===
// 고정 오프셋 대신 POSIX TZ 문자열을 쓴다 (웹에서 설정, DST 자동 처리).
// NTP는 UTC로 수신하고 localtime() 시점에 TZ 환경변수가 적용된다.
#define NTP_SERVER1 "pool.ntp.org"
#define NTP_SERVER2 "time.nist.gov"

// 웹에서 시간대를 설정하기 전까지의 폴백 (config_manager가 NVS에 보관)
#define DEFAULT_TIMEZONE "KST-9"
#define TIMEZONE_MAX_LEN 48   // SystemSettings.timezone 배열 크기와 일치해야 한다

// === [3] 디스플레이 설정 ===
#define NUM_SCREENS 4
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define PAGES_PER_SCREEN 8
#define TILES_PER_PAGE 16

/**
 * 버스 번호 → SDA 핀. **v1.0.3에서 검증된 그대로 둔다.**
 * @note [v1.0.4 교훈] 화면 순서를 바꾸려고 이 배열을 뒤집었다가 화면이 깨졌다.
 *       원인 규명 전까지 **검증된 구성을 건드리지 않는다.**
 *       화면 순서는 아래 `BB_BUS_OF_SCREEN()`으로 뒤집는다(화면↔버스 배정만 변경).
 */
#define BB_SDA_PINS { BB_SDA_PIN_1, BB_SDA_PIN_2, BB_SDA_PIN_3, BB_SDA_PIN_4 }

/**
 * 화면 순서 뒤집기 (OLED1234 → OLED4321).
 *
 * @details 1이면 `screens[0]`이 **마지막 SDA(GPIO 8)** 에 그려진다.
 *          시계는 화면 0→3 순서로 `[오전오후/날짜][시][분][초]`를 그리므로,
 *          이 스위치가 곧 좌→우 배치를 뒤집는다.
 *
 * @note **핀 배열(BB_SDA_PINS)은 건드리지 않는다.** 버스↔핀 대응은 v1.0.3에서
 *       검증된 그대로 두고, 화면이 어느 버스를 쓰는지만 바꾼다. 두 방식은 논리적으로
 *       같아 보이지만, 검증된 코드를 최대한 건드리지 않는 쪽을 택했다.
 */
#define BB_SCREEN_ORDER_REVERSED 1

/** @brief 화면 i가 사용할 버스 번호 */
#define BB_BUS_OF_SCREEN(i) ((BB_SCREEN_ORDER_REVERSED) ? (NUM_SCREENS - 1 - (i)) : (i))

/**
 * @name OLED 컨트롤러 선택
 * @details 1.3" 128×64 I2C 모듈은 **컨트롤러가 여러 종류**다. 겉모습과 핀 배치는
 *          똑같고 I2C 주소도 0x3C로 같아서, **소프트웨어로는 구별할 수 없다**
 *          (I2C에 읽기 경로가 없어 ID를 되읽을 방법이 없다).
 *
 *          잘못 고르면 이런 일이 생긴다:
 *            · SH1106을 SSD13xx로 구동 → SH1106은 **132열 RAM**이라 가시 영역이
 *              2열부터 시작한다. 열 오프셋(+2)을 안 주면 화면이 2픽셀 밀린다.
 *            · SSD13xx를 SH1106으로 구동 → 반대로 2픽셀 밀리고,
 *              초기화 시퀀스가 달라 화면이 아예 안 켜질 수도 있다.
 *
 *          → 어느 것인지 모르면 `BB_DIAG_MODE 1`로 업로드해 **컨트롤러 스윕**을 돌린다.
 *            네 종류를 차례로 초기화하며 패턴을 그리므로, **제대로 나오는 것을 눈으로
 *            고르면** 된다 (bb_diagnostic.cpp 참조).
 *
 *          기본값이 SH1106인 이유: 1.3" 모듈에서 **가장 흔한** 컨트롤러다.
 *          (0.96"는 대부분 SSD1306)
 */
#define OLED_DRIVER_SH1106  1
#define OLED_DRIVER_SSD1315 2
#define OLED_DRIVER_SSD1306 3

/** @brief 사용할 컨트롤러. 진단 스윕에서 확인한 값으로 바꾼다. */
#define OLED_DRIVER OLED_DRIVER_SH1106

#if OLED_DRIVER == OLED_DRIVER_SH1106
  #define OLED_DRIVER_NAME "SH1106"
  /**
   * 컨트롤러 RAM의 열 주소 보정값.
   * @details SH1106은 132열 RAM을 갖고 **가시 영역이 2열부터** 시작한다.
   *          그래서 화면 x=0을 쓰려면 컨트롤러에는 열 2를 지정해야 한다.
   *          (U8g2의 SH1106 드라이버도 default_x_offset = 2 로 같은 일을 한다)
   */
  #define OLED_COL_OFFSET 2
#elif OLED_DRIVER == OLED_DRIVER_SSD1315
  #define OLED_DRIVER_NAME "SSD1315"
  #define OLED_COL_OFFSET 0
#elif OLED_DRIVER == OLED_DRIVER_SSD1306
  #define OLED_DRIVER_NAME "SSD1306"
  #define OLED_COL_OFFSET 0
#else
  #error "OLED_DRIVER 값이 올바르지 않습니다 (OLED_DRIVER_SH1106 / SSD1315 / SSD1306 중 하나)"
#endif

// === [4] BitBang I2C 성능 설정 ===
/**
 * SCL **반주기** 지연 (ns). 비트 주기 = 2 × 이 값.
 * @details [v1.0.8] `delayMicroseconds(1)`을 쓰면 **호출 오버헤드** 때문에 실효 반주기가
 *          1.3us쯤 되어 실측 약 455kHz밖에 안 나왔다. 원본 Hangeul_Clock은 화면 2개를
 *          하드웨어 I2C **800kHz**로, 나머지 2개는 지연 콜백이 없는 SW I2C(사실상 최대
 *          속도)로 돌렸다 — 그래서 원본이 더 빨랐다.
 *
 *          지금은 부팅 시 **CPU 사이클을 실측해** 목표 ns에 맞춘 NOP 루프를 쓴다.
 *          보정 결과는 시리얼에 찍힌다:
 *            `[I2C] 반주기 지연: 목표 500ns / 실측 512ns -> 약 976 kHz`
 *
 *          | 값(ns) | 비트 주기 | 실효 클럭 |
 *          |--:|--:|--:|
 *          | 2500 | 5.0us | 약 200 kHz (가장 안전) |
 *          | 1250 | 2.5us | 약 400 kHz (SH1106 데이터시트 상한) |
 *          | 600 | 1.2us | 약 833 kHz |
 *          | **0 (기본)** | 지연 없음 | **최대 속도 — 실기에서 원본과 동급 확인** |
 *
 *          ⚠ 0은 **지연을 아예 넣지 않는다**. SCL 상승은 `sclWaitHigh()`가 실제 상승을
 *            기다려 자연히 맞춰지고, LOW 기간은 GPIO 쓰기 속도가 결정한다. 원본
 *            Hangeul_Clock의 SW I2C 경로도 지연 콜백이 없어 같은 방식이었다.
 *            실기에서 정상 동작을 확인했지만, **화면이 깨지면 이 값을 키운다**(예: 1250).
 */
#define BB_HALF_BIT_NS 0
#define BB_I2C_SPEED_HZ 800000  // Multi_BitBang(부팅 진단)에 넘기는 참고값

/**
 * U8g2 초기화 시퀀스(즉시 전송 경로)가 누적하는 패킷 버퍼 크기 (화면당 1개).
 * @note 초기화 명령은 몇 바이트 단위라 256이면 충분하다. 넘치면 그 패킷은 버린다 —
 *       절반만 나간 명령 시퀀스는 화면을 알 수 없는 상태로 만든다.
 */
#define I2C_PACKET_BUF_SIZE 256

/** 누적 오류 횟수 — 초과 시 recoverBus()로 버스 자가 복구 */
#define I2C_ERROR_THRESHOLD 50

/**
 * SCL이 HIGH로 올라오기를 기다리는 상한 (us).
 * @details [필수] BitBang의 최대 위험은 **stuck-low를 ACK 성공으로 오독**하는 것이다.
 *          SDA가 LOW로 고정되면 ack=0(=성공)으로 읽혀 오류가 영원히 보고되지 않는다.
 *          SCL 상승을 감시해 한계를 넘으면 그 트랜잭션을 실패로 처리하고
 *          recoverBus()가 클럭 펄스로 버스를 풀어준다.
 */
#define BB_SCL_STRETCH_TIMEOUT_US 3000

/**
 * 하드웨어 진단 진입 — **BTN4(GPIO 4)를 누른 채 부팅**하면 진단 모드로 들어간다.
 *
 * @details 왜 컴파일 스위치(BB_DIAG_MODE)를 없앴나:
 *          이 파일은 클라우드 동기화로 **예전 내용으로 되돌아간 사례가 여러 번** 있었다.
 *          진단 스위치가 1로 되돌아가면 펌웨어가 시계 대신 진단만 돌리는데,
 *          사용자는 그 사실을 모른 채 "화면이 이상하다"고 판단하게 된다.
 *          **런타임 트리거는 되돌아갈 스위치가 없으므로 이 혼동이 원천적으로 사라진다.**
 *
 *   사용법: BTN4를 누른 상태로 전원/리셋 → 시리얼(115200)과 화면을 본다.
 *           BTN4를 누르지 않으면 항상 정상 시계로 부팅한다.
 */
#define BB_DIAG_BUTTON_PIN BTN4_PIN
/** 이 시간 이상 눌린 상태여야 진단으로 들어간다 (기계적 채터링 배제) */
#define BB_DIAG_HOLD_MS 300

/**
 * @name 화면 전송 경로 선택
 * @details dirty 페이지만 골라내는 diff는 두 경로가 **똑같이** 쓰고, 전송 방식만 다르다.
 *
 *  - `BB_FRAME_PATH_CONCURRENT`(0) — **4버스 동시 BitBang** (I2CPlatform + 워커 태스크).
 *    SCL 공유를 이용해 4화면에 같은 클럭으로 서로 다른 바이트를 싣는다. 빠르지만
 *    페이지/열 명령과 **컨트롤러 열 오프셋을 우리가 직접** 만들어야 한다.
 *  - `BB_FRAME_PATH_U8G2`(1) — **U8g2 CAD** (`screen->updateDisplayArea()`).
 *    컨트롤러별 명령 시퀀스와 열 오프셋을 U8g2가 만든다 — **검증된 경로**다.
 *    대신 버스별 순차 전송이라 4버스 동시 전송보다 느리다.
 *
 * @note [v1.0.3] 실기에서 0(동시 BitBang)의 출력이 깨져 1로 전환했다.
 *       1은 진단 3에서 SH1106으로 정상 표시가 확인된 경로다.
 *       0의 결함이 규명되면 다시 0으로 두어 애니메이션 성능을 되찾는다 —
 *       진단 4가 두 경로를 같은 내용으로 그려 비교해 그 판정을 한다.
 */
#define BB_FRAME_PATH_CONCURRENT 0
#define BB_FRAME_PATH_U8G2       1
#define BB_FRAME_PATH BB_FRAME_PATH_CONCURRENT

// === [6] 앱 설정 ===
/** 펌웨어 버전 — README/RELEASE_NOTES와 부팅 로그가 같은 값을 가리키게 하는 정의처 */
#define FW_VERSION "1.0.9"

// [BB v1.0.0] AP 이름을 원본(Hangeul_Clock_Setup)과 구분한다 —
//   두 기기가 동시에 설정 모드에 들어가면 어느 쪽에 붙었는지 알 수 없다.
//   ⚠ 길이 상한: 이 이름은 설정 모드 화면에 6x10 폰트로 그려진다. 128px ÷ 6px = 21자까지.
//     넘기면 조용히 잘리므로 21자 이하로 유지한다 (아래는 16자).
#define WIFI_SSID_AP "Hangeul_Clock_BB"
#define UPDATE_INTERVAL_MS 1000  // 1초마다 갱신

// === [7] 폰트 및 디자인 설정 ===
#define HANGEUL_FONT u8g2_font_unifont_t_korean1  // 경량 한글 폰트 (메모리 절약)
#define STATUS_FONT u8g2_font_6x10_tf           // 상태 메시지용 폰트
#define TEXT_Y_POS 42                            // 한글 텍스트 출력 높이 (0~63)
// [SYNC] 배치 상수의 정의처는 renderer_layout.h다. 이쪽은 그 값을 그대로 노출할 뿐이다.
//   두 곳에 따로 적으면 값이 어긋난 채로 컴파일되어 버린다.
#define GLYPH_CELL_W LAYOUT_CELL_W       // 한 글자의 가로 칸 (한글 32px 래스터 폭)
#define RIGHT_COL_X  LAYOUT_RIGHT_COL_X  // 우측 고정 열의 시작 x

// === [8] 애니메이션 설정 ===
#define ANIMATION_TYPE_NONE 0
#define ANIMATION_TYPE_SCROLL_UP 1
#define ANIMATION_TYPE_SCROLL_DOWN 2
#define ANIMATION_TYPE_VERTICAL_FLIP 3
#define ANIMATION_TYPE_DITHERED_FADE 4
#define ANIMATION_TYPE_ZOOM 5
#define ANIMATION_TYPE_SNOW_ASSEMBLE 6
#define ANIMATION_TYPE_SPLIT_FLAP 7
#define ANIMATION_TYPE_COUNT 8      // BTN3 순환 및 웹 입력 검증 상한
#define ANIMATION_STEP_DELAY_MS 10  // 고속 프레임 (기존 모드 공통)

#define ANIMATION_STEPS_DEFAULT 16  // 기존 모드 및 분할 플랩 프레임 수
#define ANIMATION_STEP_DELAY_SNOW_MS 16
#define ANIMATION_STEPS_SNOW 48    // 눈 조립 모드: 48 x 16ms = 약 0.8초
#define ANIM_SNOW_FALLBACK_THRESHOLD 128 // 픽셀 조립이 불가능한 글자는 진행도 절반 이후에 시스템 폰트로 그린다
#define ANIM_PROGRESS_FULL 255          // 픽셀/행 단위 애니메이션 진행도의 만점 (스텝 → 0~255 매핑 상한)

// 분할 플랩 접힘 기하 (web_pages.h의 FLAP_HALF_H / FLAP_PHASE_SPLIT과 맞춰야 한다)
#define ANIM_FLAP_HALF_H (SCREEN_HEIGHT / 2) // 글자를 접는 축의 높이 절반 (위쪽 0~31, 아래쪽 32~63)
#define ANIM_FLAP_PHASE_SPLIT 128              // 이 진행도부터 아래쪽 절반 접힘으로 넘어간다

// === [9] 표시 형식 설정 ===
#define CLOCK_MODE_HANGUL 0
#define CLOCK_MODE_NUMERIC 1
#define HOUR_FORMAT_12H 0
#define HOUR_FORMAT_24H 1

// === [10] 기타 하드코딩 상수 통합 ===
// [BB v1.0.0] 1.3" SSD1315 모듈은 주소 선택 패드가 없다 → 4화면 전부 0x3C.
//   화면 구분은 **버스 번호**(공유 SCL + 개별 SDA)로만 한다. 0x3D는 존재하지 않는다.
#define OLED_I2C_ADDR 0x3C
#define MAX_BITMAP_SIZE 512
#define WEB_PORT 80

// === [11] UI 및 버튼 동작 상수 ===
#define UI_STAGE_COUNT 3
#define FONT_SLOT_COUNT 5         // 폰트 슬롯 수 (경로 "/f0"~"/f4", 버튼 순환·웹 입력 검증 상한)
/**
 * 슬롯 이름표(폰트 파일 이름)의 최대 길이 — 바이트 단위, UTF-8. 이것을 **넘으면 거부**한다.
 * @details [A-2④ 수정 — 2026-10-06, 중국어판 §12.14 승계] 예전엔 `name.length() >= 32`
 *          (실질 31바이트)였다. 그 근거 주석은 "이름을 \"/fN/name.txt\" 파일명으로도 쓴다"였는데
 *          **사실이 아니다** — 경로는 `"/f" + String(slot) + "/name.txt"` 라는 **리터럴**이고,
 *          이름은 그 파일의 **내용**으로만 들어간다. 이름이 흘러가는 곳은 전부 길이 제한이 없다:
 *            · `f.print(name)`  → name.txt 의 내용 (LittleFS)
 *            · `jsonEscape()`   → /api/config JSON (web_manager 가 출구에서 이스케이프)
 *            · `SystemSettings::font_name` = **String** (고정 버퍼 아님) → NVS putString
 *          중국어판 실기에서 `MFXuanRen_Noncommercial-Regular.ttf`(35바이트)가 이 상한에 걸려
 *          **조용히** 거부됐고 슬롯이 "Empty Slot"으로 보였다. 라이선스 접미사
 *          (`-Noncommercial-Regular` = 24바이트)가 붙으면 ASCII 이름도 금방 31을 넘는다.
 *          완전히 없애지는 않는다 — 이름은 JSON 응답과 드롭다운에 실리므로 무한정 받을
 *          이유가 없다. **늘리려면 이 한 곳만** 고치면 된다.
 */
#define FONT_NAME_MAX_LEN 64

#define LONG_PRESS_TIME_MS 1000
#define DEBOUNCE_TIME_MS 50
#define WIFI_CONFIG_TIMEOUT 120
#define DEBUG_MODE 1                             // 1: 디버그 정보 출력 활성화, 0: 비활성화

#endif
