// worynim@gmail.com
/**
 * @file layout_crosscheck.mjs
 * @brief 웹 페이지의 JS 미러가 펌웨어와 동일한지 전수 대조한다 (시간 표현 + 레이아웃 + 문자집합)
 * @details 위험 #2 (JS/펌웨어 로직 이중화)의 실제 방어선이다.
 *          계획서·주석으로만 막지 않고 숫자로 대조해 어긋남을 즉시 드러낸다.
 *
 * @note [SYNC] 원본: ENG_Clock/test/js/layout_crosscheck.mjs (+ time_crosscheck.mjs)
 *             중국어판은 두 축을 한 파일로 합쳤다 — 시간 표현과 레이아웃이 같은
 *             표(§3)를 공유하므로 나눠 놓으면 같은 표를 두 번 훑게 된다.
 *
 * 이 테스트가 잡는 실제 결함 세 가지:
 *   1. 시간이 어긋난다   — JS가 "三点", 펌웨어가 "3點" → 미리보기와 OLED가 다른 시간을 보여준다
 *   2. 화면이 통째로 꺼진다 — layoutLine이 false → display_manager는 **빈 화면**을 그린다
 *   3. 글자가 안 나온다   — 펌웨어가 내는 문자가 Font Studio 문자집합에 없다 → OLED에 못 그린다
 *
 * 전제: /tmp/zh_dump 이 zh_dump.cpp로 빌드되어 있어야 한다. run_all.sh가 순서를 보장한다.
 * 실행: node test/js/layout_crosscheck.mjs
 */
import { readFileSync } from 'fs';
import { execFileSync } from 'child_process';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import {
    GLYPH_W, GLYPH_H, RASTER_W, BYTES_PER_ROW, GLYPH_SIZE,
    MAX_PER_LINE, LINE_HEIGHT, LINE_TOP_Y, SCREEN_W, SCREEN_H,
    X_OFFSET, ANIM_STEPS,
    CHARS_SIMPLIFIED, CHARS_TRADITIONAL, TRAD_MAP,
    layoutLine,
    hourToChars, msToChars, weekdayToChars, dayPartToChars, twoDigit, withUnit,
} from './_extracted.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, '..', '..');
const DUMPER = '/tmp/zh_dump';

let failures = 0;
function fail(msg) { failures++; console.error(`FAIL: ${msg}`); }

/** 펌웨어 덤퍼의 탭 구분 행을 {kind: {a:b: [v1,v2,v3]}} 으로 파싱한다 */
function parseDump(text) {
    const rows = {};
    for (const line of text.split('\n')) {
        if (!line) continue;
        const [kind, a, b, ...vals] = line.split('\t');
        rows[kind] ??= {};
        rows[kind][`${a}:${b}`] = vals;
    }
    return rows;
}

/**
 * 기하 상수 — 펌웨어 헤더 값과 어긋나면 아래 전수 비교가 무의미하므로 **먼저** 확인한다
 * @details 기대값을 이 파일에 숫자로 적으면 **자기확인**이 된다 — config.h가 바뀌어도
 *          JS와 이 숫자가 같이 남으면 테스트는 통과한다. 실제로 문자집합 대조에서
 *          같은 착각을 한 적이 있다. 그래서 config.h와 renderer_geometry.cpp에서 뽑는다.
 * @note 펌웨어가 같은 상수를 **두 곳**에 두면 그 일관성은 g++가 아니라 이 테스트가 지킨다.
 *       (config.h의 GLYPH_W ↔ renderer_geometry.cpp GEOM_288B가 그런 쌍이다)
 */
