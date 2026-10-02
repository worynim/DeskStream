// worynim@gmail.com
/**
 * @file make_baseline_check.mjs
 * @brief 잉크 세로 기준(합집합 vs 대문자) 편차 측정 페이지 생성기 (v6 — PLAN §6.15)
 * @details 현재 설계(38자 합집합 중앙)와 후보 설계(대문자 합집합 중앙)에서
 *          대문자 잉크가 래스터 중앙에서 얼마나 치우치는지 실제 픽셀로 잰다.
 *          대문자는 시계 문안의 지배 잉크다 — 숫자·슬래시의 디센더가 합집합에
 *          끼면 대문자가 위로 치우치고("화면 중앙이 아니라 약간 위"), 2줄 밴드
 *          중앙 창(래스터 중앙 32행)과 결합하면 윗줄 상단만 잘린다.
 *
 * 실행: node make_baseline_check.mjs  →  /tmp/eng_bias_check.html 생성
 */
import { readFileSync, writeFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const src = readFileSync(join(HERE, '_extracted.mjs'), 'utf8');
const body = src.split('\nexport {')[0];   // export 줄 앞까지만 (선언 전체)

const page = `<!doctype html>
<html><head><meta charset="utf-8"><title>baseline bias check</title></head>
<body><pre id="out">RUNNING</pre>
<script>
${body}
const out = [];
const CAPS = UNIQ_CHARS.filter(ch => ch >= 'A' && ch <= 'Z');
for (const size of [26, 32, 40, 48]) {
    const c = document.createElement('canvas'); c.width = RASTER_W; c.height = GLYPH_H;
    const x = c.getContext('2d');
    x.font = size + 'px sans-serif';
    x.textAlign = 'center'; x.textBaseline = 'middle';
    const ms = UNIQ_CHARS.map(ch => x.measureText(ch));
    // 현재 설계: 38자 합집합 중앙
    const yUnion = fontBaselineY(ms);
    // 후보 설계: 대문자 합집합 중앙
    const yCaps = fontBaselineY(CAPS.map(ch => x.measureText(ch)));
    // 각 베이스라인에서 대문자 잉크가 어디에 놓이는지 실제로 그려 측정
    function capsInkBox(baseY) {
        const cc = document.createElement('canvas'); cc.width = RASTER_W; cc.height = GLYPH_H;
        const xx = cc.getContext('2d');
        xx.fillStyle = '#fff'; xx.font = size + 'px sans-serif';
        xx.textAlign = 'center'; xx.textBaseline = 'middle';
        let mn = 1e9, mx = -1;
        for (const ch of CAPS) {
            xx.clearRect(0, 0, RASTER_W, GLYPH_H);
            xx.fillText(ch, RASTER_W / 2, baseY);
            const px = xx.getImageData(0, 0, RASTER_W, GLYPH_H).data;
            for (let yy = 0; yy < GLYPH_H; yy++) for (let xx2 = 0; xx2 < RASTER_W; xx2++)
                if (px[(yy * RASTER_W + xx2) * 4 + 3] > 128) { if (yy < mn) mn = yy; if (yy > mx) mx = yy; }
        }
        return { mn, mx };
    }
    const a = capsInkBox(yUnion), b = capsInkBox(yCaps);
    const cA = (a.mn + a.mx) / 2, cB = (b.mn + b.mx) / 2;
    out.push('size=' + size +
        ' [현재:38자합집합] 대문자 중심=' + cA.toFixed(1) + ' (중앙 32, 편차 ' + (cA - 32).toFixed(1) + ')' +
        ' 위여백=' + a.mn + ' 아래여백=' + (63 - a.mx) +
        ' [대문자기준] 중심=' + cB.toFixed(1) + ' 편차 ' + (cB - 32).toFixed(1) +
        ' 위여백=' + b.mn + ' 아래여백=' + (63 - b.mx));
}
document.getElementById('out').textContent = out.join('\\n');
</script></body></html>`;

writeFileSync('/tmp/eng_bias_check.html', page);
console.log('생성: /tmp/eng_bias_check.html');
