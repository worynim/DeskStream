// worynim@gmail.com
/**
 * @file make_overlap_check.mjs
 * @brief 미리보기 글자 중첩 회귀 검증 페이지 생성기 (v5 — PLAN §6.14)
 * @details "AM"에서 M만 보이던 회귀(불투명 래스터가 앞 글자 잉크를 지움)를 실제
 *          브라우저에서 재현·검증한다. _extracted.mjs의 실제 상수(SCREEN_W/SCREEN_H/
 *          LINE_HEIGHT/GLYPH_H/X_OFFSET)로 drawChar의 좌표 규칙을 그대로 재현해
 *          두 글자를 겹쳐 그린 뒤, 두 셀의 잉크가 모두 남는지 픽셀로 센다.
 *
 * 실행: node make_overlap_check.mjs  →  /tmp/eng_overlap_check.html 생성
 */
import { readFileSync, writeFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const src = readFileSync(join(HERE, '_extracted.mjs'), 'utf8');
const body = src.split('\nexport {')[0];   // export 줄 앞까지만 (선언 전체)

const page = `<!doctype html>
<html><head><meta charset="utf-8"><title>overlap check</title></head>
<body><pre id="out">RUNNING</pre>
<script>
${body}

const results = [];
// drawChar의 실제 좌표 규칙을 그대로 재현: 두 글자를 피치 GLYPH_W로 겹쳐 그린다.
// 래스터는 투명 배경 + 흰 잉크(packPixels가 만드는 형식을 되돌린 RGBA).
const PITCH = 14;
const cells = ['A', 'M'];
// 1) 래스터를 만든다: 잉크는 셀(14px)을 넘어 좌우로 퍼진다(48px 래스터).
function makeRaster(ch, opaque) {
    const c = document.createElement('canvas'); c.width = RASTER_W; c.height = GLYPH_H;
    const x = c.getContext('2d');
    if (opaque) { x.fillStyle = '#000'; x.fillRect(0, 0, RASTER_W, GLYPH_H); }
    x.fillStyle = '#fff';
    x.font = '40px sans-serif';
    x.textAlign = 'center'; x.textBaseline = 'middle';
    x.fillText(ch, RASTER_W / 2, GLYPH_H / 2);   // 잉크만 (투명 배경)
    return c;
}
// 2) drawChar 규칙: ctx.drawImage(img, x - X_OFFSET, top), top = yOffset + (LINE_HEIGHT - GLYPH_H)/2
const inkTop = 16 + (LINE_HEIGHT - GLYPH_H) / 2;
function renderCells(opaque) {
    const screen = document.createElement('canvas'); screen.width = SCREEN_W; screen.height = SCREEN_H;
    const ctx = screen.getContext('2d');
    cells.forEach((ch, i) => ctx.drawImage(makeRaster(ch, opaque), i * PITCH - X_OFFSET, inkTop));
    return ctx.getImageData(0, 0, SCREEN_W, SCREEN_H).data;
}
const inkCols = (px) => {      // 흰 잉크 판정(R>128) — 투명/불투명 배경 양쪽에 모두 통한다
    let n = 0;
    for (let col = 0; col < 14; col++) {
        for (let y = 0; y < SCREEN_H; y++) {
            if (px[(y * SCREEN_W + col) * 4] > 128) { n++; break; }
        }
    }
    return n;
};
const alpha = inkCols(renderCells(false));
const opaquePx = renderCells(true);
const opaqueA = inkCols(opaquePx);
results.push((alpha >= 8 ? 'OK' : 'FAIL') + ' 투명 래스터: A 셀 잉크 열 수=' + alpha + ' (M과 겹쳐도 남는다)');
results.push((opaqueA < 8 ? 'OK' : 'FAIL') + ' 대조: 구 불투명 래스터는 A가 지워진다(열 수=' + opaqueA + ' — 회귀 기준)');
document.getElementById('out').textContent = results.join('\\n');
</script></body></html>`;

writeFileSync('/tmp/eng_overlap_check.html', page);
console.log('생성: /tmp/eng_overlap_check.html');
