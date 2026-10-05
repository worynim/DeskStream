// worynim@gmail.com
/**
 * @file chinese_time_core.h
 * @brief 시간 → 중국어 텍스트 변환 순수 로직 (Arduino 의존성 없음)
 * @details 네이티브 단위 테스트를 위해 Arduino/String을 사용하지 않는다.
 *          모든 함수는 호출자가 제공한 버퍼에 결과를 쓰고, 실패 시 빈 문자열을 남긴다.
 *
 * @note [SYNC] 구조는 ENG_Clock/english_time_core.h를 따른다 (버퍼 계약·false 반환 규약 동일).
 *       표현 규칙과 문자집합은 PLAN.md §3이 유일한 기준이다.
 *
 * @note **표준형(标准型)만 채택한다** (PLAN §2.2). 구어체(`四点差一刻`)와 시진(`申时`)은
 *       기각했다 — 구어체는 `差` 생략 시 뜻이 뒤집히고, 시진은 2시간 단위라 초 단위를 잃는다.
 *
 * @note **번체는 3글자만 다르다** (`点`/`點`, `时`/`時`, `两`/`兩`). 전체 문자열을 2벌로 두지 않고
 *       치환 표 하나로 처리하므로, 치환 규칙이 어긋날 지점이 코드 전체에 단 하나뿐이다.
 */
#ifndef CHINESE_TIME_CORE_H
#define CHINESE_TIME_CORE_H

#include <stddef.h>

/** 결과 버퍼 권장 크기. 최장 표현 "四十五分"(4자 × 3바이트) + NUL 여유 포함. */
#define CHT_BUF_SIZE 16

namespace chtime {

/**
 * @brief 문자판: 간체(简体) / 번체(繁體)
 * @note 열거자 이름에 **CHT_ 접두사**를 붙인 이유:
 *       config.h가 `#define SCRIPT_SIMPLIFIED 0`을 정의하는데, 이 모듈은
 *       Arduino/config.h를 include하지 않는다. include한다면 매크로가 열거자 이름을
 *       치환해 `enum Script { 0 = 0, 1 = 1 }`이 되어 컴파일이 깨진다.
 *       접두사는 그 충돌을 막고, 동시에 이 모듈이 **네이티브 g++ 테스트로 검증된다**는
 *       사실을 보존한다 (매크로에 지면 테스트가 config.h까지 끌어와야 한다).
 */
enum Script { CHT_SCRIPT_SIMPLIFIED = 0, CHT_SCRIPT_TRADITIONAL = 1 };

/** 시 유효 범위 (0~23) */
bool isValidHour24(int n);

/** 12시간제 시 유효 범위 (1~12) */
bool isValidHour12(int n);

/** 분/초 유효 범위 (0~59) */
bool isValidMinute(int n);

/**
 * @brief 0~59를 한자 수사 문자열로 변환 ("四十五" / "五" / "五十")
 * @param n 0~59 (범위 밖이면 false)
 * @note 30은 여기서 "三十"이다. `半` 치환은 minuteToChars()/secondToChars()가 담당한다.
 */
bool numberToChars(int n, Script s, char* buf, size_t cap);

/**
 * @brief 시를 한자로 변환 ("三点" / "四十五分"의 앞부분 / "兩點")
 * @param hour  0~23
 * @param is24h true면 24시간제(0→"零点"), false면 12시간제(0→"十二点")
 * @note 12시간제에서 0시(=자정)는 12시로 센다 (`h = hour % 12`, 0이면 12).
 */
bool hourToChars(int hour, bool is24h, Script s, char* buf, size_t cap);

/**
 * @brief 분을 한자로 변환 ("四十五分" / "零五分" / "整" / "半")
 * @note 규칙 ③ 10분 미만은 십의 자리에 `零`을 넣는다. 3:05가 "三点五分"(오독)되지 않게 한다.
 * @note 규칙 ④ 30분은 정확히 `半`만 쓴다. 31분은 "三十一分"으로 되돌아간다.
 */
bool minuteToChars(int minute, Script s, char* buf, size_t cap);

/**
 * @brief 초를 한자로 변환 (minuteToChars와 완전히 대칭 — 끝 글자만 `分`→`秒`)
 */
bool secondToChars(int second, Script s, char* buf, size_t cap);

/**
 * @brief 요일 번호를 "星期X"로 변환
 * @param day 0=일요일 ~ 6=토요일 (tm_wday 규약)
 * @note 최장 "星期日" = 3자 — 32px 셀 4자 한계 안에 넉넉히 들어간다.
 */
bool weekdayToChars(int day, Script s, char* buf, size_t cap);

/**
 * @brief 오전/오후를 "上午"/"下午"로 변환
 * @param hour24 0~23. 0~11은 上午, 12~23은 下午.
 * @note 자정(0시)이 `上午`인 것은 중국 실무 관행과 같으므로 별도 예외를 두지 않는다.
 */
bool dayPartToChars(int hour24, Script s, char* buf, size_t cap);

/** @brief 0~59를 2자리 문자열로 변환 ("09") */
bool twoDigit(int n, char* buf, size_t cap);

/**
 * @brief 해당 문자판에서만 쓰이는 글자 개수 (치환표 크기 = 3, 두 문자판 모두)
 * @details scriptOnlyGlyph()와 짝을 이룬다. 왜 세는가:
 *          슬롯 폰트가 그 글자들을 갖고 있지 않으면 해당 문자판으로 전환해도 빈칸이 뜬다.
 *          치환표를 코어 밖에서 다시 적지 않고 여기서 꺼내 쓴다.
 * @note 간체에만 있는 3글자(点·时·两)와 번체에만 있는 3글자(點·時·兩) 모두 **3**이다.
 */
size_t scriptOnlyCount(Script s);

/**
 * @brief 해당 문자판에서만 쓰이는 글자를 i번째부터
 * @param s 문자판
 * @param i 0 이상 scriptOnlyCount(s) 미만
 * @return "點"/"时" 등. 범위 밖이면 nullptr
 * @note 순서는 치환표 순서이며 **표현 순서에 의존하지 않는다** — 존재 여부만 물을 때 쓴다.
 */
const char* scriptOnlyGlyph(Script s, size_t i);

} // namespace chtime

#endif