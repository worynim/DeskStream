// worynim@gmail.com
/**
 * @file layout_crosscheck.mjs
 * @brief 웹 페이지의 JS layoutWrap()가 펌웨어 layout_engine.cpp와 동일한지 전수 대조한다
 * @details 위험 #2 (JS/펌웨어 로직 이중화)의 실제 방어선이다.
 *          계획서_comment로만 막지 않고, 숫자로 대조해 어긋남을 즉시 드러낸다.
 *
 * @note [SYNC] 양쪽은 `MAX_PER_LINE`(9) · `GLYPH_W`(14) · `SCREEN_W`(128)를 공유한다.
 *             펌웨어 값은 layout_dump.cpp가 하드코딩 대신 헤더에서 읽는다.
 *
 * 전제: /tmp/eng_layout_dump 이 layout_dump.cpp로 빌드되어 있어야 한다.
 * 실행: run_all.sh 가 이 순서를 보장한다.
 */
import { readFileSync } from 'fs';
import { execFileSync } from 'child_process';
import { layoutWrap, GLYPH_W, MAX_PER_LINE, SCREEN_W, UNIQ_CHARS, previewInkByChar } from './_extracted.mjs';

const DUMPER = '/tmp/eng_layout_dump';

/** 펌웨어가 실제로 돌리는 모든 문자열: 시·분·초·요일·AM/PM + 합성 케이스 */
function buildCases() {
    const SMALL = ["ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE",
                   "TEN", "ELEVEN", "TWELVE", "THIRTEEN", "FOURTEEN", "FIFTEEN",
                   "SIXTEEN", "SEVENTEEN", "EIGHTEEN", "NINETEEN"];
    const TENS = ["", "", "TWENTY", "THIRTY", "FORTY", "FIFTY"];
    const DAYS = ["SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"];
    const n2w = (n) => n < SMALL.length ? SMALL[n]
        : (n % 10 === 0 ? TENS[Math.floor(n / 10)] : TENS[Math.floor(n / 10)] + " " + SMALL[n % 10]);
    const oclock = (n) => n === 0 ? "O'CLOCK" : n2w(n);

    const out = new Set();
    for (const h of [...Array(24).keys()]) {
        out.add(n2w(h % 12 === 0 ? 12 : h % 12));   // 12시간제 시
        out.add(n2w(h));                             // 24시간제 시
        out.add(h < 12 ? "AM" : "PM");
        for (let m = 0; m < 60; m++) {
            out.add(oclock(m)); out.add(n2w(m));
            for (let s = 0; s < 60; s++) { out.add(oclock(s)); out.add(n2w(s)); }
        }
    }
    DAYS.forEach(d => out.add(d));
    // [수정할 사항 3] 24H 첫 화면: "날짜 요일" (2줄) — 단어 모드·숫자 모드 전부
    for (let mon = 1; mon <= 12; mon++) {
        for (let day of [1, 2, 9, 10, 12, 15, 21, 25, 28, 30, 31]) {
            const dm = `${day}/${mon}`, md = `${mon}/${day}`;
            for (const d of DAYS) { out.add(`${dm} ${d}`); out.add(`${md} ${d}`); }
            for (let w = 1; w <= 7; w++) { out.add(`${dm} ${w}`); out.add(`${md} ${w}`); }
        }
    }
    // 경계 케이스: 2줄 용량을 넘기거나 어절 경계에 걸리는 문자열
    ["ONE THOUSAND", "A B C D E F G H I", "ABCDEFGHIJKLMNOP", "WEDNESDAY PM O'CLOCK",
     "TWENTY THREE", "FORTY FIVE", "FORTY ONE", "FIFTY NINE", "SEVENTEEN", "O'CLOCK"]
        .forEach(s => out.add(s));
    return [...out];
}

/** JS 결과를 펌웨어 출력 포맷(`line,x,y,...|OK`)으로 직렬화 */
function jsRender(text, inkWidth) {
    const r = layoutWrap(text, false, inkWidth);
    return r.chars.map(d => `${d.line},${d.x},${d.y},`).join('') + `|${r.dropped === 0 ? 'OK' : 'TRUNC'}`;
}

/**
 * [§6.16b] 글자별 잉크 표 — layout_dump.cpp `dummyInkOf()`와 **같은 규칙**이어야 한다.
 * @param {boolean} [on] false면 표를 비워 폰트 최대로 폴백시킨다(기존 경로 대조용)
 * @see layout_dump.cpp 의 "48px 폰트를 재현할 수는 없으므로…" 주석
 * @note ESM 임포트는 읽기 전용이라 **내용만** 바꾼다(웹의 refreshPreviewCache는
 *       객체를 통째로 교체하지만, 거긴 일반 스크립트라 가능한 방법이다).
 * @note **모든** 문자를 표에 넣어야 한다 — 비워 둔 문자는 inkOfChar()가
 *       previewInkWidth로 폴백하는데, 이 테스트에선 그 값이 0이라 C++(28)와 어긋난다.
 *       폴백 경로 자체는 on=false로 따로 대조한다.
 */
