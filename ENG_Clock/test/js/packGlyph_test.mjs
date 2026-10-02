// worynim@gmail.com
/**
 * @file packGlyph_test.mjs
 * @brief web_pages.h의 packPixels()가 U8g2 drawBitmap 규약과 일치하는지 검증한다
 * @details 펌웨어는 384바이트 파일을 보고 geometryForSize(384) → GEOM_384B로 해석한다.
 *          업로드되는 바이트열이 그 기하와 맞아야 OLED에서 글자가 읽힌다.
 *
 * @note [SYNC] U8g2 `u8g2_DrawBitmap`(u8g2_bitmap.c)의 규약:
 *          - 3번째 인자 cnt는 **바이트/행**이다 (w = cnt * 8, bitmap += cnt)
 *          - MSB 우선 (mask = 128 에서 시작해 오른쪽으로 1씩)
 *          아래 decode()는 그 구현을 그대로 옮긴 것이다.
 *
 *          [여백 수정 v4 — PLAN §6.13] 래스터 폭(RASTER_W=48)이 피치(GLYPH_W=14)보다 넓다:
 *          잉크는 셀(0~13)을 넘어 x=47까지 그려질 수 있고, 펌웨어는 xOffset(−17)로
 *          이 넘친 부분을 **옆 글자 위에 겹쳐** 그린다 (사용자 요구 — 클리핑 금지).
 *          그래서 "x≥14는 비어 있어야 한다"는 구 계약은 폐기되었다.
 *
 *          [여백 수정 v5 — PLAN §6.14] 래스터 높이(GLYPH_H=64)가 밴드(LINE_HEIGHT=32)보다
 *          높다: 잉크 합집합은 래스터 세로 중앙에 정렬되고, 펌웨어는 래스터를 밴드 중앙에
 *          놓아(rasterTopY) 1줄 화면에서 잉크 64px까지 온전히 그린다.
 *          v6(PLAN §6.15)부터 세로 기준 잉크 집합은 대문자(A–Z) 합집합이다 —
 *          computeGlyphBaselineFor가 기준을 좁혀 측정하며, fontBaselineY 자체는
 *          metrics 집합만 받는 순수 함수라 이 테스트의 기대값은 변하지 않는다.
 *
 *          잉크 판정은 **알파 채널** > 128 이다. 래스터 배경이 투명하므로(v5) 알파 =
 *          잉크 커버리지이며, 구 형식(불투명 검은 배경 + 흰 잉크, R > 128)과 같은 50%
 *          커버리지 기준이다. 알파 채널은 프리멀티플라이되지 않아 값이 정확하다.
 *
 *          packPixels()를 직접 호출하므로, 웹 페이지의 패킹 규칙을 바꾸면 이 테스트가 실패한다.
 *          (테스트가 사본을 검증하는 구조가 되면 어긋남이 조용히 통과한다)
 */
import { GLYPH_W, GLYPH_H, RASTER_W, X_OFFSET, BYTES_PER_ROW, UNIQ_CHARS,
         packPixels, glyphCenterX, fontBaselineY } from './_extracted.mjs';

let pass = 0, fail = 0;
const chk = (name, ok) => {
    if (ok) pass++;
    else { fail++; console.error(`  FAIL  ${name}`); }
};

/** 마스크(행별 boolean 배열)를 캔버스가 돌려주는 RGBA 바이트 배열처럼 만든다.
 *  [v5] 잉크 = 알파 255, 배경 = 알파 0 (투명 배경 래스터의 실제 값과 같다).
 *  R 채널은 잉크에 255를 넣지만 배경은 0 — 판정은 알파만 한다. */
function toRgba(mask) {
    const px = new Uint8ClampedArray(RASTER_W * GLYPH_H * 4);
    for (let y = 0; y < GLYPH_H; y++) {
        for (let x = 0; x < RASTER_W; x++) {
            const v = mask[y][x] ? 255 : 0;      // 잉크 = 불투명 흰색, 배경 = 투명
            const i = (y * RASTER_W + x) * 4;
            px[i] = v; px[i + 1] = v; px[i + 2] = v; px[i + 3] = v;
        }
    }
    return px;
}

/** U8g2 u8g2_DrawBitmap + u8g2_DrawHorizontalBitmap을 그대로 옮긴 디코더 */
function decode(bm, cnt, h) {
    const w = cnt * 8;
    const out = [];
    for (let y = 0; y < h; y++) {
        const row = [];
        let mask = 128;
        for (let i = 0; i < w; i++) {
            row.push((bm[y * cnt + (i >> 3)] & mask) !== 0);
            mask >>= 1;
            if (mask === 0) mask = 128;
        }
        out.push(row);
    }
    return out;
}

const blank = () => Array.from({ length: GLYPH_H }, () => Array(RASTER_W).fill(false));

