// worynim@gmail.com
/**
 * @file make_browser_check.mjs
 * @brief 실제 브라우저 픽셀 검증 페이지 생성기 (v4 회귀 — PLAN §6.13e)
 * @details v4 초판의 세로 중앙 정렬 회귀는 Node 단위 테스트가 잡지 못했다.
 *          테스트 기대값이 잘못된 공식과 같은 해석으로 쓰였기 때문이다.
 *          이 검증은 배포본(web_pages.h)에서 추출한 **실제 함수**(_extracted.mjs)를
 *          진짜 브라우저에서 실행해, 캔버스에 그린 뒤 픽셀을 직접 세어
 *          "잉크 합집합이 셀 세로 중앙에 오는가 / 글자별 가로 중앙이 맞는가"를 확인한다.
 *          v6(PLAN §6.15)부터 세로 기준은 대문자(A–Z) 합집합이다 — 배포 함수
 *          computeGlyphBaselineFor를 그대로 호출해 정의 복제 없이 검증한다.
 *
 * 실행: node make_browser_check.mjs  →  /tmp/eng_browser_check.html 생성
 */
import { readFileSync, writeFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const src = readFileSync(join(HERE, '_extracted.mjs'), 'utf8');
const body = src.split('\nexport {')[0];   // export 줄 앞까지만 (선언 전체)

const page = `<!doctype html>
<html><head><meta charset="utf-8"><title>ink centering check</title></head>
<body><pre id="out">RUNNING</pre>
<script>
${body}

// ==== 검증: renderGlyph와 동일한 경로로 그린 뒤 픽셀을 직접 센다 ====
// [v6 — PLAN §6.15] 세로 기준은 대문자(A–Z) 합집합이므로 세로 검증도 대문자로 한다.
const CAPS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ".split("");
const CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'/".split("");
const results = [];
function measureCtx(size) {
    const c = document.createElement('canvas');
    const x = c.getContext('2d');
    x.font = size + 'px sans-serif';
    x.textAlign = 'center'; x.textBaseline = 'middle';
    return x;
}
function inkBBox(ch, size, baseY) {
    const c = document.createElement('canvas'); c.width = RASTER_W; c.height = GLYPH_H;
    const x = c.getContext('2d');
    x.fillStyle = '#000'; x.fillRect(0, 0, RASTER_W, GLYPH_H);
    x.fillStyle = '#fff';
    x.font = size + 'px sans-serif';
    x.textAlign = 'center'; x.textBaseline = 'middle';
    const m = x.measureText(ch);
    x.fillText(ch, glyphCenterX(m), baseY);
    const px = x.getImageData(0, 0, RASTER_W, GLYPH_H).data;
    let minX = 1e9, maxX = -1, minY = 1e9, maxY = -1;
    for (let y = 0; y < GLYPH_H; y++) for (let xx = 0; xx < RASTER_W; xx++) {
        if (px[(y * RASTER_W + xx) * 4] > 128) {
            if (xx < minX) minX = xx; if (xx > maxX) maxX = xx;
            if (y < minY) minY = y; if (y > maxY) maxY = y;
        }
    }
    return { minX, maxX, minY, maxY, empty: maxY < 0 };
}
for (const size of [12, 18, 26, 32, 40, 48]) {
    // 실제 배포 함수(computeGlyphBaselineFor)를 그대로 호출한다 — 검증이 정의를 복제하지 않는다.
    const baseY = computeGlyphBaselineFor(size);
    // 세로: 대문자 합집합이 래스터 중앙에 오는가
    let uMinY = 1e9, uMaxY = -1;
    for (const ch of CAPS) {
        const b = inkBBox(ch, size, baseY);
        if (b.empty) { results.push('FAIL 빈 글자: ' + ch + '@' + size); continue; }
        uMinY = Math.min(uMinY, b.minY); uMaxY = Math.max(uMaxY, b.maxY);
    }
    const topM = uMinY, botM = GLYPH_H - 1 - uMaxY;
    // 허용치 ±2행: measureText(실수)와 실제 그린 픽셀(정수 격자)의 양자화로 잉크가
    // 최대 1행 어긋날 수 있다. v6 결함 범주는 폰트 메트릭이 만드는 수 px 편차다.
    if (Math.abs(topM - botM) > 2)
        results.push('FAIL 세로(대문자) size=' + size + ': 위여백=' + topM + ' 아래여백=' + botM +
                     ' (대문자 합집합 ' + uMinY + '..' + uMaxY + ')');
    else
        results.push('OK size=' + size + ': 대문자 위여백=' + topM + ' 아래여백=' + botM +
                     ' 합집합 y=' + uMinY + '..' + uMaxY);
    // 2줄 밴드(중앙 32행)에 대문자가 들어가는가 — 대문자 높이가 32px 이하일 때만 요구
    if (uMaxY - uMinY + 1 <= 32 && (uMinY < 16 || uMaxY > 47))
        results.push('FAIL 2줄 밴드 size=' + size + ': 대문자 y=' + uMinY + '..' + uMaxY +
                     ' 가 중앙 32행(16..47)을 벗어난다');
    // 가로: 모든 글자 잉크 중앙이 래스터 중앙((RASTER_W−1)/2 = 23.5)에 와야 한다
    for (const ch of CHARS) {
        const b = inkBBox(ch, size, baseY);
        if (b.empty) continue;
        const cInk = (b.minX + b.maxX) / 2;
        if (Math.abs(cInk - (RASTER_W - 1) / 2) > 2)
            results.push('FAIL 가로 ' + ch + '@' + size + ': centerX=' + cInk);
    }
}
document.getElementById('out').textContent = results.join('\\n');
</script></body></html>`;

writeFileSync('/tmp/eng_browser_check.html', page);
console.log('생성: /tmp/eng_browser_check.html');
