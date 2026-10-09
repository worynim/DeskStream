// worynim@gmail.com
/**
 * @file extract_from_web_page.mjs
 * @brief web_pages.h에 실제로 실린 JS를 그대로 뽑아 비교 테스트의 입력으로 만든다
 * @details [신설 2026-10-06] 이 판에는 JS 하네스가 없었다(중국어판 PLAN §📌 C).
 *          미러 스크립트를 따로 두면 web_pages.h와 어긋난 채로 굳어지므로,
 *          검증 대상은 언제나 **배포되는 파일**이어야 한다.
 *
 * @note 산출물 두 개:
 *        · `_extracted.mjs`   — 펌웨어와 1:1이어야 하는 심볼(테스트가 import 한다).
 *          심볼을 지우면 이 스크립트가 즉시 실패한다 — 검증을 조용히 잃지 않는다.
 *        · `_full_script.mjs` — `<script>` 전문. run_all.sh가 `node --check`로 **문법**을 본다.
 *          중국어판 §12.6 #3: 선언 순서(TDZ) 하나로 `<script>` 블록 전체가 죽어
 *          Font Studio가 아예 동작하지 않았는데, 컴파일도 정적 검사도 통과했다.
 *          문법 오류는 이 검사가, 선언 **순서**는 브라우저 실행이 잡는다.
 *
 * 실행: node test/js/extract_from_web_page.mjs
 */
import { readFileSync, writeFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HERE = dirname(fileURLToPath(import.meta.url));
const WEB_PAGES = join(HERE, '..', '..', 'web_pages.h');

/**
 * 펌웨어와 대응해 **테스트가 직접 호출**하는 심볼.
 * @note 지금은 문자집합 하나다 — 업로드 루프와 배지 id 규약이 이 목록을 함께 본다.
 *       (눈 조립·분할 플랩의 픽셀 운동학은 아직 `renderer.cpp` 안에 있어 순수 모듈이
 *        아니므로 대상이 아니다. 분리되면 여기에 추가한다.)
 */
const NAMES = [
    'UNIQ_CHARS',
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
    // 문법 검사용 전문 — export를 붙이지 않는다 (`node --check`는 파싱만 한다).
    writeFileSync(join(HERE, '_full_script.mjs'), script[1]);
    console.log(`추출 완료: ${NAMES.length}개 심볼 → _extracted.mjs, <script> 전문 → _full_script.mjs`);
}

main();
