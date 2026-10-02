// worynim@gmail.com
/**
 * @file make_spacing_diag.mjs
 * @brief §6.16 브라우저 검증 — 실패 케이스의 정밀 진단 (make_spacing_check.mjs 보완)
 * @details make_spacing_check.mjs가 잡은 "겹침/중앙 어긋남"이
 *          (a) 기하학적으로 불가능한 케이스인지
 *          (b) 화면 밖으로 새어 나가는 실제 결함인지
 *          를 숫자로 구분해 준다.
 *
 * 판단 기준 (물리적 한계):
 *   줄의 잉크 폭 = (n−1)·pitch + 해당 줄의 실제 최대 잉크
 *   이 값이 128을 넘으면 **어떤 배치를 해도** 화면 안에 담기지 않는다.
 *   → 이때의 겹침/잘림은 산식 탓이 아니라 폰트가 화면에 안 들어가는 사실이다.
 */
import { readFileSync, writeFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const src = readFileSync(join(HERE, '_extracted.mjs'), 'utf8');
const body = src.split('\nexport {')[0];

const page = `<!doctype html>
<html><head><meta charset="utf-8"></head>
<body><pre id="out">RUNNING</pre>
<script>
${body}

const SIZES = [26, 32, 40, 48];
const out = [];
const say = (s) => out.push(s);

function renderGlyphLike(ch, size, baseY) {
    const c = document.createElement('canvas'); c.width = RASTER_W; c.height = GLYPH_H;
    const x = c.getContext('2d');
    x.fillStyle = "#fff";
    x.font = size + 'px sans-serif';
    x.textAlign = "center"; x.textBaseline = "middle";
    x.fillText(ch, glyphCenterX(x.measureText(ch)), baseY);
    return c;
}
function inkBox(c) {
    const px = c.getContext('2d').getImageData(0, 0, RASTER_W, GLYPH_H).data;
    let minX = 1e9, maxX = -1;
    for (let y = 0; y < GLYPH_H; y++) for (let x = 0; x < RASTER_W; x++)
        if (px[(y * RASTER_W + x) * 4 + 3] > 128) { if (x < minX) minX = x; if (x > maxX) maxX = x; }
    return { minX, maxX };
}

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
    say('\\n=== size ' + size + ' : 폰트 최대 잉크 ' + fontInk + ' ===');

    for (const text of ['AM', 'SEVEN', 'TWELVE', 'THIRTEEN', 'O\\'CLOCK', 'SEVENTEEN']) {
        const laid = layoutWrap(text, false, fontInk).chars;
        const n = laid.length;
        const pitch = laid[1] ? laid[1].x - laid[0].x : 0;

        // 이 줄이 실제로 쓰는 글자들의 잉크 (폰트 전체 최대가 아니라 줄 실제 최대)
        const boxes = laid.filter(c => c.c !== ' ').map(c => {
            const b = inkBox(cache[c.c]);
            return { c: c.c, l: c.x - X_OFFSET + b.minX, r: c.x - X_OFFSET + b.maxX };
        });
        const lineInk = Math.max(...boxes.map(b => b.r - b.l + 1));
        const inkSpan = (n - 1) * pitch + lineInk;      // 이 줄 잉크 블록 폭
        const L = Math.min(...boxes.map(b => b.l));
        const R = Math.max(...boxes.map(b => b.r));

        const offLeft = L < 0, offRight = R > SCREEN_W - 1;
        const overlapPairs = boxes.filter((b, i) => i > 0 && b.l < boxes[i - 1].r).length;

        say('  ' + text.padEnd(10) +
            ' n=' + n + ' pitch=' + String(pitch).padStart(2) +
            ' 줄잉크=' + String(lineInk).padStart(2) +
            ' 잉크블록폭=' + String(inkSpan).padStart(3) +
            '[' + String(L).padStart(4) + ',' + String(R).padStart(3) + ']' +
            ' 중앙=' + ((L + R) / 2).toFixed(1).padStart(5) +
            (inkSpan > SCREEN_W ? '  ⚠물리불가(잉크폭' + inkSpan + '>128)' : '') +
            (overlapPairs ? '  겹침' + overlapPairs : '') +
            (offLeft || offRight ? '  ✂화면밖(' + (offLeft ? '좌' : '') + (offRight ? '우' : '') + ')' : ''));
    }
}
document.getElementById('out').textContent = out.join('\\n');
</script></body></html>`;

writeFileSync('/tmp/eng_spacing_diag.html', page);
console.log('생성: /tmp/eng_spacing_diag.html');