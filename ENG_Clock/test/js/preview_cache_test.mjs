// worynim@gmail.com
/**
 * @file preview_cache_test.mjs
 * @brief 웹 페이지의 미리보기 캐시가 폰트·크기 변경을 따라갱신되는지 검증한다
 * @details [버그] 웹 페이지에서 폰트를 새로 불러와도 미리보기의 글자 크기가 바뀌지 않았다.
 *
 *          원인: bitmapCache가 processAll()(=업로드) 안에서만 채워졌다.
 *          drawChar()는 캐시가 있으면 캐시를 쓰고, 없을 때만 라이브 폰트를 쓰기 때문에
 *          "업로드 전에는 정상 → 업로드 후로 굳음" 형태의 버그가 된다.
 *
 *          이 테스트는 **배포되는 web_pages.h의 buildGlyphCache()** 를 직접 호출해
 *          "새 폰트/새 크기로 만든 캐시가 이전 캐시를 완전히 대체하는가"를 확인한다.
 *          renderGlyph는 캔버스가 필요하므로 스텁을 주입한다.
 */
import { readFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import { buildGlyphCache, UNIQ_CHARS } from './_extracted.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const WEB_PAGES = join(HERE, '..', '..', 'web_pages.h');

let pass = 0, fail = 0;
const chk = (name, ok) => {
    if (ok) pass++;
    else { fail++; console.error(`  FAIL  ${name}`); }
};

/**
 * renderGlyph 스텁 — 실제로는 14×32 캔버스를 만들지만,
 * 캐시 계약(어떤 값이 키로 묶이는지)만 보면 되므로 "크기@글자" 문자열을 돌려준다.
 */
const stubRender = (size) => (ch) => `${size}@${ch}`;

// --- 1. 모든 문자에 대한 캐시가 만들어진다 ---
console.log('캐시 구성');
const cache26 = buildGlyphCache(UNIQ_CHARS, stubRender(26));
chk(`키 ${UNIQ_CHARS.length}개`, Object.keys(cache26).length === UNIQ_CHARS.length);
const missing = UNIQ_CHARS.filter(c => !(c in cache26));
chk(`누락 문자 없음${missing.length ? ' (' + missing.join('') + ')' : ''}`, missing.length === 0);

// --- 2. 값은 "현재 크기 + 글자" (이전 크기 값이 새어 나오면 안 된다) ---
console.log('\n크기 변경 반영');
for (const ch of UNIQ_CHARS) {
    chk(`'${ch}' 26px`, cache26[ch] === `26@${ch}`);
}
const cache40 = buildGlyphCache(UNIQ_CHARS, stubRender(40));
const stale = UNIQ_CHARS.filter(c => cache40[c] !== `40@${c}`);
chk(`40px로 바꾸면 38자 전부 새 값${stale.length ? ' (남은 것: ' + stale.join('') + ')' : ''}`,
    stale.length === 0);
chk('이전 캐시 객체는 변하지 않는다 (불변성)',
    UNIQ_CHARS.every(c => cache26[c] === `26@${c}`));
chk('두 캐시는 서로 다른 객체', cache26 !== cache40);

// --- 3. 폰트를 바꾼 경우도 값이 전부 달라진다 ---
console.log('\n폰트 변경 반영');
const cacheOther = buildGlyphCache(UNIQ_CHARS, (ch) => `OTHER@${ch}`);
const same = UNIQ_CHARS.filter(c => cacheOther[c] === cache26[c]);
chk(`다른 폰트 결과가 전부 다름${same.length ? ' (같음: ' + same.join('') + ')' : ''}`, same.length === 0);

// --- 4. 예외 입력 ---
console.log('\n경계');
chk('빈 문자 목록 → 빈 캐시', Object.keys(buildGlyphCache([], stubRender(26))).length === 0);

// --- 5. 배선 검사: 이벤트 핸들러가 실제로 갱신을 호출하는가 ---
//
// buildGlyphCache만 통과해서는 버그가 재발할 수 있다. 함수가 아무 데서도 호출되지
// 않으면(= 기존 버그) 테스트는 조용히 통과한다. 그래서 *호출 지점*을 정적으로 확인한다.
console.log('\n이벤트 배선 (캐시가 호출되지 않으면 위 테스트는 무의미해진다)');
const src = readFileSync(WEB_PAGES, 'utf8');
const handlerBody = (re) => {
    const m = src.match(re);
    return m ? m[0] : '';
};

const fontHandler = handlerBody(/els\.fIn\.onchange = async \(e\) => \{[\s\S]*?\n        \};/);
const sizeHandler = handlerBody(/els\.sIn\.oninput = \(\) => \{[\s\S]*?\n        \};/);
const refreshFn = handlerBody(/function refreshPreviewCache\(\) \{[\s\S]*?\n        \}/);

chk('refreshPreviewCache() 정의 존재', refreshFn.length > 0);
chk('refreshPreviewCache가 refreshPreviewCache를 통해 캐시를 재구성',
    /buildGlyphCache\(/.test(refreshFn));
chk('refreshPreviewCache가 미로드 상태를 가드', /if \(!fontLoaded\) return;/.test(refreshFn));
chk('폰트 로드 핸들러가 미리보기를 갱신', /refreshPreviewCache\(\)/.test(fontHandler));
chk('크기 슬라이더 핸들러가 미리보기를 갱신', /refreshPreviewCache\(\)/.test(sizeHandler));

// 이전 버그 재발 지점: bitmapCache[ch] = ... 대입이 캐시 교체 밖에 남아 있으면 안 된다
const directWrites = (src.match(/bitmapCache\[[^\]]+\]\s*=/g) || []).length;
chk(`bitmapCache에 직접 대입하는 곳이 없음 (있으면 ${directWrites}건)`, directWrites === 0);

console.log(`\n=== 결과: ${pass} passed, ${fail} failed ===`);
process.exit(fail === 0 ? 0 : 1);