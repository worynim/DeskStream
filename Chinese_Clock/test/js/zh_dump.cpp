// worynim@gmail.com
/**
 * @file zh_dump.cpp
 * @brief 펌웨어를 실제 호출해 JS 미러와 기계적으로 대조할 수 있는 행으로 덤프한다
 * @details ENG판 layout_dump.cpp / time_dump.cpp의 중국어판 통합본이다.
 *          중국어판은 시간 표현과 레이아웃이 같은 파일(chinese_time_core / layout_engine)에
 *          얽혀 있어 **한 덤퍼**로 두 축을 한 번에 낸다.
 *
 * @note [SYNC] 원본: ENG_Clock/test/js/layout_dump.cpp + time_dump.cpp
 *
 * 출력 형식: `kind \t a \t b \t v1 \t v2 \t v3` (값 없는 칸은 빈 문자열)
 *   hour     a=시(0~23)  b=문자판  v1=12H  v2=24H
 *   ms       a=분/초(0~59) b=문자판  v1=분    v2=초
 *   weekday  a=요일(0~6)  b=문자판  v1=요일
 *   daypart  a=시(0~23)   b=문자판  v1=오전/오후
 *   num      a=0~59       b=단위    v1="NN 분"  v2="NN 秒"
 *   numhour  a=시(0~23)   b=문자판  v1="NN 时"
 *   layout   a=0~59       b=문자판  v1=원문  v2=셀 직렬화
 *   layoutH  a=시(0~23)   b=문자판  v1=원문  v2=셀 직렬화
 *   plain    a=입력 인덱스 b=0      v1=원문  v2=셀 직렬화
 *   used     a=0          b=0      v1=펌웨어가 낼 수 있는 문자(정렬된 집합)
 *
 * @warning **깨진 UTF-8 입력은 여기 넣지 않는다.** JS 문자열 리터럴로는 표현할 수 없어
 *          JS↔C++ 대조가 구조적으로 불가능하다. 그 경로의 독점자는 test_layout.cpp §7이다.
 *
 * 빌드: run_all.sh가 담당. 직접 빌드하려면 §7의 주석 참고.
 */
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include "chinese_time_core.h"
#include "layout_engine.h"

/** 펌웨어가 만들어 낸 모든 표현에 등장한 문자의 누적 집합 (아래 dumpAll이 채운다) */
static std::set<std::string> g_usedChars;

/**
 * @brief 원문 한 줄에 등장하는 **글리프가 필요한** 문자를 누적한다
 * @details 공백은 넣지 않는다 — 숫자 모드 "NN 분"의 간격은 layoutLine()이 **빈 셀 한 칸**으로
 *          소비할 뿐 글리프를 요구하지 않는다. 넣으면 커버리지 검사가
 *          "공백 글자를 못 그린다"고 잘못 보고한다 (실제로 이 오탐이 한 번 났다).
 */
static void collectChars(const char* s) {
    for (const char* p = s; *p; ) {
        const unsigned char c = (unsigned char)*p;
        if ((c & 0xC0) == 0x80) { p++; continue; }   // 컨티뉴레이션 — 앞 문자에 붙었다
        const size_t len = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        if (!(len == 1 && c == ' ')) g_usedChars.insert(std::string(p, len));
        p += len;
    }
}

/** 한 행 출력 — 값 없는 칸은 빈 문자열 */
static void emit(const char* kind, int a, int b, const char* v1,
                 const char* v2 = "", const char* v3 = "") {
    printf("%s\t%d\t%d\t%s\t%s\t%s\n", kind, a, b, v1, v2, v3);
}

/**
 * @brief layoutLine()의 셀들을 `글자@x|글자@x` 형태로 직렬화한다
 * @return "REJECT" 또는 셀 직렬화 문자열 (호출자가 emit에 넘길 버퍼에 쓴다)
 * @details 공백 셀은 **" @x"** 로 표기한다 — 원문이 아닌 리터럴 BLANK_CELL이라
 *          글자를 찍으면 구분이 안 된다. 문자는 `%.*s`로 잘라서 바이트 수가 달라도 안전하다.
 * @note layout_engine.cpp가 blank 셀에 주는 텍스트는 원본 밖의 1바이트 공백이다.
 */
static const char* layoutCells(const char* text, int textLen, char* out, size_t cap) {
    // 용량 4 = display_manager.cpp가 넘기는 LAYOUT_MAX_CHARS와 **같은 값**.
    //   더 넉넉하게(8) 주면 test_layout.cpp §12와 달리 "용량 초과" 경로가 증발해
    //   런타임과 다른 판정이 나올 수 있다. 기동 경로를 그대로 재현해야 한다.
    LayoutChar cells[LAYOUT_MAX_CHARS];
    int count = 0;
    out[0] = '\0';
    if (!layoutLine(text, textLen, defaultGeometry(), cells, LAYOUT_MAX_CHARS, 128, count)) {
        snprintf(out, cap, "REJECT");
        return out;
    }
    for (int i = 0; i < count; i++) {
        char one[32];
        if (cells[i].len == 1 && cells[i].text[0] == ' ')
            snprintf(one, sizeof one, " @%d", (int)cells[i].x);
        else
            snprintf(one, sizeof one, "%.*s@%d", (int)cells[i].len, cells[i].text, (int)cells[i].x);
        if (i) strncat(out, "|", cap - strlen(out) - 1);
        strncat(out, one, cap - strlen(out) - 1);
    }
    return out;
}

