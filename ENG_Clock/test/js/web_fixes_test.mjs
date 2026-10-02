// worynim@gmail.com
/**
 * @file web_fixes_test.mjs
 * @brief [수정할 사항 v3] 버그 1·2·3의 회귀 검증
 *
 * @details
 * 버그 1 (폰트 크기 슬라이더가 동작하지 않는다) — v3 재정의
 *   원인: 자동 축소(fitSize/commonSize)가 잉크를 실측해 셀(14×32)에 들어올 때까지
 *         크기를 0.92배씩 줄였다. 슬라이더 값을 올려도 렌더 크기는 공통 크기로 고정됐다.
 *   계약([수정할 사항.md bug 1]): 슬라이더 값을 **그대로** 렌더 크기로 쓴다.
 *         공간이 부족해 글자가 겹치더라도 사용자가 고른 크기를 존중한다.
 *
 * 버그 2 (웹 설정을 바꿔도 반영이 안 된다)
 *   원인: saveConfig()가 변경 전체를 한 번에 POST하는데, 응답이 도착하기 전에 도착한
 *         5초 폴링이 옛 값을 컨트롤에 써 버린다. 사용자는 되돌아간 UI를 보고 다른 항목을
 *         건드리면 되돌아간 값으로 다시 저장해 변경이 증발한다.
 *   계약: 저장 확인 전(pending)에는 폴링이 그 필드를 덮어쓰지 않는다.
 *
 * 버그 3 (2줄일 때 애니메이션이 다른 줄에 보인다)
 *   원인: 스크롤이 SCREEN_H(64px)를 이동해서 아래줄 글자가 위줄 위로 지나간다.
 *   계약: 이동량은 LINE_HEIGHT(32)로 줄이고, 그리기는 줄 밴드로 잘라낸다.
 *
 * @note 순수 함수는 web_pages.h에서 직접 추출(_extracted.mjs)해 호출한다.
 *       배선(어느 함수가 어디서 호출되는가)은 정적 검사로 확인한다 —
 *       원래 버그의 모양이 "함수는 있는데 호출되지 않음"이었기 때문이다.
 */
import { readFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import {
    GLYPH_W, GLYPH_H, UNIQ_CHARS, LINE_HEIGHT, SCREEN_H, MAX_PER_LINE,
    currentGlyphSize, CONFIG_FIELDS, pollCanOverwrite, fieldToControl,
    layoutWrap,
} from './_extracted.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const WEB_PAGES = join(HERE, '..', '..', 'web_pages.h');

let pass = 0, fail = 0;
const chk = (name, ok) => {
    if (ok) pass++;
    else { fail++; console.error(`  FAIL  ${name}`); }
};

// 정적 검사용 — 배포되는 web_pages.h 본문에서 선언 하나를 잘라낸다.
const src = readFileSync(WEB_PAGES, 'utf8');
const grab = (re) => { const m = src.match(re); return m ? m[0] : ''; };

// ===================================================================
// 버그 1 — 슬라이더 크기를 그대로 쓴다 (자동 축소 삭제)
// ===================================================================
console.log('버그 1: 폰트 크기 슬라이더가 실제로 동작한다');

// 계약 1: currentGlyphSize()는 슬라이더 값 그대로를 돌려준다.
//   추출한 함수는 모듈 스코프의 els 를 참조하므로, els 를 주입해 호출한다.
const sizeFn = new Function('els', currentGlyphSize.toString() + '\nreturn currentGlyphSize();');
chk('슬라이더 26 → 렌더 크기 26', sizeFn({ sIn: { value: '26' } }) === 26);
chk('슬라이더 48 → 렌더 크기 48 (올리면 커진다)', sizeFn({ sIn: { value: '48' } }) === 48);
chk('슬라이더 12 → 렌더 크기 12 (내리면 작아진다)', sizeFn({ sIn: { value: '12' } }) === 12);
chk('셀(14px)보다 큰 값도 요청 그대로다 (겹침 허용)', sizeFn({ sIn: { value: '48' } }) > GLYPH_W);

// 계약 2: 자동 축소 코드가 남아 있지 않다 (재발 방지)
chk('fitSizeWith 선언이 제거됨', !/\bfunction\s+fitSizeWith\b/.test(src));
chk('fitSize 선언이 제거됨', !/\bfunction\s+fitSize\b/.test(src));
chk('commonSize 선언이 제거됨', !/\bfunction\s+commonSize\b/.test(src));
chk('fitSizeWith/commonSize 호출이 남아 있지 않다',
    !/fitSizeWith\(/.test(src) && !/commonSize\(/.test(src));

// 계약 3: renderGlyph는 받은 size를 그대로 폰트에 쓴다
const renderGlyphFn = grab(/function renderGlyph\(ch, size\) \{[\s\S]*?\n        \}/);
chk('renderGlyph 정의 존재', renderGlyphFn.length > 0);
chk('renderGlyph가 받은 size 를 그대로 폰트에 쓴다', /\$\{size\}px ClockFont/.test(renderGlyphFn));

// 계약 4: 캐시·라이브 폰트·업로드가 모두 같은 크기(슬라이더)를 쓴다
chk('refreshPreviewCache가 currentGlyphSize()를 쓴다',
    /currentGlyphSize\(\)/.test(grab(/function refreshPreviewCache\(\) \{[\s\S]*?\n        \}/)));
chk('drawChar 폴백 경로가 currentGlyphSize()를 쓴다',
    /currentGlyphSize\(\)/.test(grab(/function drawChar\(ctx, ch, x, yOffset\) \{[\s\S]*?\n        \}/)));
chk('업로드 경로가 currentGlyphSize()를 쓴다',
    /currentGlyphSize\(\)/.test(grab(/async function processAll\(\) \{[\s\S]*?\n        \}/)));

// 계약 5: UI 안내 문구가 새 동작을 설명한다 (겹침 허용)
chk('라벨에 자동 맞춤 문구가 남아 있지 않다', !/auto-fit/.test(src));
chk('라벨이 "그대로 적용 + 겹침 가능"을 안내한다',
    /used as-is/.test(src) && /overlap/.test(src));

// ===================================================================
// 버그 2 — 저장 확인 전에는 폴링이 값을 덮어쓰지 않는다
// ===================================================================
console.log('\n버그 2: 웹 설정 반영');

const el = { id: 'stub' };
const other = { id: 'other' };
const fresh = { saving: false, pending: new Set(), active: null };

// 계약 1: 평상시에는 덮어써도 된다
chk('대기 중인 필드 없음 → 덮어씀 허용', pollCanOverwrite('display_mode', el, fresh) === true);
chk('대기 중인 필드 없음 → 시간대도 허용', pollCanOverwrite('timezone', el, fresh) === true);

// 계약 2: 사용자가 방금 바꾼 값은 서버 확인 전까지 지킨다
const changed = { saving: false, pending: new Set(['display_mode']), active: null };
chk('저장 확인 전 필드는 덮어쓰지 않음', pollCanOverwrite('display_mode', el, changed) === false);
chk('바꾸지 않은 필드는 계속 갱신됨', pollCanOverwrite('hour_format', el, changed) === true);

// 계약 3: 저장 요청이 떠 있는 동안에는 전부 지킨다
const inflight = { saving: true, pending: new Set(), active: null };
chk('저장 중에는 모든 필드 보호', ['display_mode', 'brightness', 'font_slot']
    .every(k => pollCanOverwrite(k, el, inflight) === false));

// 계약 4: 사용자가 조작 중인 컨트롤은 지킨다 (기존 시간대 가드의 일반화)
const focused = { saving: false, pending: new Set(), active: el };
chk('포커스된 컨트롤 보호', pollCanOverwrite('brightness', el, focused) === false);
chk('포커스되지 않은 컨트롤은 갱신', pollCanOverwrite('brightness', other, focused) === true);
chk('el 이 null 이어도 크래시 없음', pollCanOverwrite('brightness', null, focused) === true);

// 계약 5: 필드 표가 실제 컨트롤과 일치해야 한다 (표가 틀리면 보호가 빈틈난다)
const KEYS = ['anim_mode', 'display_mode', 'hour_format', 'chime_enabled',
              'is_flipped', 'is_inverted', 'font_slot', 'brightness', 'date_order'];
const tableKeys = CONFIG_FIELDS.map(f => f.key).sort();
chk('필드 표가 펌웨어 파싱 키와 일치',
    JSON.stringify(tableKeys) === JSON.stringify([...KEYS].sort()));
chk('모든 필드에 컨트롤 매핑과 기본값이 있음',
    CONFIG_FIELDS.every(f => typeof f.el === 'string' && f.el.length && 'def' in f));
chk('불리언 필드가 isBool 로 표시됨',
    CONFIG_FIELDS.filter(f => f.isBool).map(f => f.key).sort()
        .join() === 'chime_enabled,is_flipped,is_inverted');

// 계약 6: 서버 값 → 컨트롤 문자열 변환
chk('불리언 true → "1"', fieldToControl(true, true) === '1');
chk('불리언 false → "0"', fieldToControl(false, true) === '0');
chk('숫자 2 → "2"', fieldToControl(2, false) === '2');
chk('숫자 0 → "0" (falsy 를 빈 값으로 보내지 않는다)', fieldToControl(0, false) === '0');

// ===================================================================
// 버그 3 — 2줄에서 애니메이션이 다른 줄 밴드를 넘지 않는다
// ===================================================================
console.log('\n버그 3: 2줄 애니메이션');

// 계약 1: 이동량은 한 줄 높이여야 한다 (이전엔 화면 높이 64px)
const STEPS = 16;
const offAt = (step) => step * (LINE_HEIGHT / STEPS);
chk(`최대 이동량 = 한 줄 높이 (${offAt(STEPS)} = ${LINE_HEIGHT})`, offAt(STEPS) === LINE_HEIGHT);
chk('이전 값(4px/스텝)은 한 줄 높이를 넘는다', STEPS * 4 > LINE_HEIGHT);

// 계약 2: 2줄에서 아래줄 글자가 위줄 밴드로 나가지 않는다
//   아래줄 baseY = 32, 위줄 밴드 = [0, 32). 스크롤업으로 새 글자가 내려오면
//   y = 32 + LINE_HEIGHT - off. off = 32일 때 y = 32 → 위줄 밴드 밖.
const twoLine = layoutWrap('TWENTY TWO').chars;          // 공백 → 2줄
const bottom = twoLine.filter(c => c.line === 1);
const topBandEnd = LINE_HEIGHT;
const bottomBaseY = bottom.length ? bottom[0].y : 0;
const enteringY = (step) => bottomBaseY + LINE_HEIGHT - offAt(step);
chk('"TWENTY TWO" 가 실제로 2줄이다', twoLine.some(c => c.line === 1));
chk(`아래줄 baseY = ${LINE_HEIGHT} (2줄 세로 중앙)`, bottomBaseY === LINE_HEIGHT);
chk('스크롤업 새 글자가 위줄 밴드(y<32)로 들어가는 스텝이 없다',
    Array.from({ length: STEPS + 1 }, (_, s) => s)
        .every(s => enteringY(s) >= topBandEnd));
chk('스크롤업 새 글자가 화면 밖으로 벗어나지도 않는다',
    enteringY(0) === bottomBaseY + LINE_HEIGHT && enteringY(STEPS) === bottomBaseY);

// 계약 3: 1줄은 세로 중앙 (기존 계약 유지)
const oneLine = layoutWrap('TWENTY').chars;
chk('1줄은 세로 중앙 (y = 16)', oneLine.every(c => c.y === Math.floor((SCREEN_H - LINE_HEIGHT) / 2)));
chk(`한 줄 폭이 화면을 넘지 않음 (${MAX_PER_LINE * GLYPH_W} ≤ ${SCREEN_H * 2}px)`,
    oneLine.every(c => c.x + GLYPH_W <= layoutWrap('X'.repeat(MAX_PER_LINE)).chars.length * GLYPH_W));

// 계약 4: [수정할 사항 1] singleLine=true면 "어절 2개 → 2줄" 규칙을 건너뛴다
const numeric = layoutWrap('02 H', true);
chk('"02 H" singleLine → 1줄이다', numeric.chars.every(c => c.line === 0));
chk('"02 H" singleLine → 세로 중앙 (y = 16)',
    numeric.chars.every(c => c.y === Math.floor((SCREEN_H - LINE_HEIGHT) / 2)));
const hChar = numeric.chars.find(c => c.c === 'H');
const twoChar = numeric.chars.find(c => c.c === '2');
chk('단위 문자 H 가 공백 한 칸 뒤에 배치된다 (x 간격 = 2×GLYPH_W)',
    hChar && twoChar && hChar.x - twoChar.x === 2 * GLYPH_W);
chk('기본값(false)은 기존 규칙 그대로 — "TWENTY TWO"는 여전히 2줄',
    layoutWrap('TWENTY TWO').chars.some(c => c.line === 1));
chk('singleLine에서 첫 넘침은 다음 줄로 내려간다 (잘리지 않는다)',
    layoutWrap('TWELVE THREE', true).dropped === 0);
chk('2줄마저 넘치는 어절은 여전히 버려진다 (무한 확장 아님)',
    layoutWrap('TWELVE THREE FOUR FIVE', true).dropped === 4);

// ===================================================================
// 배선 (정적) — 순수 함수만 통과해선 재발을 막을 수 없다
// ===================================================================
console.log('\n배선 검사 (호출 지점이 없으면 위 테스트는 무의미하다)');

// renderGlyph가 다시 크기를 조정하면 슬라이더가 무시된다
chk('renderGlyph가 크기를 재조정하지 않는다 (fitSize 재등장 방지)',
    !/\$\{fitSize\(/.test(renderGlyphFn));

// 폴링 가드가 실제 fetchConfig 안에 있다
const fetchFn = grab(/async function fetchConfig\(\) \{[\s\S]*?\n        \}/);
chk('fetchConfig가 pollCanOverwrite 로 각 필드를 가드', /pollCanOverwrite/.test(fetchFn));
chk('fetchConfig가 CONFIG_FIELDS 표를 순회', /for \(const f of CONFIG_FIELDS\)/.test(fetchFn));
chk('시간대도 pollCanOverwrite 로 가드', /pollCanOverwrite\('timezone'/.test(fetchFn));
// 가드를 통과시키려고 상태를 새로 만들면 원인이 그대로 남는다.
chk('fetchConfig가 실제 saving/pending 상태를 쓴다 (새 Set 으로 속이지 않는다)',
    /saving: saving, pending: pending/.test(fetchFn));
chk('필드 적용 전에 가드를 통과시켜야 한다',
    /if \(!pollCanOverwrite\(f\.key, el, state\)\) continue;[\s\S]{0,200}?el\.value =/.test(fetchFn));

// 저장이 성공 확인 전까지 pending 을 지우지 않는다
const saveFn = grab(/async function saveConfig\(\) \{[\s\S]*?\n        \}/);
chk('saveConfig가 성공 여부를 확인한다 (res.ok)', /res\.ok/.test(saveFn));
chk('saveConfig가 실패하면 pending 을 지우지 않는다',
    /catch[\s\S]*?pending\.delete/.test(saveFn) === false);
chk('saveConfig가 성공하면 pending 을 비운다', /pending\.delete/.test(saveFn));
chk('저장 중 플래그가 finally 에서 풀린다', /finally \{ saving = false; \}/.test(saveFn));

// 줄 밴드 클립과 이동량
const renderFn = grab(/function render\(\) \{[\s\S]*?\n        \}\n        render\(\);/);
const byLineCalls = (renderFn.match(/drawByLine\(/g) || []).length;
// 정지 분기 / 새 글자 / 퇴장 글자 — 세 곳 모두 줄 단위여야 한다.
chk(`렌더 루프의 세 드로잉 지점이 모두 줄 단위 (${byLineCalls}곳)`, byLineCalls === 3);
chk('스크롤 이동량이 LINE_HEIGHT 기준', /animStep \* \(LINE_HEIGHT \/ 16\)/.test(renderFn));
// 화면 높이(64px)만큼 이동하면 2줄에서 위줄 밴드(y<32)를 훑는다.
chk('스크롤이 화면 높이만큼 이동하지 않는다 (2줄 침범 원인)',
    !/baseY [+-] SCREEN_H/.test(renderFn));
chk('스크롤업 진입점이 한 줄 높이를 쓴다', /baseY \+ LINE_HEIGHT - off/.test(renderFn));
chk('스크롤다운 진입점이 한 줄 높이를 쓴다', /baseY - LINE_HEIGHT \+ off/.test(renderFn));
const drawByLineFn = grab(/function drawByLine\(ctx, chars, drawOne\) \{[\s\S]*?\n        \}/);
chk('drawByLine 정의 존재', drawByLineFn.length > 0);
chk('drawByLine가 줄 높이만큼 클립한다',
    /ctx\.rect\(0, d\.y, SCREEN_W, LINE_HEIGHT\)/.test(drawByLineFn)
    && /ctx\.clip\(\)/.test(drawByLineFn));
chk('drawByLine가 줄이 바뀔 때 save/restore 로 클립을 갱신한다',
    (drawByLineFn.match(/ctx\.restore\(\)/g) || []).length === 2
    && (drawByLineFn.match(/ctx\.save\(\)/g) || []).length === 1);

// [수정할 사항 1] 미리보기 렌더 루프도 singleLine 규칙을 쓴다 (펌웨어와 1:1)
chk('render()가 H/M/S 화면에 singleLine 을 넘긴다',
    /layoutWrap\(targetTimeStrings\[s\], !isWordPreview && s !== 0\)/.test(renderFn)
    && /layoutWrap\(lastTimeStrings\[s\], !isWordPreview && s !== 0\)/.test(renderFn));

// 수정할 사항 2·3 (IP 화면은 펌웨어 쪽이라 firmware_wiring_test.mjs 가 담당한다)
chk('요일을 숫자로 표시하는 함수가 남아 있지 않다', !/function numericDay/.test(src));
chk('숫자 모드 시/분/초에 공백 한 칸 + 단위 문자가 붙는다',
    /numericHour\(h, is24H\) \+ " H"/.test(src)
    && /twoDigit\(m\) \+ " M"/.test(src)
    && /twoDigit\(s\) \+ " S"/.test(src));
chk('숫자 모드에서도 요일은 영문', /const dayStr = dayName\(d\);/.test(src));

// ===================================================================
// [여백 수정 v4+v5 — PLAN §6.13/§6.14] 잉크 중앙 정렬 + 넓고 높은 투명 래스터
// ===================================================================
console.log('\n여백 v4/v5: 잉크 중앙 정렬 래스터');

chk('renderGlyph가 RASTER_W 폭 래스터를 만든다 (피치 GLYPH_W 아님)',
    /c\.width = RASTER_W/.test(renderGlyphFn));
chk('renderGlyph가 잉크 좌우 중앙을 래스터 중앙에 놓는다 (glyphCenterX)',
    /x\.fillText\(ch, glyphCenterX\(m\),/.test(renderGlyphFn));
chk('renderGlyph가 잉크 합집합 세로 중앙 y로 그린다 (glyphBaselineY — 정렬점 절대 y)',
    /x\.fillText\(ch, glyphCenterX\(m\), glyphBaselineY\);/.test(renderGlyphFn));

// --- v6: 기준 잉크 = 대문자(A–Z) 합집합 (PLAN §6.15) ---
const computeBaselineFn = grab(/function computeGlyphBaselineFor\(size\) \{[\s\S]*?\n        \}/);
chk('computeGlyphBaselineFor 정의 존재', computeBaselineFn.length > 0);
chk('computeGlyphBaselineFor가 대문자(A–Z)만 기준으로 측정한다',
    /UNIQ_CHARS\.filter\(ch => ch >= "A" && ch <= "Z"\)/.test(computeBaselineFn));
chk('computeGlyphBaselineFor가 기준 집합을 fontBaselineY에 그대로 넘긴다',
    /fontBaselineY\(caps\.map\(ch => x\.measureText\(ch\)\)\)/.test(computeBaselineFn));
chk('computeGlyphBaselineFor가 38자 전체 기준으로 돌아가지 않았다 (숫자 디센더 편차 회귀)',
    !/fontBaselineY\(UNIQ_CHARS\.map/.test(computeBaselineFn));
chk('renderGlyph는 반환 y에 GLYPH_H/2를 더해 그리지 않는다 (v4 초판 회귀 — 16px 밀림)',
    !/fillText\(ch, glyphCenterX\(m\), GLYPH_H \/ 2 \+/.test(renderGlyphFn));
chk('renderGlyph는 em 박스 중앙 하드코딩으로 돌아가지 않았다 (위쪽만 잘리던 원인)',
    !/fillText\(ch, GLYPH_W \/ 2, GLYPH_H \/ 2\)/.test(renderGlyphFn));
chk('measureText로 잉크 박스를 잰다',
    /x\.measureText\(ch\)/.test(renderGlyphFn));

// --- v5: 투명 배경 래스터 ("AM"에서 M만 보이던 원인 제거) ---
chk('renderGlyph가 배경을 채우지 않는다 (불투명 래스터는 앞 글자 잉크를 지운다)',
    !/x\.fillRect/.test(renderGlyphFn));
const packPixelsFn = grab(/function packPixels\(px\) \{[\s\S]*?\n        \}/);
chk('packPixels가 정의 존재', packPixelsFn.length > 0);
chk('packPixels가 알파 채널(+3)로 잉크를 판정한다 (투명 배경의 커버리지)',
    /px\[\(y \* RASTER_W \+ x\) \* 4 \+ 3\] > 128/.test(packPixelsFn));
chk('packPixels가 R 채널만 보는 구 계약으로 돌아가지 않았다 (투명 배경에서 R=255 과포화)',
    !/px\[\(y \* RASTER_W \+ x\) \* 4\] > 128/.test(packPixelsFn));
const tintedFn = grab(/function tintedGlyph\(img, color\) \{[\s\S]*?\n        \}/);
chk('tintedGlyph 정의 존재 (알파 마스크 재생색)', tintedFn.length > 0);
chk('tintedGlyph가 source-in으로 색만 바꾼다',
    /globalCompositeOperation = 'source-in'/.test(tintedFn)
    && /tx\.fillRect\(0, 0, RASTER_W, GLYPH_H\)/.test(tintedFn));
chk('구 반전 경로(흰 배경 + destination-out — 전부 지워져 아무것도 안 그렸음)가 남아 있지 않다',
    !/destination-out/.test(src));

const drawCharFn = grab(/function drawChar\(ctx, ch, x, yOffset\) \{[\s\S]*?\n        \}/);
const drawScaledFn = grab(/function drawScaledChar\(ctx, ch, x, baseY, h\) \{[\s\S]*?\n        \}/);
const drawZoomedFn = grab(/function drawZoomedChar\(ctx, ch, x, baseY, scale\) \{[\s\S]*?\n        \}/);
chk('drawChar 정의 존재', drawCharFn.length > 0);
chk('drawScaledChar 정의 존재', drawScaledFn.length > 0);
chk('drawZoomedChar 정의 존재', drawZoomedFn.length > 0);
// 래스터가 피치보다 넓으므로 잉크 중앙이 피치 중앙에 오려면 x − X_OFFSET 이어야 한다
// (펌웨어 drawBitmap(x + xOffset, ...) 미러). x에 그리면 글자가 오른쪽으로 17px 밀린다.
// [v5] 래스터는 밴드 중앙에 놓인다 — y = yOffset + (LINE_HEIGHT − GLYPH_H)/2 (펌웨어 rasterTopY).
chk('drawChar가 밴드 중앙 y(top)를 계산한다',
    /const top = yOffset \+ \(LINE_HEIGHT - GLYPH_H\) \/ 2;/.test(drawCharFn));
chk('drawChar가 x − X_OFFSET, top 에 그린다',
    /ctx\.drawImage\((?:img|tintedGlyph\(img, "#000"\)), x - X_OFFSET, top\)/.test(drawCharFn));
chk('drawScaledChar가 밴드 중앙에 h 높이로 그린다',
    /const top = baseY \+ \(LINE_HEIGHT - h\) \/ 2;/.test(drawScaledFn));
chk('drawScaledChar가 래스터 중앙의 LINE_HEIGHT 창을 샘플링한다 (펌웨어 winTop 규칙)',
    /ctx\.drawImage\(src, 0, \(GLYPH_H - LINE_HEIGHT\) \/ 2, RASTER_W, LINE_HEIGHT,/
        .test(drawScaledFn));
chk('drawZoomedChar가 x − X_OFFSET 를 기준으로 확대한다',
    /const left = x - X_OFFSET \+ \(RASTER_W - tw\) \/ 2/.test(drawZoomedFn));
chk('drawZoomedChar가 확대 래스터를 밴드 중앙에 놓는다',
    /top = baseY \+ \(LINE_HEIGHT - th\) \/ 2/.test(drawZoomedFn));
chk('반전 경로가 tintedGlyph를 쓴다 (구 destination-out 재등장 방지)',
    (drawCharFn + drawScaledFn + drawZoomedFn).includes('tintedGlyph(img, "#000")'));
chk('폴백(라이브 폰트) 경로도 같은 잉크 중앙 정렬을 쓴다',
    /glyphCenterX\(m\)/.test(drawCharFn) && /glyphCenterX\(m\)/.test(drawZoomedFn));
chk('폴백 경로가 glyphBaselineY를 그대로 그린다 (GLYPH_H/2 재합성 금지)',
    /top \+ glyphBaselineY/.test(drawCharFn)
    && /glyphBaselineY - GLYPH_H \/ 2/.test(drawZoomedFn));
chk('폴백 확대의 이동 중심이 밴드 중앙이다',
    /translate\(x - X_OFFSET \+ RASTER_W \/ 2, baseY \+ LINE_HEIGHT \/ 2\)/.test(drawZoomedFn));

// --- v5: 1줄은 클립하지 않는다 (큰 폰트가 밴드 밖으로 나가도 온전히 보인다) ---
const drawByLineFnV5 = grab(/function drawByLine\(ctx, chars, drawOne\) \{[\s\S]*?\n        \}/);
chk('drawByLine 정의 존재', drawByLineFnV5.length > 0);
chk('drawByLine가 1줄 레이아웃(y=세로 중앙)을 판별한다',
    /chars\.every\(d => d\.y === \(SCREEN_H - LINE_HEIGHT\) \/ 2\)/.test(drawByLineFnV5));
chk('1줄이면 클립 전에 일반 그리기로 빠져나간다',
    drawByLineFnV5.indexOf('if (singleLine)') < drawByLineFnV5.indexOf('ctx.clip()'));
chk('drawByLine가 줄 높이만큼 클립한다 (2줄 규칙 유지)',
    /ctx\.rect\(0, d\.y, SCREEN_W, LINE_HEIGHT\)/.test(drawByLineFnV5)
    && /ctx\.clip\(\)/.test(drawByLineFnV5));

const refreshFnV4 = grab(/function refreshPreviewCache\(\) \{[\s\S]*?\n        \}/);
chk('refreshPreviewCache가 캐시 생성 **전에** glyphBaselineY를 계산한다 (순서 계약)',
    refreshFnV4.indexOf('computeGlyphBaselineFor') < refreshFnV4.indexOf('buildGlyphCache')
    && /glyphBaselineY = computeGlyphBaselineFor/.test(refreshFnV4));

console.log(`\n=== 결과: ${pass} passed, ${fail} failed ===`);
process.exit(fail === 0 ? 0 : 1);
