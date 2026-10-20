// worynim@gmail.com
/**
 * @file ip_dot_test.mjs
 * @brief [2026-10-21 사용자 보고] IP 화면 도트가 숫자 아래 줄처럼 내려가던 결함의 회귀 검증
 *
 * @details
 * IP 화면은 "숫자 3자리 + 도트" 묶음을 가로 중앙에 놓고, 도트를 마지막 숫자 밑동에 붙인다.
 * 그런데 래스터라이저는 잉크를 래스터(48×48) **세로 중앙**에 두므로 래스터 하단은 잉크
 * 하단이 아니다. 32px 폰트면 래스터 하단(52)과 잉크 하단(47)이 5px 차이 나서 도트만
 * 아래로 내려가 줄바꿈처럼 보였다. 작은 폰트(22px)면 10px까지 벌어진다.
 *
 * plan.md 의 "IP 화면" 요구(도트가 숫자 옆·밑동)를 소스 배선과 산식으로 고정한다.
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

/** config.h의 #define 정수 값을 읽는다 (산식을 테스트에서 다시 풀기 위함) */
const define = (name) => {
    const m = config.match(new RegExp(`^#define\\s+${name}\\s+(\\d+)`, 'm'));
    if (!m) throw new Error(`config.h 에서 ${name} 을 찾지 못했다`);
    return parseInt(m[1], 10);
};
const body = (src, re) => { const m = src.match(re); return m ? m[0] : ''; };

const SCREEN_HEIGHT = define('SCREEN_HEIGHT');
const LINE_HEIGHT = define('LINE_HEIGHT');
const IP_DOT_SIZE = define('IP_DOT_SIZE');
const IP_DOT_GAP = define('IP_DOT_GAP');
const FALLBACK_BASELINE = define('FALLBACK_BASELINE');
const GLYPH_H = define('GLYPH_H');          // 288B 래스터 높이(48)와 같다
const LINE_TOP_Y = Math.floor((SCREEN_HEIGHT - LINE_HEIGHT) / 2);   // renderer.h와 같은 식
const dotArea = IP_DOT_GAP + IP_DOT_SIZE;

const ipFn = body(dm, /void DisplayManager::showLargeIP\([\s\S]*?\n\}/);
chk('showLargeIP 정의 존재', ipFn.length > 0);

// --- 1. 버그 성립 조건: 래스터 하단 정렬은 잉크 하단보다 아래다 ---
// 잉크는 래스터 세로 중앙에 놓인다. 래스터 높이 GLYPH_H, 잉크 높이 ink 라면
//   잉크 하단 행 = (GLYPH_H + ink)/2 - 1, 화면 y = LINE_TOP_Y + 그 행.
const inkBottomOf = (ink) => LINE_TOP_Y + Math.floor((GLYPH_H + ink) / 2) - 1;
const oldDotY = LINE_TOP_Y + GLYPH_H - IP_DOT_SIZE;            // 수정 전 산식
const dotYFor32 = inkBottomOf(32) - IP_DOT_SIZE + 1;            // 수정 후 (32px 폰트)
const dotYFor22 = inkBottomOf(22) - IP_DOT_SIZE + 1;            // 수정 후 (22px 폰트)
chk(`수정 전 도트(${oldDotY})는 32px 잉크 하단(${inkBottomOf(32)})보다 5px 아래 — 보고된 증상`,
    oldDotY > inkBottomOf(32));
chk(`작은 폰트일수록 벌어진다 (22px 도트 위 ${dotYFor22} vs 32px ${dotYFor32})`,
    dotYFor22 < dotYFor32);
chk('수정 후 도트 하단이 잉크 하단(baseline)과 같다',
    dotYFor32 + IP_DOT_SIZE - 1 === inkBottomOf(32));
chk('수정 후 도트가 화면 안에 있다',
    dotYFor32 >= 0 && dotYFor32 + IP_DOT_SIZE <= SCREEN_HEIGHT && dotYFor22 >= 0);
const GLYPH_W = define('GLYPH_W');
chk('3자리 + 도트 묶음이 화면 폭 안에 들어온다',
    3 * GLYPH_W + dotArea <= define('SCREEN_WIDTH'));

// --- 2. 폴백(폰트 없음) 도트는 baseline에 붙는다 ---
const fallbackDotY = LINE_TOP_Y + FALLBACK_BASELINE - IP_DOT_SIZE + 1;
chk('폴백 도트 하단 = 폴백 폰트 baseline', fallbackDotY + IP_DOT_SIZE - 1 === LINE_TOP_Y + FALLBACK_BASELINE);

// --- 3. 소스 배선 ---
chk('캐시 글리프는 inkBottomY로 실제 잉크 하단을 잰다',
    /int inkBottom = renderer\.inkBottomY\(segment\.substring\(charCount - 1\), LINE_TOP_Y\)/.test(ipFn)
    && /dotY = inkBottom - IP_DOT_SIZE \+ 1/.test(ipFn));
chk('폴백은 FALLBACK_BASELINE 기준이다',
    /dotY = LINE_TOP_Y \+ FALLBACK_BASELINE - IP_DOT_SIZE \+ 1/.test(ipFn));
chk('도트가 화면 밖으로 나가지 않도록 클램프한다',
    /dotY = SCREEN_HEIGHT - IP_DOT_SIZE/.test(ipFn) && /dotY < 0\) dotY = 0/.test(ipFn));
chk('도트를 마지막 숫자 옆(묶음 좌표)에 둔다',
    /dotX\s*=\s*startX \+ charCount \* pitch \+ IP_DOT_GAP/.test(ipFn)
    && /drawBox\(dotX, dotY, IP_DOT_SIZE, IP_DOT_SIZE\)/.test(ipFn));
chk('inkBottomY가 renderer.h에 선언·renderer.cpp에 정의되어 있다',
    /int inkBottomY\(const String& charStr, int y\);/.test(rh)
    && /int Renderer::inkBottomY\(const String& charStr, int y\)/.test(rc));

console.log(`\n=== 결과: ${pass} passed, ${fail} failed ===`);
process.exit(fail === 0 ? 0 : 1);