function useDummyInk(on) {
    for (const k of Object.keys(previewInkByChar)) delete previewInkByChar[k];
    if (!on) return;
    for (const ch of UNIQ_CHARS) previewInkByChar[ch] = 28;   // 기본값 (dummyInkOf의 return 28)
    Object.assign(previewInkByChar, {
        I: 18, J: 18,            // 좁은 글자
        W: 40, M: 40,            // 넓은 글자
        T: 32, V: 32,            // 중간 글자
        ' ': 0,                  // 공백은 그려지지 않는다
    });
}

/** 펌웨어 layoutWrap()을 실제 바이너리로 호출한다. */
function cppRender(text, inkWidth, perChar) {
    const args = [String(inkWidth)];
    if (perChar) args.push('1');
    return execFileSync(DUMPER, args, { input: text }).toString().trim();
}

function main() {
    // 기하 상수 자체가 어긋나 있으면 아래 전수 비교가 무의미하므로 먼저 확인한다.
    const expect = { GLYPH_W: 14, MAX_PER_LINE: 9, SCREEN_W: 128 };
    const actual = { GLYPH_W, MAX_PER_LINE, SCREEN_W };
    const geomFail = Object.keys(expect).filter(k => actual[k] !== expect[k]);
    if (geomFail.length > 0) {
        console.error(`FAIL: 기하 상수 불일치 — ${geomFail.map(k => `${k}=${actual[k]}(기대 ${expect[k]})`).join(', ')}`);
        process.exit(1);
    }

    // UNIQ_CHARS가 실제로 클럭이 요구하는 문자 집합을 덮는지
    // (아포스트로피 필수: O'CLOCK, 슬래시 필수: 날짜 "2/10")
    const needed = new Set("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'/");
    const missing = [...needed].filter(c => !UNIQ_CHARS.includes(c));
    if (missing.length > 0) {
        console.error(`FAIL: UNIQ_CHARS에 필수 문자 없음 — ${missing.join(', ')}`);
        process.exit(1);
    }

    const cases = buildCases();
    // [§6.16] 잉크 폭별로 전수 대조한다.
    //   0  — 캐시 미적용/폰트 미로드. 기존 고정 피치 (회귀 0이어야 함)
    //   11 — 작은 폰트(≈12px). 잉크가 셀 안이라 **확장이 없어야 한다** (회귀 0)
    //   14 — 확장 경계. glyphW와 같아서 여전히 확장 없음 (회귀 0)
    //   15 — 경계 바로 위. 처음으로 확장이 시작된다
    //   24 — 중간(≈26px). 5자 이하에서 겹침이 풀린다
    //   43 — 최대(≈48px). 2자만 화면 안에 들어온다
    const INK_WIDTHS = [0, 11, 14, 15, 24, 43];

    let fail = 0, total = 0;
    // [§6.16b] 두 경로를 모두 대조한다.
    //   off — 표가 비어 있어 **폰트 최대로 폴백**한다(§6.16b 이전 동작).
    //   on  — 글자별 표를 써서 **줄마다 다른 상한**이 걸린다(실제 기기 동작).
    // 둘이 같으면 줄별 잉크가 전혀 쓰이지 않는 것이고, 다른데 통과하면 미러가 어긋난다.
    for (const perChar of [false, true]) {
        useDummyInk(perChar);
        for (const ink of INK_WIDTHS) {
            let inkFail = 0;
            for (const text of cases) {
                total++;
                const js = jsRender(text, ink), cpp = cppRender(text, ink, perChar);
                if (js !== cpp) {
                    inkFail++; fail++;
                    if (inkFail <= 5) {
                        console.error(`FAIL(ink=${ink}, perChar=${perChar}): "${text}"\n  JS: ${js}\n  C++: ${cpp}`);
                    }
                }
            }
            const mode = perChar ? '§6.16b 표 ON ' : '폰트최대 폴백';
            console.log(`  ${mode} ink=${String(ink).padStart(2)}: ${cases.length}건 ${inkFail === 0 ? '통과' : `→ ${inkFail}건 실패`}`);
        }
    }

    console.log(fail === 0
        ? `레이아웃 대조 통과: ${total}건 (${cases.length}문자열 × 잉크폭 ${INK_WIDTHS.length}종 × 표 ON/OFF, JS ↔ C++ layoutWrap)`
        : `레이아웃 대조 실패: ${fail}/${total}건`);
    process.exit(fail === 0 ? 0 : 1);
}

main();