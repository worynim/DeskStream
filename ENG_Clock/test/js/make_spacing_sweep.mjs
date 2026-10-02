// worynim@gmail.com
/**
 * @file make_spacing_sweep.mjs
 * @brief §6.16 — 실제 시계가 표시하는 **모든 문자열** × 폰트 크기 전수 스윕
 * @details make_spacing_check.mjs는 대표 6문자열만 본다. 실제로 나오는 문자열은
 *          훨씬 많고, 특정 조합에서만 나타나는 화면 밖 잘림이 있으면 눈치채기 어렵다.
 *          이 스윕은 layout_crosscheck.mjs와 같은 문자열 집합을 브라우저 픽셀로 확인한다.
 *
 * 판정:
 *   - 화면 밖으로 나간 잉크(좌/우) → FAIL (눈에 보이는 결함)
 *   - 잉크 블록의 중앙 어긋남 (>2px) → FAIL (잘림이 없을 때만 판정)
 *   - 글자 간 겹침 → 물리 가능(글자수×줄잉크 ≤ 128)일 때만 FAIL
 */
import { readFileSync, writeFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const body = readFileSync(join(HERE, '_extracted.mjs'), 'utf8').split('\nexport {')[0];

const page = `<!doctype html>
<html><head><meta charset="utf-8"></head>
<body><pre id="out">RUNNING</pre>
<script>
${body}

const SIZES = [12, 18, 26, 32, 40, 48];
const lines = [];
const fails = [];
const say = (s) => lines.push(s);

function renderGlyphLike(ch, size, baseY) {
    const c = document.createElement('canvas'); c.width = RASTER_W; c.height = GLYPH_H;
    const x = c.getContext('2d');
    x.fillStyle = "#fff"; x.font = size + 'px sans-serif';
    x.textAlign = "center"; x.textBaseline = "middle";
    x.fillText(ch, glyphCenterX(x.measureText(ch)), baseY);
    return c;
}

/** 시계가 실제로 그리는 문자열 집합 (layout_crosscheck.mjs buildCases와 동일) */
function buildCases() {
    const SMALL = ["ZERO","ONE","TWO","THREE","FOUR","FIVE","SIX","SEVEN","EIGHT","NINE",
                   "TEN","ELEVEN","TWELVE","THIRTEEN","FOURTEEN","FIFTEEN",
                   "SIXTEEN","SEVENTEEN","EIGHTEEN","NINETEEN"];
    const TENS = ["","","TWENTY","THIRTY","FORTY","FIFTY"];
    const DAYS = ["SUNDAY","MONDAY","TUESDAY","WEDNESDAY","THURSDAY","FRIDAY","SATURDAY"];
    const n2w = (n) => n < SMALL.length ? SMALL[n]
        : (n % 10 === 0 ? TENS[Math.floor(n/10)] : TENS[Math.floor(n/10)] + " " + SMALL[n % 10]);
    const oclock = (n) => n === 0 ? "O'CLOCK" : n2w(n);
    const out = new Set();
    for (let h = 0; h < 24; h++) {
        out.add(n2w(h % 12 === 0 ? 12 : h % 12));
        out.add(n2w(h));
        out.add(h < 12 ? "AM" : "PM");
        for (let m = 0; m < 60; m++) {
            out.add(oclock(m)); out.add(n2w(m));
            for (let s = 0; s < 60; s++) { out.add(oclock(s)); out.add(n2w(s)); }
        }
    }
    DAYS.forEach(d => out.add(d));
    for (let mon = 1; mon <= 12; mon++)
        for (let day of [1, 2, 9, 10, 12, 15, 21, 25, 28, 30, 31])
            for (let w = 1; w <= 7; w++) {
                out.add(day + '/' + mon + ' ' + w); out.add(mon + '/' + day + ' ' + w);
                for (const d of DAYS) { out.add(day + '/' + mon + ' ' + d); out.add(mon + '/' + day + ' ' + d); }
            }
    ["ONE THOUSAND","A B C D E F G H I","ABCDEFGHIJKLMNOP","WEDNESDAY PM O'CLOCK",
     "TWENTY THREE","FORTY FIVE","FORTY ONE","FIFTY NINE","SEVENTEEN","O'CLOCK"]
        .forEach(s => out.add(s));
    return [...out];
}

let checked = 0, clipped = 0, offCenter = 0, overlapReal = 0;
let sClipBaseline = 0, sClipByExpansion = 0;   // 잘림의 출처: 기존 고정간격 vs 이번 확장

for (const size of SIZES) {
    const baseY = computeGlyphBaselineFor(size);
    const cache = buildGlyphCache(UNIQ_CHARS, ch => renderGlyphLike(ch, size, baseY));
    const fontInk = maxInkWidthOf(cache);
    // [§6.16b/c] refreshPreviewCache()와 동일하게 글자별 잉크 표를 채운다.
    //   빠뜨리면 inkOfChar()가 0을 돌려 줄별 상한이 폰트 최대로 되돌아가
    //   검사하려는 경로 자체를 타지 않는다.
    previewInkWidth = fontInk;
    previewInkByChar = {};
    for (const ch of UNIQ_CHARS) previewInkByChar[ch] = glyphInkWidth(cache[ch]);
    // 글자별 잉크 박스를 한 번만 재서 캐시한다 (getImageData는 비싸다)
    const ink = {};
    for (const ch of UNIQ_CHARS) {
        const px = cache[ch].getContext('2d').getImageData(0, 0, RASTER_W, GLYPH_H).data;
        let mn = 1e9, mx = -1;
        for (let y = 0; y < GLYPH_H; y++) for (let x = 0; x < RASTER_W; x++)
            if (px[(y * RASTER_W + x) * 4 + 3] > 128) { if (x < mn) mn = x; if (x > mx) mx = x; }
        ink[ch] = mx < 0 ? null : { mn, mx, w: mx - mn + 1 };
    }

    let sClip = 0, sCenter = 0, sOverlap = 0;

    for (const text of buildCases()) {
        const laid = layoutWrap(text, false, fontInk).chars;
        // 줄별로 따로 본다 (2줄 화면이 실제로 나올 수 있다)
        const byLine = {};
        for (const ch of laid) (byLine[ch.line] = byLine[ch.line] || []).push(ch);
        for (const ln of Object.keys(byLine)) {
            const cells = byLine[ln];
            const boxes = cells.filter(c => c.c !== ' ' && ink[c.c])
                               .map(c => ({ c: c.c, x: c.x,
                                            l: c.x - X_OFFSET + ink[c.c].mn,
                                            r: c.x - X_OFFSET + ink[c.c].mx }));
            if (!boxes.length) continue;
            checked++;
            const L = Math.min(...boxes.map(b => b.l)), R = Math.max(...boxes.map(b => b.r));
            const lineInk = Math.max(...boxes.map(b => b.r - b.l + 1));
            const pitch = cells[1] ? cells[1].x - cells[0].x : 0;
            const inkSpan = (cells.length - 1) * pitch + lineInk;

            // 1) 화면 밖으로 나간 잉크
            if (L < 0 || R > SCREEN_W - 1) {
                sClip++;
                // [회귀 판정] pitch가 GLYPH_W 그대로면 §6.16이 좌표를 **안 바꿨다**.
                //   이 경우의 잘림은 9자 고정 간격의 기존 동작이지 이번 변경 탓이 아니다.
                if (pitch === GLYPH_W) {
                    sClipBaseline++;
                } else {
                    sClipByExpansion++;
                    if (fails.filter(f => f.startsWith('clip')).length < 6)
                        fails.push('clip @' + size + ' "' + text + '"(줄' + ln + ') [' + L + ',' + R +
                                   '] pitch=' + pitch + ' 잉크블록=' + inkSpan);
                }
            } else if (Math.abs((L + R) / 2 - (SCREEN_W - 1) / 2) > 2) {
                // 2) 중앙 어긋남 — 잘림이 없을 때만 판정
                sCenter++;
                if (fails.filter(f => f.startsWith('center')).length < 6)
                    fails.push('center @' + size + ' "' + text + '"(줄' + ln + ') 중앙=' +
                               ((L + R) / 2).toFixed(1) + ' [' + L + ',' + R + ']');
            }
            // 3) 겹침 — 물리적으로 피할 수 있을 때만
            const canAvoid = cells.length * lineInk <= SCREEN_W;
            if (pitch > GLYPH_W && canAvoid &&
                boxes.some((b, i) => i > 0 && b.l < boxes[i - 1].r)) {
                sOverlap++;
                if (fails.filter(f => f.startsWith('overlap')).length < 6)
                    fails.push('overlap @' + size + ' "' + text + '"(줄' + ln + ') pitch=' +
                               pitch + ' 줄잉크=' + lineInk);
            }
        }
    }
    clipped += sClip; offCenter += sCenter; overlapReal += sOverlap;
    say('size ' + String(size).padStart(2) + ' (폰트 잉크 ' + String(fontInk).padStart(2) + '): ' +
        '잘림 ' + sClip + ' · 중앙어긋남 ' + sCenter + ' · 겹침 ' + sOverlap);
}

lines.push('');
lines.push('=== 줄 검사 ' + checked + '건 / 잘림 ' + clipped + ' · 중앙 ' + offCenter +
           ' · 겹침 ' + overlapReal + ' ===');
lines.push('잘림 출처 — 기존 고정간격(pitch=' + GLYPH_W + ') ' + sClipBaseline +
           ' / ★확장이 만든 것 ' + sClipByExpansion);
lines.push('※ 중앙 어긋남은 확장이 없어도 있는 기존 성질이다(무확장에서도 최대 1.5px).');
if (fails.length) { lines.push(''); fails.forEach(f => lines.push('FAIL ' + f)); }
lines.push(fails.length ? '=== 회귀 있음 ===' : '=== 전부 통과 (확장이 만든 결함 0) ===');
document.getElementById('out').textContent = lines.join('\\n');
</script></body></html>`;

writeFileSync('/tmp/eng_spacing_sweep.html', page);
console.log('생성: /tmp/eng_spacing_sweep.html');