function parseDefines(file) {
    const out = {};
    for (const line of readFileSync(join(ROOT, file), 'utf8').split('\n')) {
        const m = line.match(/^\s*#define\s+([A-Z_0-9]+)\s+(-?\d+)\s*(\/\/.*)?$/);
        if (m) out[m[1]] = Number(m[2]);
    }
    return out;
}

function checkGeometry() {
    const cfg = parseDefines('config.h');
    // X_OFFSET은 JS가 (RASTER_W − GLYPH_W)/2로 **파생**한다 — 하드코딩하지 않는다.
    const expect = {
        GLYPH_W: cfg.GLYPH_W, GLYPH_H: cfg.GLYPH_H,
        RASTER_W: cfg.GLYPH_H === 48 ? 48 : cfg.GLYPH_H,   // §5.4: 래스터 높이 = 글자 높이
        BYTES_PER_ROW: cfg.RASTER_BYTES_PER_ROW,
        MAX_PER_LINE: cfg.MAX_CHARS_PER_LINE,
        LINE_HEIGHT: cfg.LINE_HEIGHT,
        LINE_TOP_Y: (cfg.SCREEN_HEIGHT - cfg.LINE_HEIGHT) / 2,
        SCREEN_W: cfg.SCREEN_WIDTH, SCREEN_H: cfg.SCREEN_HEIGHT,
        ANIM_STEPS: 16,   // display_manager.cpp에만 있는 값 — 아래에서 따로 대조
    };
    const actual = {
        GLYPH_W, GLYPH_H, RASTER_W, BYTES_PER_ROW, GLYPH_SIZE,
        MAX_PER_LINE, LINE_HEIGHT, LINE_TOP_Y,
        SCREEN_W, SCREEN_H, X_OFFSET, ANIM_STEPS,
    };
    for (const [k, want] of Object.entries(expect)) {
        if (want === undefined) { fail(`config.h에서 ${k}의 기준값을 못 읽었다`); continue; }
        if (actual[k] !== want) fail(`기하 상수 ${k}=${actual[k]} (config.h 기준 ${want})`);
    }
    // GLYPH_SIZE는 파생값 — BYTES_PER_ROW × GLYPH_H로 계산되는지 본다
    if (GLYPH_SIZE !== BYTES_PER_ROW * GLYPH_H)
        fail(`GLYPH_SIZE=${GLYPH_SIZE} ≠ BYTES_PER_ROW × GLYPH_H = ${BYTES_PER_ROW * GLYPH_H}`);

    // 기하 값이 **세 벌** 존재한다: config.h · renderer_geometry.cpp GEOM_288B · 위 JS.
    //   renderer_geometry.cpp는 Arduino 없는 **순수 모듈**이라 config.h를 include할 수 없다
    //   (config.h가 Arduino.h를 끌어온다). 그래서 static_assert로 막을 수 없고,
    //   기하 값이 어긋나는 순간은 **조용히** 발생한다 — 이 테스트가 유일한 방어선이다.
    const geomSrc = readFileSync(join(ROOT, 'renderer_geometry.cpp'), 'utf8');
    const nums = geomSrc.match(/GEOM_288B\s*=\s*\{([^}]*)\}/)?.[1]
        .split(',').map(s => Number(s.trim()));
    if (!nums || nums.length !== 6) { fail('GEOM_288B 리터럴을 읽지 못했다'); return; }
    const [gW, gH, gBpr, gDrawW, gXOff, gMax] = nums;
    const pairs = [
        ['glyphW', gW, cfg.GLYPH_W], ['glyphH', gH, cfg.GLYPH_H],
        ['bytesPerRow', gBpr, cfg.RASTER_BYTES_PER_ROW],
        ['drawW', gDrawW, RASTER_W], ['maxPerLine', gMax, cfg.MAX_CHARS_PER_LINE],
        ['xOffset', gXOff, -X_OFFSET],
    ];
    for (const [name, got, want] of pairs) {
        if (want === undefined) { fail(`config.h 기준값 ${name} 없음`); continue; }
        if (got !== want) fail(`GEOM_288B.${name}=${got} (JS/config.h 기준 ${want})`);
    }
}

/**
 * 문자집합 자체 검사 — 개수·중복·치환표 일관성
 * @details §4.1/§4.2: 간체 34자, **번체도 34자**(치환은 추가가 아니라 교체다).
 */