/** numeric 모드 표현 "NN 단위"를 만든다 (두Digit() + 공백 + 단위) */
static void numericWith(const char* digits, const char* unit, char* out, size_t cap) {
    snprintf(out, cap, "%s %s", digits, unit);
}

/**
 * @brief 원문+셀 직렬화 두 값을 한 행으로 낸다
 * @param collect 문자 집합에도 이 원문을 넣을지 (펌웨어가 **생성**한 표현만 true)
 * @details `plain` 케이스는 손으로 만든 입력이라 글리프가 필요한 표현이 아니다.
 *          거기까지 섞으면 라틴 문자 A~F가 "펌웨어가 요구하는 문자"로 잡혀
 *          문자집합 커버리지 검사(가장 중요한 검사)의 신호가 흐려진다.
 */
static void emitLayout(const char* kind, int a, int sc, const char* text, bool collect) {
    char cells[512];
    layoutCells(text, (int)strlen(text), cells, sizeof cells);
    emit(kind, a, sc, text, cells);
    if (collect) collectChars(text);
}

/** 시·분·초·요일·오전오후 (§3.1~3.5) — 문자 표현 자체를 대조한다 */
static void dumpWordExprs(int sc, chtime::Script s) {
    char buf[CHT_BUF_SIZE], alt[CHT_BUF_SIZE];
    for (int h = 0; h < 24; h++) {
        chtime::hourToChars(h, false, s, buf, sizeof buf);
        chtime::hourToChars(h, true,  s, alt, sizeof alt);
        emit("hour", h, sc, buf, alt);
        collectChars(buf); collectChars(alt);
    }
    for (int m = 0; m < 60; m++) {
        chtime::minuteToChars(m, s, buf, sizeof buf);
        chtime::secondToChars(m, s, alt, sizeof alt);
        emit("ms", m, sc, buf, alt);
        collectChars(buf); collectChars(alt);
    }
    for (int d = 0; d < 7; d++) {
        chtime::weekdayToChars(d, s, buf, sizeof buf);
        emit("weekday", d, sc, buf);
        collectChars(buf);
    }
    for (int h = 0; h < 24; h++) {
        chtime::dayPartToChars(h, s, buf, sizeof buf);
        emit("daypart", h, sc, buf);
        collectChars(buf);
    }
}

/** 숫자 모드 "NN 단위" (§3.6) — 시 단위만 간체/번체가 갈린다 */
static void dumpNumeric(int sc) {
    char buf[CHT_BUF_SIZE], built[CHT_BUF_SIZE];
    const char* unitHr = (sc == 1) ? "時" : "时";
    for (int h = 0; h < 24; h++) {
        chtime::twoDigit(h, buf, sizeof buf);
        numericWith(buf, unitHr, built, sizeof built);
        emit("numhour", h, sc, built);
        collectChars(built);
    }
    // 분/초 단위는 두 문자판이 같다 — 키만 분리해 같은 "NN"을 두 번 낸다
    for (int n = 0; n < 60; n++) {
        chtime::twoDigit(n, buf, sizeof buf);
        numericWith(buf, "分", built, sizeof built);
        emit("num", n, sc, built);
        collectChars(built);
        numericWith(buf, "秒", built, sizeof built);
        emit("num", n, sc + 2, built);      // 단위 번체는 동일 → 키만 분리
        collectChars(built);
    }
}

/** 레이아웃 축 — 표현을 실제 layoutLine()에 넣어 셀 x 좌표까지 대조한다 */
static void dumpLayouts(int sc, chtime::Script s) {
    char buf[CHT_BUF_SIZE], built[CHT_BUF_SIZE];
    for (int n = 0; n < 60; n++) {
        chtime::twoDigit(n, buf, sizeof buf);
        numericWith(buf, "分", built, sizeof built);
        emitLayout("layout", n, sc, built, true);
    }
    for (int h = 0; h < 24; h++) {
        chtime::hourToChars(h, true, s, buf, sizeof buf);
        emitLayout("layoutH", h, sc, buf, true);
    }
}

/** JS 문자열 리터럴로 표현 가능한 입력 — 깨진 바이트는 test_layout.cpp §7이 맡는다 */
static void dumpPlainCases(void) {
    const char* plain[] = { " 两点", "两点 ", "13  时", "    ", "   两点",
                            "ABCDE", "ABCDEF", "", "十三时", "星期日 下午" };
    for (int i = 0; i < (int)(sizeof plain / sizeof plain[0]); i++)
        emitLayout("plain", i, 0, plain[i], false);
}

/** §3 전 표현을 둘 다 문자판으로 훑는다 */
static void dumpAll(void) {
    for (int sc = 0; sc < 2; sc++) {
        const chtime::Script s = (sc == 1) ? chtime::CHT_SCRIPT_TRADITIONAL
                                           : chtime::CHT_SCRIPT_SIMPLIFIED;
        dumpWordExprs(sc, s);
        dumpNumeric(sc);
        dumpLayouts(sc, s);
    }
    dumpPlainCases();
}

/** 펌웨어가 실제로 낼 수 있는 문자 집합을 한 행으로 낸다 (구분자 0x1f) */
static void dumpUsedChars(void) {
    std::string all;
    for (const std::string& c : g_usedChars) {
        if (!all.empty()) all += '\x1f';
        all += c;
    }
    emit("used", 0, 0, all.c_str());
}

int main() {
    dumpAll();
    dumpUsedChars();
    return 0;
}