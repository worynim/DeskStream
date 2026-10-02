// worynim@gmail.com
/**
 * @file extract_from_web_page.mjs
 * @brief web_pages.h에 실제로 실린 JS를 그대로 뽑아내 비교 테스트의 입력으로 만든다
 * @details 미러 스크립트를 따로 두면 web_pages.h와 어긋난 채로 굳어진다.
 *          검증 대상은 언제나 **배포되는 파일**이어야 하므로 여기서 소스를 뽑는다.
 *
 * @note [SYNC] 아래 NAMES는 펌웨어와 1:1이어야 하는 로직만 담는다.
 *             심볼을 지우면 이 스크립트가 즉시 실패하므로,
 *             검증을 조용히 잃는 일이 없다 (이 의도가 예외를 던지는 이유다).
 *
 * 실행: node test/js/extract_from_web_page.mjs
 * 산출: test/js/_extracted.mjs
 */
import { readFileSync, writeFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const WEB_PAGES = join(HERE, '..', '..', 'web_pages.h');

/** 펌웨어(layout_engine.cpp / english_time_core.cpp / renderer_geometry.cpp)와 대응하는 심볼 */
const NAMES = [
    // 기하 상수 → renderer_geometry.cpp GEOM_384B (래스터) · GEOM_64B (피치)
    // SCREEN_H/LINE_HEIGHT는 layout_engine.h의 LAYOUT_SCREEN_HEIGHT/LAYOUT_LINE_HEIGHT와 짝이다
    // (renderer.cpp의 static_assert가 config.h와의 일치를 컴파일 타점에 막는다)
    'GLYPH_W', 'GLYPH_H', 'RASTER_W', 'X_OFFSET', 'BYTES_PER_ROW',
    'MAX_PER_LINE', 'LINE_HEIGHT', 'SCREEN_W', 'SCREEN_H',
    // 문자집합 → 펌웨어 캐시가 런타임에 찾는 키 집합
    'UNIQ_CHARS',
    // 비트 패킹 → U8g2 drawBitmap 규약 (DOM을 분리해 순수 함수로 둔다)
    'packPixels',
    // 잉크 중앙 정렬 → [여백 수정 v4] PLAN §6.13 (순수 함수)
    // fontBaselineY는 **정렬점 절대 y**를 돌려준다 — "이동량"으로 더하면 안 된다 (§6.13e 회귀)
    'glyphCenterX', 'fontBaselineY',
    // 세로 기준 측정 → [여백 수정 v6] PLAN §6.15 — 기준 잉크 = 대문자(A–Z) 합집합.
    // 브라우저 검증 페이지(make_browser_check.mjs)가 이 함수를 그대로 호출한다.
    'computeGlyphBaselineFor',
    // 미리보기 캐시 → 폰트·크기가 바뀔 때마다 전부 새로 그려야 한다 (버그 회귀)
    'buildGlyphCache',
    // 글자 크기 → [bug 1 수정] 슬라이더 값을 그대로 쓴다 (자동 축소 fitSizeWith/commonSize는 삭제됨)
    'currentGlyphSize',
    // 웹 설정 → 저장 확인 전에는 5초 폴링이 값을 되돌리지 않아야 한다 (버그 2 회귀)
    'CONFIG_FIELDS', 'pollCanOverwrite', 'fieldToControl',
    // 레이아웃 → layout_engine.cpp layoutWrap()
    'layoutWrap',
    // 시간 표현 → english_time_core.cpp engtime::*
    'SMALL', 'TENS', 'DAYS',
    'numberToWords', 'hourToWords', 'minuteToWords', 'secondToWords',
    'amPm', 'twoDigit', 'dayName', 'numericHour', 'dateString',
];

/**
 * 최상위 선언 하나를 균형 괄호 기준으로 잘라낸다.
 * @param {string} js 추출 대상 JS 본문
 * @param {string} name 심볼 이름
 * @returns {string|null} 선언문 전체. 찾지 못하면 null.
 */
function extractDecl(js, name) {
    const fnRe = new RegExp(`function\\s+${name}\\s*\\(`);
    const conRe = new RegExp(`\\b(?:const|let)\\s+${name}\\s*=`);

    const fn = js.match(fnRe);
    const con = js.match(conRe);

    if (fn && (!con || fn.index < con.index)) {
        const brace = js.indexOf('{', fn.index);
        let depth = 0;
        for (let i = brace; i < js.length; i++) {
            if (js[i] === '{') depth++;
            else if (js[i] === '}' && --depth === 0) return js.slice(fn.index, i + 1);
        }
        return null;
    }
    if (con) {
        let depth = 0;
        for (let i = con.index + con[0].length; i < js.length; i++) {
            const ch = js[i];
            if (ch === '[' || ch === '(') depth++;
            else if (ch === ']' || ch === ')') depth--;
            else if ((ch === '\n' || ch === ';') && depth <= 0) return js.slice(con.index, i);
        }
    }
    return null;
}

function main() {
    const src = readFileSync(WEB_PAGES, 'utf8');

    const body = src.split('R"rawliteral(', 2)[1];
    if (!body) throw new Error('web_pages.h에서 raw string 리터RAL을 찾지 못했다.');

    const script = body.match(/<script>([\s\S]*?)<\/script>/);
    if (!script) throw new Error('web_pages.h에서 <script> 블록을 찾지 못했다.');

    const missing = [];
    const parts = NAMES.map((name) => {
        const decl = extractDecl(script[1], name);
        if (!decl) { missing.push(name); return ''; }
        return decl;
    });

    if (missing.length > 0) {
        console.error(`FAIL: web_pages.h에서 정의를 찾지 못한 심볼 — ${missing.join(', ')}`);
        console.error('      웹 페이지 로직이 바뀌면 NAMES 목록도 함께 갱신해야 한다.');
        process.exit(1);
    }

    const out = parts.join('\n\n') + `\n\nexport { ${NAMES.join(', ')} };\n`;
    writeFileSync(join(HERE, '_extracted.mjs'), out);
    console.log(`추출 완료: ${NAMES.length}개 심볼 → test/js/_extracted.mjs`);
}

main();