// --- 기하 자체가 기댓값인지 (펌웨어 renderer_geometry.cpp의 GEOM_384B) ---
console.log('기하 상수 (renderer_geometry.cpp GEOM_384B와 일치해야 한다)');
chk(`GLYPH_W=${GLYPH_W} (피치, 기대 14)`, GLYPH_W === 14);
chk(`GLYPH_H=${GLYPH_H} (기대 64 — 밴드 32보다 높은 전체 래스터)`, GLYPH_H === 64);
chk(`RASTER_W=${RASTER_W} (기대 48 — 슬라이더 최대 48px 잉크 수용)`, RASTER_W === 48);
chk(`BYTES_PER_ROW=${BYTES_PER_ROW} (기대 6)`, BYTES_PER_ROW === 6);
chk(`RASTER_W == BYTES_PER_ROW*8`, RASTER_W === BYTES_PER_ROW * 8);
chk(`X_OFFSET=${X_OFFSET} (기대 17 — 그릴 때 x − X_OFFSET = 펌웨어 xOffset −17)`, X_OFFSET === 17);
chk('X_OFFSET = (RASTER_W − GLYPH_W)/2 (잉크 중앙 = 피치 중앙)',
    X_OFFSET === (RASTER_W - GLYPH_W) / 2);
chk(`파일 크기 ${BYTES_PER_ROW * GLYPH_H}B = 384 (geometryForSize가 GEOM_384B로 매핑)`,
    BYTES_PER_ROW * GLYPH_H === 384);

// --- 1. 단일 픽셀 라운드트립: 모든 (x, y) — 래스터 전 폭 ---
console.log('\n픽셀 라운드트립 (U8g2 디코딩 기준, 래스터 전 폭)');
for (let y = 0; y < GLYPH_H; y++) {
    for (let x = 0; x < RASTER_W; x++) {
        const m = blank(); m[y][x] = true;
        const d = decode(packPixels(toRgba(m)), BYTES_PER_ROW, GLYPH_H);
        chk(`(${x},${y}) 복원`, d[y][x] === true && d[y].filter(Boolean).length === 1);
    }
}

// --- 2. 겹침 영역(x ≥ 14)도 잉크를 보관한다 (구 계약의 폐기를 확인하는 회귀 기준) ---
console.log('\n겹침 영역 규칙 (셀 밖 잉크는 잘리지 않고 옆 글자와 겹친다)');
const edge = Array.from({ length: GLYPH_H }, () =>
    Array.from({ length: RASTER_W }, (_, x) => x === RASTER_W - 1));   // 마지막 열(47)만 잉크
const dEdge = decode(packPixels(toRgba(edge)), BYTES_PER_ROW, GLYPH_H);
chk('마지막 열(47) 보존 — 클리핑 없음', dEdge.every(r => r[RASTER_W - 1] === true));
chk('마지막 열 외에는 비어 있음', dEdge.every(r => r.filter(Boolean).length === 1));
const overlap = Array.from({ length: GLYPH_H }, () =>
    Array.from({ length: RASTER_W }, (_, x) => x === GLYPH_W));        // 셀 경계 바로 다음(14)만 잉크
const dOv = decode(packPixels(toRgba(overlap)), BYTES_PER_ROW, GLYPH_H);
chk('x=14(인접 글자 칸)도 보존 — 겹침이 의도임', dOv.every(r => r[GLYPH_W] === true));

// --- 3. 임의 패턴 라운드트립 ---
console.log('\n임의 패턴 라운드트립');
const pattern = Array.from({ length: GLYPH_H }, (_, y) =>
    Array.from({ length: RASTER_W }, (_, x) => (x * 3 + y * 5) % 7 < 3));
const dPat = decode(packPixels(toRgba(pattern)), BYTES_PER_ROW, GLYPH_H);
chk('패턴 완전 복원', JSON.stringify(dPat) === JSON.stringify(pattern));

// --- 4. 임계값: 패킹은 알파 채널 128 초과분만 잉크로 본다 (커버리지 50%) ---
console.log('\n알파 임계값');
const half = new Uint8ClampedArray(RASTER_W * GLYPH_H * 4);
for (let i = 0; i < RASTER_W * GLYPH_H; i++) { half[i * 4] = 255; half[i * 4 + 3] = 128; }
chk('알파=128(경계)은 잉크 아님', decode(packPixels(half), BYTES_PER_ROW, GLYPH_H)
    .every(r => r.every(v => v === false)));
const justOver = new Uint8ClampedArray(RASTER_W * GLYPH_H * 4);
for (let i = 0; i < RASTER_W * GLYPH_H; i++) { justOver[i * 4] = 255; justOver[i * 4 + 3] = 129; }
chk('알파=129(초과)은 잉크임', decode(packPixels(justOver), BYTES_PER_ROW, GLYPH_H)
    .every(r => r.every(v => v === true)));

// --- 5. 잉크 중앙 정렬 순수 함수 ([여백 수정 v4/v5]) ---
console.log('\n잉크 중앙 정렬 (glyphCenterX / fontBaselineY)');