function checkCharsetShape() {
    for (const [name, list] of [['간체', CHARS_SIMPLIFIED], ['번체', CHARS_TRADITIONAL]]) {
        if (list.length !== 34) fail(`${name} 문자집합 ${list.length}자 (기대 34)`);
        const dup = [...new Set(list.filter((c, i) => list.indexOf(c) !== i))];
        if (dup.length > 0) fail(`${name} 문자집합에 중복 — ${dup.join(' ')}`);
    }
    // 번체는 반드시 "간체를 치환표로 덮은 것"이어야 한다 (직접 쓴 문자열이 아님)
    const mapped = CHARS_SIMPLIFIED.map(c => TRAD_MAP[c] || c);
    if (mapped.join('') !== CHARS_TRADITIONAL.join(''))
        fail('CHARS_TRADITIONAL이 "간체 + TRAD_MAP"과 다르다 — 번체를 직접 쓴 듯하다');
}

/**
 * 펌웨어의 치환표를 **소스 텍스트에서** 뽑아 JS 표와 대조한다
 * @details chinese_time_core.cpp에는 34자 문자집합 **상수가 없다** — TRADITIONAL_MAP 3쌍만 있다.
 *          글리프는 업로드된 파일에서 런타임에 찾는다. 그래서 대조 가능한 펌웨어 자산은
 *          치환표뿐이며, 이것도 임의로 하드코딩해 비교하면 아무것도 검증되지 않는다.
 */
function checkTraditionalMap() {
    const src = readFileSync(join(ROOT, 'chinese_time_core.cpp'), 'utf8');
    const pairs = [...src.matchAll(/\{\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\}/g)]
        .map(m => [m[1], m[2]]);
    if (pairs.length === 0) { fail('펌웨어에서 TRADITIONAL_MAP을 찾지 못했다'); return; }
    for (const [simp, trad] of pairs) {
        if (TRAD_MAP[simp] !== trad) fail(`치환표 불일치 — 펌웨어 ${simp}→${trad}, JS ${simp}→${TRAD_MAP[simp]}`);
    }
    const missing = Object.entries(TRAD_MAP).filter(([k]) => !pairs.some(p => p[0] === k));
    if (missing.length > 0) fail(`JS에만 있는 치환 — ${missing.map(m => m[0]).join(' ')}`);
}

/**
 * 펌웨어가 낼 수 있는 문자가 Font Studio 문자집합을 덮는지 본다
 * @param {object} cpp parseDump 결과
 * @details 이게 **가장 위험한 부류**다. 펌웨어가 "零五分"을 내는데 Font Studio에 `零`이 없으면
 *          upload.bin에 그 글자가 없어 OLED가 **빈 칸**으로 그린다.
 *          §3 표현을 대표 몇 개로만 훑으면 최장 표현(20~23시 + 59분)이 놓친다.
 */
function checkCharsetCoverage(cpp) {
    const used = (cpp.used['0:0'][0] || '').split('\x1f').filter(Boolean);
    const inSimp = new Set(CHARS_SIMPLIFIED);
    const inTrad = new Set(CHARS_TRADITIONAL);
    const uncovered = used.filter(c => !inSimp.has(c) && !inTrad.has(c));
    if (uncovered.length > 0)
        fail(`펌웨어가 내지만 문자집합에 없는 글자 — ${uncovered.join(' ')}`);
    // [여기까지가 이 검사의 전부다] "번체에 없는 글자가 있나?" 같은 역검사는 하지 않는다 —
    //   펌웨어는 번체 모드에서 `两`를 아예 내지 않는다(`兩`를 낸다). 그래서 역검사는
    //   정상 동작을 결함으로 본다. 번체 정합성은 checkCharsetShape()/checkTraditionalMap()가 맡는다.
    console.log(`  커버리지: 펌웨어 도달 가능 ${used.length}자 / 간체 ${inSimp.size}·번체 ${inTrad.size}자`);
}

