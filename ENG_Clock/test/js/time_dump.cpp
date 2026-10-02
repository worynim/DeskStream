// worynim@gmail.com
/**
 * @file time_dump.cpp
 * @brief english_time_core의 전수 결과를 `태그\t성공여부\t값`으로 출력한다 (테스트용)
 * @details time_crosscheck.mjs가 이 출력 전체를 JS 결과와 줄 단위로 비교한다.
 *          태그를 argv로 받는 대신 한 번에 전부 뽑아, 프로세스 기동을 1회로 줄인다.
 *
 * @note 펌웨어와 같은 버퍼 규약을 쓴다: 실패 시 빈 문자열, 용량은 ENG_BUF_SIZE.
 *       12시간제 시 래핑 규칙은 english_time.cpp(Arduino 래퍼)를 그대로 따른다.
 */
#include <cstdio>
#include <string>
#include "english_time_core.h"

namespace {

/** 단일 정수 함수의 결과를 출력 포맷으로 반환 */
std::string line(const std::string& tag, bool ok, const char* buf) {
    return tag + "\t" + (ok ? "OK" : "FAIL") + "\t" + buf + "\n";
}

std::string unary(const std::string& tag, bool (*fn)(int, char*, size_t), int arg) {
    char buf[ENG_BUF_SIZE];
    bool ok = fn(arg, buf, sizeof(buf));
    return line(tag, ok, buf);
}

/** english_time.cpp getNumericDay(): 제거됨 — 숫자 모드도 요일을 영문으로 표시한다 (수정할 사항 3) */

std::string hours() {
    std::string s;
    for (int h = 0; h < 24; h++) {
        for (int k = 0; k < 2; k++) {          // k=0: 12시간제, k=1: 24시간제
            char buf[ENG_BUF_SIZE];
            bool ok = engtime::hourToWords(h, k == 1, buf, sizeof(buf));
            s += line("HOUR_" + std::to_string(h) + "_" + std::to_string(k), ok, buf);
            // english_time.cpp getNumericHour()의 12시간제 래핑 규칙
            int x = (k == 1) ? h : (h % 12 == 0 ? 12 : h % 12);
            char nbuf[ENG_BUF_SIZE];
            bool nok = engtime::twoDigit(x, nbuf, sizeof(nbuf));
            s += line("NHOUR_" + std::to_string(h) + "_" + std::to_string(k), nok, nbuf);
        }
        s += unary("AMPM_" + std::to_string(h), engtime::amPm, h);
    }
    return s;
}

std::string minutesAndSeconds() {
    std::string s;
    for (int n = 0; n < 60; n++) {
        std::string t = std::to_string(n);
        s += unary("MIN_" + t, engtime::minuteToWords, n);
        s += unary("SEC_" + t, engtime::secondToWords, n);
        s += unary("NMIN_" + t, engtime::twoDigit, n);
    }
    return s;
}

std::string days() {
    std::string s;
    for (int d = 0; d < 7; d++) s += unary("DAY_" + std::to_string(d), engtime::dayName, d);
    return s;
}

/** [수정할 사항 3] 날짜 — 월 1~12 × 일 {1,2,9,10,12,15,21,25,28,30,31} × 순서 2 + 범위 밖 */
std::string dates() {
    const int mdays[] = { 1, 2, 9, 10, 12, 15, 21, 25, 28, 30, 31 };
    std::string s;
    for (int mon = 1; mon <= 12; mon++) {
        for (int md : mdays) {
            for (int k = 0; k < 2; k++) {          // k=0: 일/월, k=1: 월/일
                char buf[ENG_BUF_SIZE];
                bool ok = engtime::dateString(mon, md, k == 1, buf, sizeof(buf));
                s += line("DATE_" + std::to_string(mon) + "_" + std::to_string(md) + "_" + std::to_string(k),
                          ok, buf);
            }
        }
    }
    for (int mon : { 0, 13, -1 }) {
        for (int md : { 0, 32, -1 }) {
            char buf[ENG_BUF_SIZE];
            bool ok = engtime::dateString(mon, md, false, buf, sizeof(buf));
            s += line("BADDATE_" + std::to_string(mon) + "_" + std::to_string(md), ok, buf);
        }
    }
    return s;
}

/** 범위 밖 입력 — 빈 문자열을 남겨야 하는 방어 경로 */
std::string outOfRange() {
    const int bad[] = { -1, 60, 999 };
    std::string s;
    for (int n : bad) {
        std::string t = std::to_string(n);
        s += unary("BAD_n2w_" + t, engtime::numberToWords, n);
        s += unary("BAD_min_" + t, engtime::minuteToWords, n);
        s += unary("BAD_day_" + t, engtime::dayName, n);
    }
    return s;
}

} // namespace

int main() {
    std::string out = hours() + minutesAndSeconds() + days() + dates() + outOfRange();
    fwrite(out.data(), 1, out.size(), stdout);
    return 0;
}