// glyphCenterX: actualBoundingBoxLeft/Right는 정렬점 기준 왼쪽/오른쪽 잉크 폭.
// 잉크 중앙 = (abbRight − abbLeft)/2 를 0으로 옮기려면 정렬점을 (abbLeft − abbRight)/2 움직인다.
const cx = (l, r) => glyphCenterX({ actualBoundingBoxLeft: l, actualBoundingBoxRight: r });
chk('대칭 잉크(l=r=5) → 정렬점 이동 없음 (RASTER_W/2)', cx(5, 5) === RASTER_W / 2);
chk('오른쪽으로 치우친 잉크(l=2, r=8) → 왼쪽으로 3 이동', cx(2, 8) === RASTER_W / 2 - 3);
chk('왼쪽으로 치우친 잉크(l=8, r=2) → 오른쪽으로 3 이동', cx(8, 2) === RASTER_W / 2 + 3);
chk('비대칭 이동량 = (l − r)/2 (부호 포함)',
    cx(7, 1) === RASTER_W / 2 + 3 && cx(1, 7) === RASTER_W / 2 - 3);

// fontBaselineY: 반환값은 **정렬점 절대 y**다. 결과를 공식으로 다시 서술하면 v4 초판처럼
// 잘못된 해석("이동량")이 그대로 통과하므로(PLAN §6.13e), 물리적 결과로 검증한다:
// 정렬점 y에 잉크 상대 범위를 더한 **실제 잉크 박스**가 래스터 세로 중앙에 와야 한다.
// [v5] 래스터 높이는 GLYPH_H(64) — 합집합 중앙도 래스터 중앙이다.
const yOf = (pairs) => fontBaselineY(
    pairs.map(([a, d]) => ({ actualBoundingBoxAscent: a, actualBoundingBoxDescent: d })));
const inkBox = (pairs) => {
    const A = yOf(pairs);
    const asc = Math.max(...pairs.map(([a]) => a));
    const desc = Math.max(...pairs.map(([, d]) => d));
    return { H: asc + desc, top: A - asc, bottom: A + desc };
};
for (const [name, pairs] of [
    ['대문자 잉크 24px (asc 20 / desc 4)', [[20, 4]]],
    ['아포스트로피 포함 합집합 26px', [[20, 4], [22, 2], [3, 1]]],
    ['밴드(32px)를 넘는 합집합 40px — 1줄 화면에서 온전히 보인다', [[34, 6]]],
    ['합집합 22px (asc 16 / desc 6)', [[16, 4], [14, 6]]],
]) {
    const b = inkBox(pairs);
    chk(`${name}: 잉크 상단 = (${GLYPH_H}−H)/2 (위아래 여백 균등)`,
        b.top === Math.round((GLYPH_H - b.H) / 2));
    chk(`${name}: 위 여백 == 아래 여백 (±1 반올림 허용)`,
        Math.abs(b.top - (GLYPH_H - b.bottom)) <= 1);
    if (b.H <= GLYPH_H) {
        chk(`${name}: 잉크가 래스터 안에 온전히 든다`, b.top >= 0 && b.bottom <= GLYPH_H);
    }
}
chk('metrics 미지원 → em 중앙 (GLYPH_H/2)',
    fontBaselineY([{ actualBoundingBoxAscent: undefined, actualBoundingBoxDescent: undefined }]) === GLYPH_H / 2);
chk('빈 목록 → em 중앙', fontBaselineY([]) === GLYPH_H / 2);

// --- 6. 문자집합: 클럭이 요구하는 글자를 모두 포함하는가 ---
console.log('\n문자집합');
const needed = new Set("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'/");
const missing = [...needed].filter(c => !UNIQ_CHARS.includes(c));
chk(`필수 문자 38자 포함${missing.length ? ' (누락: ' + missing.join('') + ')' : ''}`, missing.length === 0);
chk(`UNIQ_CHARS ${UNIQ_CHARS.length}개 (공백 미포함)`, UNIQ_CHARS.length === 38 && !UNIQ_CHARS.includes(" "));

// --- 7. 파일명이 펌웨어 getHexKey() 규약과 맞는가 ---
console.log('\n파일명 규약 (펌웨어 renderer.cpp getHexKey: sprintf("%02X"))');
// 주의: TextEncoder().encode()는 Uint8Array라 .map()을 쓰면 결과가 다시
// Uint8Array로 좁혀져 문자열이 0으로 뭉갠다. 문자열 누적에는 forEach만 쓴다.
for (const c of UNIQ_CHARS) {
    let hex = '';
    new TextEncoder().encode(c).forEach(b => {
        hex += b.toString(16).toUpperCase().padStart(2, '0');
    });
    chk(`'${c}' → c_${hex}.bin`, /^[0-9A-F]{2}$/.test(hex));
}
// 펌웨어는 name.substring(2, len-4)로 키를 뽑는다 (c_XX.bin에서 XX만)
const firmwareKey = (name) => name.substring(2, name.length - 4);
chk('c_41.bin → "41"', firmwareKey('c_41.bin') === '41');
chk('c_27.bin(아포스트로피) → "27"', firmwareKey('c_27.bin') === '27');
chk('c_5A.bin(Z) → "5A"', firmwareKey('c_5A.bin') === '5A');

console.log(`\n=== 결과: ${pass} passed, ${fail} failed ===`);
process.exit(fail === 0 ? 0 : 1);
