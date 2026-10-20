// worynim@gmail.com
/**
 * @file ip_dot_test.mjs
 * @brief [2026-10-21 사용자 보고] IP 화면 도트가 숫자 아래 줄처럼 보이던 결함의 회귀 검증
 *
 * @details
 * 이전에는 도트를 화면 오른쪽 아래(120,56)에 **고정**해 찍었다. 숫자는 화면 세로 중앙인데
 * 점만 아래 모서리에 있어 "줄바꿈"처럼 보였다(빈 폰트 슬롯에서는 도트가 baseline보다
 * 14px 아래였다). 이제 도트를 마지막 숫자 뒤에 **인라인**으로 두고, 세로는 캐시 글리프의
 * 실제 잉크 하단(=baseline)에 맞춘다. 폴백 폰트는 TEXT_Y_POS baseline을 쓴다.
 */
import { readFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, '..', '..');
const read = (f) => readFileSync(join(ROOT, f), 'utf8');

let pass = 0, fail = 0;
const chk = (name, ok) => {
    if (ok) pass++;
    else { fail++; console.error(`  FAIL  ${name}`); }
};

const config = read('config.h');
const dm = read('display_manager.cpp');
const rh = read('renderer.h');
const rc = read('renderer.cpp');
const geom = read('renderer_geometry.cpp');

const define = (name) => {
    const m = config.match(new RegExp(`^#define\\s+${name}\\s+(\\d+)`, 'm'));
    if (!m) throw new Error(`config.h 에서 ${name} 을 찾지 못했다`);
    return parseInt(m[1], 10);
};
const constIn = (src, name) => {
    const m = src.match(new RegExp(`static const int ${name}\\s*=\\s*(\\d+)`));
    if (!m) throw new Error(`static const ${name} 을 찾지 못했다`);
    return parseInt(m[1], 10);
};
const body = (src, re) => { const m = src.match(re); return m ? m[0] : ''; };

const SCREEN_WIDTH = define('SCREEN_WIDTH');
const SCREEN_HEIGHT = define('SCREEN_HEIGHT');
const TEXT_Y_POS = define('TEXT_Y_POS');
const CELL_W = 32;   // GLYPH_CELL_W = LAYOUT_CELL_W
const IP_DOT_SIZE = constIn(dm, 'IP_DOT_SIZE');
const IP_DOT_GAP = constIn(dm, 'IP_DOT_GAP');
const RASTER_H = 64; // GEOM_512B/GEOM_256B glyphH

const ipFn = body(dm, /void DisplayManager::showLargeIP\([\s\S]*?\n\}/);
chk('showLargeIP 정의 존재', ipFn.length > 0);

// --- 1. 버그 성립 조건: 예전 고정 도트는 baseline/잉크 하단보다 아래다 ---
const OLD_DOT_X = SCREEN_WIDTH - 4 - IP_DOT_SIZE;    // 120 (IP_DOT_MARGIN 4)
const OLD_DOT_Y = SCREEN_HEIGHT - 4 - IP_DOT_SIZE;   // 56
chk(`예전 고정 도트 y(${OLD_DOT_Y})가 폴백 baseline(${TEXT_Y_POS})보다 14px 아래 — 보고된 증상`,
    OLD_DOT_Y - TEXT_Y_POS === 14);
// 64px 래스터에 32px 잉크를 세로 중앙에 두면 잉크 하단 행 = (64+32)/2 - 1 = 47
const inkBottom32 = Math.floor((RASTER_H + 32) / 2) - 1;
chk(`예전 고정 도트 y(${OLD_DOT_Y})가 일반 잉크 하단(${inkBottom32})보다 아래`,
    OLD_DOT_Y > inkBottom32);
chk('수정 후 인라인 도트는 3자리 + 도트가 화면 폭 안에 들어온다',
    3 * CELL_W + IP_DOT_GAP + IP_DOT_SIZE <= SCREEN_WIDTH);
chk('수정 후 도트가 화면 안에 있다',
    OLD_DOT_Y <= SCREEN_HEIGHT - IP_DOT_SIZE && TEXT_Y_POS - IP_DOT_SIZE + 1 >= 0);

// --- 2. 소스 배선 ---
chk('도트를 마지막 숫자 뒤 인라인에 둔다 (고정 120/56 제거)',
    /dotX\s*=\s*startX \+ charCount \* GLYPH_CELL_W \+ IP_DOT_GAP/.test(ipFn)
    && !/SCREEN_WIDTH\s*-\s*IP_DOT_MARGIN/.test(ipFn)
    && !/SCREEN_HEIGHT\s*-\s*IP_DOT_MARGIN/.test(ipFn));
chk('IP_DOT_MARGIN 상수가 제거됐다 (다시 쓰면 화면 모서리 고정으로 회귀)',
    !/IP_DOT_MARGIN/.test(dm));
chk('캐시 글리프는 inkBottomY로 실제 잉크 하단을 잰다',
    /int inkBottom = renderer\.inkBottomY\(segment\.substring\(charCount - 1\), 0\)/.test(ipFn)
    && /dotY = inkBottom - IP_DOT_SIZE \+ 1/.test(ipFn));
chk('폴백은 TEXT_Y_POS baseline 기준이다',
    /dotY = TEXT_Y_POS - IP_DOT_SIZE \+ 1/.test(ipFn));
chk('도트가 화면 밖으로 나가지 않도록 클램프한다',
    /dotY > SCREEN_HEIGHT - IP_DOT_SIZE/.test(ipFn) && /dotY < 0\) dotY = 0/.test(ipFn));
chk('가로 중앙은 도트 포함 묶음 기준이다',
    /blockW = charCount \* GLYPH_CELL_W \+ \(hasDot \? IP_DOT_GAP \+ IP_DOT_SIZE : 0\)/.test(ipFn)
    && /\(SCREEN_WIDTH - blockW\) \/ 2/.test(ipFn));
chk('inkBottomY가 renderer.h에 선언·renderer.cpp에 정의되어 있다',
    /int inkBottomY\(const String& charStr, int y_offset\);/.test(rh)
    && /int Renderer::inkBottomY\(const String& charStr, int y_offset\)/.test(rc));
chk('래스터 높이 64 가정이 기하 테이블과 맞는다',
    /GEOM_512B\s*=\s*\{\s*64,\s*64,/.test(geom) && /GEOM_256B\s*=\s*\{\s*32,\s*64,/.test(geom));

console.log(`\n=== 결과: ${pass} passed, ${fail} failed ===`);
process.exit(fail === 0 ? 0 : 1);