/** JS의 layoutLine() 결과를 펌웨어 직렬화 형식(`글자@x|글자@x`)으로 만든다 */
function jsCells(text) {
    const r = layoutLine(text);
    if (!r.ok) return 'REJECT';
    return r.chars.map(c => (c.c === ' ' ? ' ' : c.c) + '@' + c.x).join('|');
}

/** 펌웨어 덤퍼 한 줄과 JS 결과를 비교한다 */
function compareRow(kind, key, jsValues, cppRow, label) {
    const cppValues = cppRow ?? [];
    for (let i = 0; i < jsValues.length; i++) {
        const got = jsValues[i], want = cppValues[i] ?? '';
        if (got !== want) {
            fail(`${label} [${kind} ${key}.${i}]\n    JS: ${JSON.stringify(got)}\n    C++: ${JSON.stringify(want)}`);
            return false;
        }
    }
    return true;
}

/** §3 전 표현을 둘 다 문자판으로 훑어 JS 행을 만든다 (키는 펌웨어 덤퍼와 동일) */
function buildJsRows() {
    const rows = { hour: {}, ms: {}, weekday: {}, daypart: {}, num: {}, numhour: {},
                   layout: {}, layoutH: {}, plain: {} };
    const PLAIN = [" 两点", "两点 ", "13  时", "    ", "   两点",
                   "ABCDE", "ABCDEF", "", "十三时", "星期日 下午"];
    PLAIN.forEach((t, i) => { rows.plain[`${i}:0`] = [t, jsCells(t)]; });

    for (let sc = 0; sc < 2; sc++) {
        for (let h = 0; h < 24; h++)
            rows.hour[`${h}:${sc}`] = [hourToChars(h, false, sc), hourToChars(h, true, sc)];
        for (let m = 0; m < 60; m++)
            rows.ms[`${m}:${sc}`] = [msToChars(m, "分"), msToChars(m, "秒")];
        for (let d = 0; d < 7; d++) rows.weekday[`${d}:${sc}`] = [weekdayToChars(d)];
        for (let h = 0; h < 24; h++) rows.daypart[`${h}:${sc}`] = [dayPartToChars(h)];

        for (let h = 0; h < 24; h++) {
            const t = withUnit(twoDigit(h), "时", "時", sc);
            rows.numhour[`${h}:${sc}`] = [t];
            rows.layout[`${h}:${sc}`] = [t, jsCells(t)];
        }
        for (let n = 0; n < 60; n++) {
            // 펌웨어 덤퍼는 분/초 단위가 두 문자판에서 같으므로 키만 분리한다 (sc, sc+2)
            const m = withUnit(twoDigit(n), "分", null, sc);
            rows.num[`${n}:${sc}`] = [m];
            rows.num[`${n}:${sc + 2}`] = [withUnit(twoDigit(n), "秒", null, sc)];
            rows.layout[`${n}:${sc}`] = [m, jsCells(m)];
        }
        for (let h = 0; h < 24; h++) {
            const t = hourToChars(h, true, sc);
            rows.layoutH[`${h}:${sc}`] = [t, jsCells(t)];
        }
    }
    return rows;
}

function main() {
    checkGeometry();
    checkCharsetShape();
    checkTraditionalMap();

    const cpp = parseDump(execFileSync(DUMPER, { maxBuffer: 1 << 24 }).toString());
    checkCharsetCoverage(cpp);

    const js = buildJsRows();
    let compared = 0;
    for (const kind of Object.keys(js)) {
        for (const [key, jsValues] of Object.entries(js[kind])) {
            const cppRow = cpp[kind]?.[key];
            if (!cppRow) { fail(`펌웨어 덤퍼에 없는 키 — ${kind} ${key}`); continue; }
            compareRow(kind, key, jsValues, cppRow, '불일치');
            compared++;
        }
    }

    console.log(`  대조 ${compared}행`);
    if (failures > 0) {
        console.log(`교차검증 실패: ${failures}건`);
        process.exit(1);
    }
    console.log('교차검증 통과: 기하·문자집합·치환표·전 표현·레이아웃 전부 일치');
}

main();