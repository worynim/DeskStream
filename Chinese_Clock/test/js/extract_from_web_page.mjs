// worynim@gmail.com
/**
 * @file extract_from_web_page.mjs
 * @brief web_pages.h에 실제로 실린 JS를 그대로 뽑아내 교차검증의 입력으로 만든다
 * @details 미러 스크립트를 따로 두면 web_pages.h와 어긋난 채로 굳어진다.
 *          검증 대상은 언제나 **배포되는 파일**이어야 하므로 여기서 소스를 뽑는다.
 *
 * @note [SYNC] 원본: ENG_Clock/test/js/extract_from_web_page.mjs
 *
 * @note NAMES는 펌웨어와 1:1이어야 하는 로직만 담는다.
 *       심볼을 지우면 이 스크립트가 즉시 실패하므로 검증을 조용히 잃는 일이 없다.
 *       **특히 NAMES를 줄이면 그만큼 검증이 사라진다** — 줄일 때는 이유를 적을 것.
 *
 * 실행: node test/js/extract_from_web_page.mjs
 * 산출: test/js/_extracted.mjs
 */
import { readFileSync, writeFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const WEB_PAGES = join(HERE, '..', '..', 'web_pages.h');

/** 펌웨어와 대응하는 심볼 */
const NAMES = [
    // 기하 상수 → renderer_geometry.cpp GEOM_288B {32,48,6,48,-8,4}
    //   (renderer.cpp의 static_assert가 헤더/config.h와의 일치를 컴파일 타점에 막는다)
    //   [순서] 이 추출기는 NAMES 순서대로 이어 붙이므로 **참조하는 상수를 먼저** 적어야 한다.
    //         LINE_TOP_Y가 SCREEN_H보다 앞에 오면 TDZ 에러가 난다.
    'GLYPH_W', 'GLYPH_H', 'RASTER_W', 'BYTES_PER_ROW', 'GLYPH_SIZE',
    'MAX_PER_LINE', 'LINE_HEIGHT', 'SCREEN_W', 'SCREEN_H', 'LINE_TOP_Y',
    // 펌웨어 xOffset(−8) = −X_OFFSET. 부호가 반대이므로 **JS가 파생값**이다.
    'X_OFFSET',
    // 디더 페이드 → display_manager.cpp ANIM_STEPS(16)
    'ANIM_STEPS',
    // 문자집합 → §4.1 간체 34자 / §4.2 번체 34자 (같은 개수!)
    //   번체는 문자열 2벌이 아니라 치환표 3쌍 + map()으로 만든다 (펌웨어 TRADITIONAL_MAP과 같은 구조)
    'CHARS_SIMPLIFIED', 'TRAD_MAP', 'CHARS_TRADITIONAL', 'charsForScript',
    // 폰트 슬롯 → 간체 /f0 ↔ 번체 /f1 (BTN3-롱 순환이 같은 역할을 한다)
    //   [주의] SLIDER_MIN/MAX/DEFAULT는 **넣지 않는다** — 웹 UI 전용이라 펌웨어 미러가 없고,
    //   한 줄 선언(`const a = 1, b = 2`)이라 이 추출기로 분리도 되지 않는다.
    'ALL_CHARS',
    // 비트 패킹 → U8g2 drawBitmap 규약 (DOM을 분리해 순수 함수로 둔다)
    'packPixels', 'packGlyph',
    // 미리보기 캐시 → 폰트·문자판이 바뀔 때마다 전부 새로 그려야 한다
    'buildGlyphCache', 'currentGlyphSize', 'currentChars', 'refreshPreviewCache',
    // 표시 방식(简体/繁體/數字) → 펌웨어 config.h PRESENTATION_* 미러 + 저장 필드 변환
    'PRESENTATION_SIMPLIFIED', 'PRESENTATION_TRADITIONAL', 'PRESENTATION_NUMERIC',
    'PRESENTATION_COUNT', 'currentPresentation', 'scriptOfPresentation', 'isWordPresentation',
    // 세로 기준 측정 → computeGlyphBaselineFor()는 CJK **34자 전부**를 잰다.
    //   CJK에는 "대문자 부분집합"이 없어 더 좁은 기준을 임의로 고를 수 없다.
    'computeGlyphBaselineFor',
    // 가로 중앙 정렬 → packGlyph의 정렬점
    'glyphCenterX', 'fontBaselineY',
    // 레이아웃 → layout_engine.cpp layoutLine()
    'ENCODER', 'DECODER', 'layoutLine',
    // 시간 표현 → chinese_time_core.cpp chtime::*
    'DIGITS', 'WEEKDAY_CHARS', 'convertChar', 'buildNumber',
    'hourToChars', 'msToChars', 'weekdayToChars', 'dayPartToChars',
    'twoDigit', 'withUnit', 'getChineseTimeStrings',
    // 그리기 → renderer.cpp drawDitheredChar() / drawSingleChar() / display_manager.cpp
    'drawDitheredChar', 'drawByLine', 'isTitleScreenOf', 'render',
    // 웹 설정 → 저장 확인 전에는 5초 폴링이 값을 되돌리지 않아야 한다
    'CONFIG_FIELDS', 'pollCanOverwrite', 'fieldToControl',
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
    if (!body) throw new Error('web_pages.h에서 raw string 리터럴을 찾지 못했다.');

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