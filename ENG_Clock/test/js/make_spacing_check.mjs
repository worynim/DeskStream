// worynim@gmail.com
/**
 * @file make_spacing_check.mjs
 * @brief 실제 브라우저 픽셀 검증 — 글자 수에 따른 간격 확장 (PLAN §6.16)
 * @details measureText()/fillText()의 잉크 폭은 **실제 캔버스 픽셀**로만 확정된다
 *          (make_browser_check.mjs와 같은 이유 — Node 합성 테스트는 브라우저의
 *          안티에일리어싱 임계값을 대체하지 못한다).
 *          이 검사는 배포본(_extracted.mjs)의 실제 함수로
 *            1) maxInkWidthOf()가 폰트 크기에 따라 증가하는지
 *            2) layoutWrap()이 준 x로 **실제로 그린** 줄의 잉크 블록이 화면 중앙에 오는지
 *            3) 확장된 줄에서 이웃 글자의 잉크가 겹치지 않는지 (기하학적으로 가능한 경우)
 *          를 픽셀로 확인한다.
 *
 * 실행: node make_spacing_check.mjs  →  ./eng_spacing_check.html 생성
 * 확인: Playwright 등으로 열면 <pre id="out"> 에 결과가 찍힌다
 */
import { readFileSync, writeFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const src = readFileSync(join(HERE, '_extracted.mjs'), 'utf8');
const body = src.split('\nexport {')[0];   // export 줄 앞까지만 (선언 전체)

const page = `<!doctype html>
<html><head><meta charset="utf-8"><title>spacing check</title></head>
<body><pre id="out">RUNNING</pre>
<script>
${body}

const SIZES = [12, 18, 26, 32, 40, 48];
const results = [];
const say = (s) => results.push(s);
const fail = (s) => results.push('FAIL ' + s);

/** 배포본 renderGlyph()와 같은 경로 (6줄 — 정의 복제 아님, 호출부만 동일) */
function renderGlyphLike(ch, size, baseY) {
    const c = document.createElement('canvas'); c.width = RASTER_W; c.height = GLYPH_H;
    const x = c.getContext('2d');
    x.fillStyle = "#fff";
    x.font = size + 'px sans-serif';       // ClockFont 미로드 환경이므로 sans-serif
    x.textAlign = "center"; x.textBaseline = "middle";
    const m = x.measureText(ch);
    x.fillText(ch, glyphCenterX(m), baseY);
    return c;
}

/** 캔버스 1장의 알파 잉크 bbox (minX, maxX) */
function inkBox(c) {
    const px = c.getContext('2d').getImageData(0, 0, RASTER_W, GLYPH_H).data;
    let minX = 1e9, maxX = -1;
    for (let y = 0; y < GLYPH_H; y++) for (let x = 0; x < RASTER_W; x++) {
        if (px[(y * RASTER_W + x) * 4 + 3] > 128) {
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
        }
    }
    return { minX, maxX, empty: maxX < 0 };
}

const inkBySize = {};

for (const size of SIZES) {
    // ==== 1) 잉크 폭 측정이 폰트 크기에 따라 증가하는가 ====
    const baseY = computeGlyphBaselineFor(size);          // 배포 함수
    const cache = buildGlyphCache(UNIQ_CHARS, ch => renderGlyphLike(ch, size, baseY));
    const w = maxInkWidthOf(cache);                       // 배포 함수
    inkBySize[size] = w;
    // [§6.16b/c] refreshPreviewCache()와 **똑같이** 글자별 잉크 표를 채운다.
    //   이걸 빠뜨리면 inkOfChar()가 0을 돌려 모든 줄이 폰트 최대로 되돌아가고,
    //   검사하려는 바로 그 경로(줄별 상한 · 겹침 없는 하단)를 전혀 타지 않는다.
    previewInkWidth = w;
    previewInkByChar = {};
    for (const ch of UNIQ_CHARS) previewInkByChar[ch] = glyphInkWidth(cache[ch]);
    if (!(w > 0)) fail('size=' + size + ': 잉크 폭 0 (측정 실패)');
    if (w > RASTER_W) fail('size=' + size + ': 잉크 폭 ' + w + ' > 래스터 폭 ' + RASTER_W);
    say('size=' + String(size).padStart(2) + ' → previewInkWidth=' + w);

    // ==== 2~3) 실제 그리기 검증 ====
    // 1줄 화면: baseY=16, 래스터는 밴드 중앙 (펌웨어 rasterTopY: 16 + (32-64)/2 = -16)
    const rasterTop = 16 + Math.floor((LINE_HEIGHT - GLYPH_H) / 2);
    for (const text of ['AM', 'SEVEN', 'TWELVE', 'THIRTEEN', 'O\\'CLOCK', 'SEVENTEEN']) {
        const laid = layoutWrap(text, false, w).chars;
        const boxes = [];
        for (const ch of laid) {
            if (ch.c === ' ') continue;
            const b = inkBox(cache[ch.c]);
            if (b.empty) continue;
            // drawChar 규약: x − X_OFFSET
            boxes.push({ c: ch.c, x: ch.x, l: ch.x - X_OFFSET + b.minX, r: ch.x - X_OFFSET + b.maxX });
        }
        if (boxes.length === 0) continue;

        const L = Math.min(...boxes.map(b => b.l));
        const R = Math.max(...boxes.map(b => b.r));
        const center = (L + R) / 2;
        const pitch = laid[1] ? laid[1].x - laid[0].x : 0;

        // [물리 한계] 이 **줄이 실제로 쓰는** 글자의 최대 잉크로 잰다.
        //   linePitch는 폰트 전체 최대 잉크로 상한을 걸지만, 정렬·겹침 판정에는
        //   해당 줄의 잉크가 맞다. (예: 40px의 "TWELVE"는 W가 넓어 줄 잉크 32)
        const lineInk = Math.max(...boxes.map(b => b.r - b.l + 1));
        const inkSpan = (laid.length - 1) * pitch + lineInk;
        // [물리 한계 — 두 가지가 서로 다르다]
        //  (a) 겹침 회피 가능?  이웃끼리 최소 lineInk만큼 떨어져야 하���
        //      최소 필요 폭 = 글자수 × 줄 잉크. 이것이 128을 넘으면 산식을 바꿔도 겹친다.
        //  (b) 화면 안收录?  실제 블록 폭 (a)와 무관 — 피치가 이미 정해졌으므로
        //      inkSpan이 128을 넘으면 좌우가 잘린다.
        // (a)와 (b)는 다른 조건이라 따로 판정한다. 실제 블록 폭만으로 (a)를
        // 판단하면, pitch가 줄 잉크보다 작은 "물리 불가" 케이스를 놓친다.
        const canAvoidOverlap = laid.length * lineInk <= SCREEN_W;
        const fitsOnScreen = inkSpan <= SCREEN_W;

        if (fitsOnScreen) {
            // 1) 화면 중앙 정렬
            //    [허용 오차의 근거] lineStartX는 **양 끝이 줄 최대(lineInk)라고 가정하고**
            //    중앙을 맞춘다. 실제로는 양 끝 글자의 잉크가 다를 수 있어
            //    중심이 (firstInk − lastInk) / 4 만큼 어긋난다(양쪽 절반씩 나눠 갖는 구조).
            //    여기에 홀수 폭 잉크의 0.5px 양자화가 더해진다.
            //    → 허용치를 검증 값에서 유도한다. 손으로 올리면 버그를 숨기게 된다.
            //    (48px "O'CLOCK": O 29 vs K 23 → 1.5 + 양자화 ≈ 3px)
            //    화면 안에는 항상 들어간다: fit 상한이 더 넓은 lineInk로 계산되므로
            //    실제 블록은 예측 블록 안에 있다(잘림 위험은 이 검사가 따로 본다).
            const firstInk = previewInkByChar[boxes[0].c] || 0;
            const lastInk = previewInkByChar[boxes[boxes.length - 1].c] || 0;
            const centerTol = 2 + Math.ceil(Math.abs(firstInk - lastInk) / 4);
            if (Math.abs(center - (SCREEN_W - 1) / 2) > centerTol)
                fail('"' + text + '"@' + size + ': 잉크 중앙 ' + center.toFixed(1) +
                     ' (기대 ' + (SCREEN_W - 1) / 2 + ' ±' + centerTol + ') pitch=' + pitch +
                     ' 잉크=[' + L + ',' + R + '] 끝잉크=' + firstInk + '/' + lastInk);
            // 2) 화면 밖으로 새어 나가지 않아야 한다
            if (L < 0 || R > SCREEN_W - 1)
                fail('"' + text + '"@' + size + ': 화면 밖으로 잘림 [' + L + ',' + R + '] pitch=' + pitch);
        }
        // 3) 확장된 줄은 글자가 서로 겹치지 않아야 한다 (물리적으로 가능한 경우만)
        const sep = boxes.every((b, i) => i === 0 || b.l >= boxes[i - 1].r);
        if (pitch > GLYPH_W && canAvoidOverlap && !sep) {
            const touching = boxes.filter((b, i) => i > 0 && b.l < boxes[i - 1].r).length;
            fail('"' + text + '"@' + size + ': 확장됐는데 잉크 겹침 (쌍 ' + touching + ') pitch=' + pitch);
        }
        // 확장 여부 기록
        if (text === 'SEVEN' || text === 'AM' || text === 'TWELVE')
            say('    "' + text + '" pitch=' + pitch + ' 줄잉크=' + lineInk +
                ' 블록=[' + L + ',' + R + '] 중앙=' + center.toFixed(1) +
                (canAvoidOverlap ? '' : '  [겹침 물리 불가: 최소 ' + (laid.length * lineInk) + 'px > ' + SCREEN_W + ']') +
                (fitsOnScreen ? '' : '  [화면 밖 잘림]')) ;
    }
}

// [§6.16] linePitch(charCount, lineInk, fontInk, lineFloor) — 인자 4개.
//   아래 두 검사는 lineInk/lineFloor을 **폰트 최댓값**으로 준다.
//   둘 다 "이 줄이 실제로 어떻게 놓이든" 성립해야 하는 불변식이라
//   특정 문자열의 잉크를 알 필요가 없고, 알면 오히려 검사가 느슨해진다.

// 4) 작은 폰트는 확장되지 않아야 한다 (요구사항)
//    판정은 fontInk <= GLYPH_W 가드 한 곳에서 이뤄지므로 이것만 보면 된다.
for (const size of [12, 18]) {
    const w = inkBySize[size];
    for (let n = 1; n <= 9; n++) {
        if (linePitch(n, w, w, w) !== GLYPH_W)
            fail('작은 폰트 size=' + size + ' (ink=' + w + ') ' + n + '자 pitch=' + linePitch(n, w, w, w) +
                 ' — 확장되면 안 된다');
    }
}
// 5) 9자는 어떤 폰트에서도 기존 간격 (요구사항)
//    9자 몫(126/9)이 GLYPH_W라서 상한·완화가 무엇이든 14로 떨어진다.
for (const size of SIZES) {
    const w = inkBySize[size];
    if (linePitch(9, w, w, w) !== GLYPH_W)
        fail('9자@' + size + ' pitch=' + linePitch(9, w, w, w) + ' — ' + GLYPH_W + '여야 한다');
}
say('경계 검증 완료: 작은 폰트 확장 없음 · 9자 항상 ' + GLYPH_W + 'px');

const bad = results.filter(r => r.startsWith('FAIL')).length;
document.getElementById('out').textContent =
    results.join('\\n') + '\\n\\n=== ' + (results.length - bad) + ' passed, ' + bad + ' failed ===';
</script></body></html>`;

writeFileSync('/tmp/eng_spacing_check.html', page);
console.log('생성: /tmp/eng_spacing_check.html');
