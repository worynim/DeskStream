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
import { layoutWrap, GLYPH_W, MAX_PER_LINE, SCREEN_W, UNIQ_CHARS } from './_extracted.mjs';

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
function jsRender(text) {
    const r = layoutWrap(text);
    return r.chars.map(d => `${d.line},${d.x},${d.y},`).join('') + `|${r.dropped === 0 ? 'OK' : 'TRUNC'}`;
}

/**
 * 펌웨어 layoutWrap()을 실제 바이너리로 호출한다.
 * 문자열은 표준 입력으로 전달한다 — argv는 공백과 빈 문자열을 손상시킨다.
 */
function cppRender(text) {
    return execFileSync(DUMPER, { input: text }).toString().trim();
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
    let fail = 0;
    for (const text of cases) {
        const js = jsRender(text), cpp = cppRender(text);
        if (js !== cpp) {
            fail++;
            console.error(`FAIL: "${text}"\n  JS: ${js}\n  C++: ${cpp}`);
        }
    }

    console.log(fail === 0
        ? `레이아웃 대조 통과: ${cases.length}건 (JS ↔ C++ layoutWrap)`
        : `레이아웃 대조 실패: ${fail}/${cases.length}건`);
    process.exit(fail === 0 ? 0 : 1);
}

main();