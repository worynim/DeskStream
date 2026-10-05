// worynim@gmail.com
/**
 * @file test_layout.cpp
 * @brief 단일 줄 레이아웃 검증
 * @details 빌드: g++ -std=c++11 -Wall -Wextra -I.. test_layout.cpp layout_engine.cpp \
 *                renderer_geometry.cpp -o /tmp/test_layout
 *          실행: /tmp/test_layout
 *
 * @note 이 테스트는 PLAN §6.4의 배치 규칙 5개를 **수치로 고정**한다.
 *       좌표는 웹 Font Studio / JS 미러와 공유하는 값이므로 어긋나면 브라우저
 *       미리보기와 실제 OLED가 다른 위치에 그린다 (조용한 불일치).
 */
#include <cstdio>
#include <cstring>
#include "layout_engine.h"
#include "chinese_time_core.h"   // §3 전 표현을 실제로 만들어 순회하기 위해

// §3 전수 순회 — 순회 함수 정의는 파일 끝(아래)에 있다.
//   main()이 먼저 선언해야 호출할 수 있다. (순회 코드가 100줄이라 앞쪽에 두면
//   배치 규칙 검증(파일의 본지)을 읽는 사람이 지루워진다)
//
//   실패 시 **첫 실패 표현**을 기억한다 — "몇 개 실패"만으로는 원인을 고칠 수 없다.
static int   g_sweepFailures = 0;
static int   g_sweepChecked = 0;
static int   g_sweepMaxCells = 0;
static char  g_sweepFirstFail[64] = {0};
static void sweepAllExpressions(const CellGeometry& g, int screenW);

static int g_pass = 0;
static int g_fail = 0;

static void checkBool(const char* what, bool got, bool want) {
    if (got == want) { g_pass++; return; }
    g_fail++;
    printf("  FAIL  %-40s got %s, want %s\n", what, got ? "true" : "false", want ? "true" : "false");
}

static void checkInt(const char* what, int got, int want) {
    if (got == want) { g_pass++; return; }
    g_fail++;
    printf("  FAIL  %-40s got %d, want %d\n", what, got, want);
}

/** layoutLine을 한 번 돌려 ok/outCount를 돌려주는 헬퍼 */
static bool run(const char* text, int textLen, const CellGeometry& g,
                LayoutChar* out, int cap, int screenW, int& count) {
    return layoutLine(text, textLen, g, out, cap, screenW, count);
}

/** 셀 하나의 x 좌표와 UTF-8 길이를 비교 */
static void checkCell(const char* what, const LayoutChar& c, int wantX, int wantLen) {
    if ((int)c.x == wantX && (int)c.len == wantLen) { g_pass++; return; }
    g_fail++;
    printf("  FAIL  %-40s got x=%d len=%u, want x=%d len=%d\n",
           what, (int)c.x, (unsigned)c.len, wantX, wantLen);
}

/** 셀의 text가 원본의 어느 위치에서 시작하는지 (부분 문자열이므로 주소로 확인) */
static void checkTextPtr(const char* what, const LayoutChar& c, const char* base, int offset) {
    if (c.text == base + offset) { g_pass++; return; }
    g_fail++;
    printf("  FAIL  %-40s text가 base+%d가 아니다\n", what, offset);
}

/**
 * @brief 빈 셀이 원본 밖의 리터럴(길이 1의 공백)을 가리키는지
 * @details 주소를 직접 비교하면 안 된다 — 컴파일러가 같은 리터럴을 합치면
 *          BLANK_CELL과 주소가 같아져 검사가 무의미해진다. 원본 [base, base+len)
 *          **밖**의 1바이트 공백인지로 판정한다.
 */
static void checkBlank(const char* what, const LayoutChar& c, const char* base, int len) {
    const bool outside = (c.text < base) || (c.text >= base + len);
    if (outside && c.text && c.text[0] == ' ' && c.len == 1) { g_pass++; return; }
    g_fail++;
    printf("  FAIL  %-40s 원본 밖의 1바이트 공백이 아니다\n", what);
}

int main() {
    const CellGeometry& g = defaultGeometry();   // {32, 48, 6, 48, -8, 4}
    const int W = 128;
    LayoutChar cells[LAYOUT_MAX_CHARS + 2];   // 용량 초과 경계용으로 2칸 여유
    int count = -1;

    // === 1. 셀 수별 시작 좌표 (가로 중앙 정렬) ===
    // 128 − n×32 가 항상 32의 배수라 나머지가 없다 — startX는 정수다.
    {
        checkInt("0자 → 셀 0개", (run("", 0, g, cells, 6, W, count), count), 0);
        checkBool("0자 → 성공", count == 0 ? true : false, true);

        // "十" — 3바이트 한자 1자
        const char* yi = "十";
        checkBool("1자 배치 성공", run(yi, 3, g, cells, 6, W, count), true);
        checkInt("1자 → 셀 1개", count, 1);
        checkCell("1자 → x=48", cells[0], 48, 3);
        checkTextPtr("1자 → 원본 0바이트부터", cells[0], yi, 0);

        // "三点" — 2자
        const char* san = "三点";
        checkBool("2자 배치 성공", run(san, 6, g, cells, 6, W, count), true);
        checkInt("2자 → 셀 2개", count, 2);
        checkCell("2자[0] → x=32", cells[0], 32, 3);
        checkCell("2자[1] → x=64", cells[1], 64, 3);
        checkTextPtr("2자[1] → 원본 3바이트부터", cells[1], san, 3);

        // "三点零五分"은 5자라 들어가지 않는다(PLAN §3.6 최장 표현은 4자).
        // 여기서는 4자를 라틴 2자 + 한자 2자로 구성해 좌표와 바이트 길이를 함께 본다.
        const char* four = "AB中文";   // 1 + 1 + 3 + 3 = 8바이트
        checkBool("4자 배치 성공", run(four, 8, g, cells, 6, W, count), true);
        checkInt("4자 → 셀 4개", count, 4);
        checkCell("4자[0] → x=0", cells[0], 0, 1);
        checkCell("4자[1] → x=32", cells[1], 32, 1);
        checkCell("4자[2] → x=64", cells[2], 64, 3);
        checkCell("4자[3] → x=96", cells[3], 96, 3);
    }

    // === 2. 5자 이상은 잘림(반쪽 표현 금지) ===
    {
        const char* five = "ABCDE";
        checkBool("5자 → false", run(five, 5, g, cells, 6, W, count), false);
        const char* six = "ABCDEF";
        checkBool("6자 → false", run(six, 6, g, cells, 6, W, count), false);
    }

    // === 3. 숫자 모드 "13 时" — 숫자 2 + 빈 칸 1 + 단위 1 = 4셀 = 128px ===
    // (이게 §6.4 표에서 "공백 셀 배치 ✅ 유지"로 남겨 둔 유일한 이유다)
    {
        const char* t = "13 \xE6\x97\xB9";   // "13 时"
        checkBool("\"13 时\" 배치 성공", run(t, 6, g, cells, 6, W, count), true);
        checkInt("\"13 时\" → 셀 4개", count, 4);
        checkCell("\"13 时\"[0] 1", cells[0], 0, 1);
        checkCell("\"13 时\"[1] 3", cells[1], 32, 1);
        checkCell("\"13 时\"[2] 빈 칸", cells[2], 64, 1);
        checkCell("\"13 时\"[3] 时", cells[3], 96, 3);
        // 빈 칸은 원본의 공백이 아니라 리터럴을 가리킨다 (수명 안전).
        checkBlank("\"13 时\"[2] → 원본 밖 리터럴 blank", cells[2], t, 6);
        checkTextPtr("\"13 时\"[3] → 원본 3바이트부터", cells[3], t, 3);
    }

    // === 4. 공백 규칙: 앞/뒤 버림, 복수 공백 합치기 ===
    {
        const char* lead = " 两点";              // 앞 공백 1칸
        checkBool("앞 공백 배치 성공", run(lead, 7, g, cells, 6, W, count), true);
        checkInt("앞 공백 → 무시되어 2셀", count, 2);
        checkCell("앞 공백 → 첫 셀 x=32", cells[0], 32, 3);

        const char* tail = "两点 ";              // 뒤 공백 1칸
        checkBool("뒤 공백 배치 성공", run(tail, 7, g, cells, 6, W, count), true);
        checkInt("뒤 공백 → Phantom 칸 없음", count, 2);

        const char* dbl = "13  \xE6\x97\xB9";    // "13  时" (공백 2칸)
        checkBool("복수 공백 배치 성공", run(dbl, 7, g, cells, 6, W, count), true);
        checkInt("복수 공백 → 한 칸으로 합침", count, 4);

        const char* onlySpace = "    ";
        checkBool("공백만 배치 성공", run(onlySpace, 4, g, cells, 6, W, count), true);
        checkInt("공백만 → 셀 0개", count, 0);
    }

    // === 5. 용량 초과 ===
    {
        const char* two = "两点";
        checkBool("용량 1 < 셀 2 → false", run(two, 6, g, cells, 1, W, count), false);
        checkBool("용량 0 → false", run(two, 6, g, cells, 0, W, count), false);
        checkBool("용량 2 = 셀 2 → 성공", run(two, 6, g, cells, 2, W, count), true);
    }

    // === 6. 잘못된 인자 방어 ===
    {
        const char* two = "两点";
        checkBool("out nullptr → false", run(two, 6, g, nullptr, 6, W, count), false);
        checkBool("text nullptr + len>0 → true(빈 화면)", run(nullptr, 6, g, cells, 6, W, count), true);
        checkInt("  → 셀 0개", count, 0);
        checkBool("len 0 → true(빈 화면)", run(two, 0, g, cells, 6, W, count), true);
        checkInt("  → 셀 0개", count, 0);

        CellGeometry bad = g; bad.glyphW = 0;
        checkBool("glyphW 0 → false", run(two, 6, bad, cells, 6, W, count), false);
        CellGeometry bad2 = g; bad2.maxPerLine = 0;
        checkBool("maxPerLine 0 → false", run(two, 6, bad2, cells, 6, W, count), false);
    }

    // === 7. 깨진 입력을 빈 칸으로 (PLAN §6.4 규칙 4) ===
    // U8g2가 래스터 밖을 읽지 않도록, 그릴 수 없는 바이트는 그리지 않는다.
    {
        // (a) 문자열 도중의 NUL — 'A' 2개 사이에 끼워 넣는다
        const char nul[] = { 'A', 0x00, 'B', 'C', 0x00 };
        checkBool("NUL 중간 배치 성공", run(nul, 5, g, cells, 6, W, count), true);
        checkInt("NUL 중간 → A,빈칸,B,C = 4셀", count, 4);
        checkCell("NUL 중간[1] → 빈 칸", cells[1], 32, 1);

        // (b) 3바이트 한자의 잘림 — 리딩 바이트만 있고 나머지 2바이트가 없다
        const char trunc[] = { 'A', (char)0xE6, (char)0x97 };
        checkBool("한자 잘림 배치 성공", run(trunc, 3, g, cells, 6, W, count), true);
        checkInt("한자 잘림 → A + 빈 칸 = 2셀", count, 2);
        checkCell("한자 잘림[0] → A", cells[0], 32, 1);
        checkCell("한자 잘림[1] → 빈 칸(깨진 바이트 아님)", cells[1], 64, 1);

        // (c) 컨티뉴레이션 바이트가 리딩으로 등장 — 앞 문자의 尾巴
        const char cont[] = { 'A', (char)0x97, (char)0xB9, 'B', 0x00 };
        checkBool("컨티뉴레이션 배치 성공", run(cont, 5, g, cells, 6, W, count), true);
        checkInt("컨티뉴레이션 → A,빈칸,빈칸,B = 4셀", count, 4);

        // (d) 정의되지 않은 리딩 0xFF
        const char badLead[] = { 'A', (char)0xFF, 0x00 };
        checkBool("0xFF 배치 성공", run(badLead, 2, g, cells, 6, W, count), true);
        checkInt("0xFF → A + 빈 칸 = 2셀", count, 2);
    }

    // === 8. 선행 공백 앞의 빈 칸은 Phantom이 아니다 ===
    // 두 번 연속 공백이 맨 앞에 있으면 첫 글자 앞에는 아무 칸도 없어야 한다.
    {
        const char* lead = "   两点";
        checkBool("앞 공백 3칸 배치 성공", run(lead, 9, g, cells, 6, W, count), true);
        checkInt("앞 공백 3칸 → 2셀", count, 2);
        checkCell("앞 공백 3칸 → 첫 셀 x=32", cells[0], 32, 3);
    }

    // === 9. line 필드는 항상 0 ===
    {
        const char* two = "两点";
        run(two, 6, g, cells, 6, W, count);
        bool allZero = true;
        for (int i = 0; i < count; i++) if (cells[i].line != 0) allZero = false;
        checkBool("line 전부 0", allZero, true);
    }

    // === 10. layoutLineCount ===
    {
        LayoutChar out3[3];
        const char* two = "两点";
        int n = 0;
        run(two, 6, g, out3, 3, W, n);
        checkInt("layoutLineCount(line=0)", layoutLineCount(out3, n, 0), 2);
        checkInt("layoutLineCount(line=1) — 1줄 전용", layoutLineCount(out3, n, 1), 0);
        checkInt("layoutLineCount(count=0)", layoutLineCount(out3, 0, 0), 0);
        checkInt("layoutLineCount(nullptr)", layoutLineCount(nullptr, 5, 0), 0);
    }

    // === 11. 좌표가 화면을 벗어나지 않는 불변식 (전수) ===
    // 1~4자 조합을 전부 돌려 보고, 모든 셀이 [0, 128) 안에 있고
    // 오른쪽 끝이 화면을 넘지 않는지 확인한다.
    {
        bool insideScreen = true;
        bool fitsScreen = true;
        for (int n = 1; n <= 4; n++) {
            char buf[16];
            memset(buf, 'A', (size_t)n);
            buf[n] = 0x00;
            int c2 = 0;
            if (!run(buf, n, g, cells, 6, W, c2)) { insideScreen = false; break; }
            for (int i = 0; i < c2; i++) {
                if (cells[i].x < 0 || cells[i].x + g.glyphW > W) insideScreen = false;
            }
            if (cells[c2 - 1].x + g.glyphW > W) fitsScreen = false;
        }
        checkBool("모든 셀이 화면 안", insideScreen, true);
        checkBool("줄이 화면 폭을 넘지 않음", fitsScreen, true);
    }

    // === 12. §3 전 표현이 4셀 이내에 들어가는가 (전수 순회) ===
    //
    // [왜 이 검증이 가장 위험한 것을 잡는가]
    //   display_manager::layoutFor()는 layoutLine이 false를 주면 **빈 화면**을 그린다.
    //   즉 어떤 표현이 5셀이면 그 순간 그 화면이 통째로 꺼지고 Serial 로그만 남는다 —
    //   OLED를 보기 전까지 알아낼 방법이 없다.
    //   대표 문자열만 손으로 고른 검사는 최장 표현을 놓칠 수 있으므로,
    //   chinese_time_core가 만들어 낼 수 있는 전 표현을 **둘 다 문자판 × 12/24시간제로** 돌린다.
    {
        sweepAllExpressions(g, W);
        if (g_sweepFailures == 0) {
            printf("  · §3 전수 순회 %d개 표현 — 최대 %d셀 (한계 %d)\n",
                   g_sweepChecked, g_sweepMaxCells, (int)g.maxPerLine);
        }
        checkInt("§3 전 표현 중 4셀 초과 건수", g_sweepFailures, 0);
        if (g_sweepFailures > 0) {
            printf("  FAIL  첫 실패 표현: \"%s\"\n", g_sweepFirstFail);
        }
        checkBool("§3 전 표현이 한 번이라도 배치됨", g_sweepChecked > 0, true);
    }

    printf("test_layout: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

// ============================================================================
// §3 전 표현 전수 순회 (Step 7 완료 조건 "§3 전 표현이 4셀 이내 수용, 전수 순회 통과")
// ============================================================================

/**
 * @brief 전수 순회 실패 보고기
 * @details "몇 개 실패"만으로는 원인을 고칠 수 없다 — **어느 표현이 실패했는지**가 필요하다.
 *          첫 실패 표현만 저장한다(전부 저장하면 로그가 수백 줄이 된다).
 *          상태 변수는 파일 앞부분(§3 전수 순회 블록)에 선언되어 있다.
 */

/**
 * @brief 표현 하나를 배치해 4셀 이내인지 확인한다
 * @details 실패 조건은 layoutLine이 false를 주는 것이다 — 셀 수가 4를 초과했다는 뜻이고,
 *          그 순간 display_manager는 **빈 화면**을 그린다.
 * @note 용량 8로 넉넉히 준다. LAYOUT_MAX_CHARS(4)로 그대로 주면
 *       "5셀이라 잘림"과 "4슬롯뿐이라 용량 초과"가 구분되지 않아
 *       어느 쪽 규칙이 걸린 건지 알 수 없다.
 */
static void sweepOne(const char* s, size_t len, const CellGeometry& g, int screenW) {
    LayoutChar cells[8];
    int count = 0;
    g_sweepChecked++;
    if (!layoutLine(s, (int)len, g, cells, 8, screenW, count)) {
        g_sweepFailures++;
        if (!g_sweepFirstFail[0]) snprintf(g_sweepFirstFail, sizeof(g_sweepFirstFail), "%s", s);
        return;
    }
    if (count > g_sweepMaxCells) g_sweepMaxCells = count;
}

/**
 * @brief §3이 정한 모든 시각 표현을 배치해 최장 표현의 셀 수를 확인한다
 * @param g    셀 기하
 * @param screenW 화면 폭
 * @details 훑는 대상 (PLAN §3 전수표와 1:1):
 *          - 시   0~23 × {12H, 24H} × {간체, 번체} = 96
 *          - 분   0~59 × {간체, 번체}             = 120
 *          - 초   0~59 × {간체, 번체}             = 120
 *          - 요일 0~6  × {간체, 번체}             = 14
 *          - 오전/오후 0~23 × {간체, 번체}        = 46
 *          - 숫자 모드 "13 时" / "05 分" / "09 秒" = 12
 * @note 숫자 모드도 반드시 포함한다. chinese_time.cpp의 withUnit()가 붙이는
 *       "공백 1칸 + 단위 1자"가 셀 2개를 더 먹으므로, 2자리 숫자(2셀) + 2셀 = 4로
 *       **정확히 한계**다. 여유가 0이므로 여기서 걸리지 않으면 그때는 통과가 맞다.
 */
static void sweepAllExpressions(const CellGeometry& g, int screenW) {
    char buf[CHT_BUF_SIZE];
    const chtime::Script scripts[2] = { chtime::CHT_SCRIPT_SIMPLIFIED, chtime::CHT_SCRIPT_TRADITIONAL };

    for (int si = 0; si < 2; si++) {
        for (int h = 0; h < 24; h++) {
            if (chtime::hourToChars(h, false, scripts[si], buf, sizeof(buf)))
                sweepOne(buf, strlen(buf), g, screenW);
            if (chtime::hourToChars(h, true, scripts[si], buf, sizeof(buf)))
                sweepOne(buf, strlen(buf), g, screenW);
        }
        for (int m = 0; m < 60; m++) {
            if (chtime::minuteToChars(m, scripts[si], buf, sizeof(buf)))
                sweepOne(buf, strlen(buf), g, screenW);
            if (chtime::secondToChars(m, scripts[si], buf, sizeof(buf)))
                sweepOne(buf, strlen(buf), g, screenW);
        }
        for (int d = 0; d < 7; d++) {
            if (chtime::weekdayToChars(d, scripts[si], buf, sizeof(buf)))
                sweepOne(buf, strlen(buf), g, screenW);
        }
        for (int h = 0; h < 24; h++) {
            if (chtime::dayPartToChars(h, scripts[si], buf, sizeof(buf)))
                sweepOne(buf, strlen(buf), g, screenW);
        }
    }

    // 숫자 모드: twoDigit + " " + 단위. 단위는 한 자(3바이트)이므로 strlen으로 통째로 붙인다.
    //   "汉"/"漢" 자형이 아니라 **단위 문자열 배열**을 쓴다 — 1바이트만 복사하면
    //   깨진 멀티바이트가 되어 "4셀 한계" 검사가 무의미해진다(깨진 바이트도 1셀로 세기 때문).
    const char* unitsSimp[3] = { "时", "分", "秒" };
    const char* unitsTrad[3] = { "時", "分", "秒" };
    for (int n = 0; n < 60; n++) {
        if (!chtime::twoDigit(n, buf, sizeof(buf))) continue;
        size_t len = strlen(buf);
        buf[len++] = ' ';
        for (int si = 0; si < 2; si++) {
            for (int u = 0; u < 3; u++) {
                const char* unit = (si == 0) ? unitsSimp[u] : unitsTrad[u];
                const size_t ulen = strlen(unit);
                memcpy(buf + len, unit, ulen);
                buf[len + ulen] = '\0';
                sweepOne(buf, len + ulen, g, screenW);
            }
        }
    }
}