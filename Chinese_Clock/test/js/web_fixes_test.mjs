// worynim@gmail.com
/**
 * @file web_fixes_test.mjs
 * @brief 슬롯↔문자판 연동 제거와 글자 검증(2026-10-05 사용자 보고 2건)의 회귀 검증
 *
 * @note [SYNC] 원본: ENG_Clock/test/js/web_fixes_test.mjs
 * @note 순수 함수는 web_pages.h에서 직접 추출(_extracted.mjs)해 호출하고,
 *       DOM 배선(어느 핸들러가 어디에 붙는지)은 **핸들러를 떼어내 실제로 돌려** 본다.
 *       정적 정규식으로만 보면 "saveConfig()가 어딘가에 있다"까지만 확인된다 —
 *       실제로 고쳐야 했던 결함은 **조건문 안에 들어 있어 실행되지 않았다**는 것이라
 *       정적 검사로는 잡히지 않는다.
 */
import { readFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import { CHARS_SIMPLIFIED, CHARS_TRADITIONAL, ALL_CHARS, CONFIG_FIELDS,
         PRESENTATION_SIMPLIFIED, PRESENTATION_TRADITIONAL, PRESENTATION_NUMERIC,
         PRESENTATION_COUNT, scriptOfPresentation, isWordPresentation } from './_extracted.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, '..', '..');
const src = readFileSync(join(ROOT, 'web_pages.h'), 'utf8');

let pass = 0, fail = 0;
const chk = (name, ok) => {
    if (ok) pass++;
    else { fail++; console.error(`  FAIL  ${name}`); }
};

/** `TARGET = ` 뒤의 화살표/함수 본문을 균형 괄호로 잘라낸다 */
function grabAssigned(target) {
    return grabFrom(src, target);
}

/** `marker` 위치부터 균형 괄호로 블록을 잘라낸다 */
function grabFrom(text, marker) {
    const at = text.indexOf(marker);
    if (at === -1) return null;
    const brace = text.indexOf('{', at);
    if (brace === -1) return null;
    let depth = 0;
    for (let i = brace; i < text.length; i++) {
        if (text[i] === '{') depth++;
        else if (text[i] === '}' && --depth === 0) return text.slice(at, i + 1);
    }
    return null;
}

/** `TARGET = <식>;` 에서 우변만 떼어낸다 */
function stripAssign(assignStmt) {
    const eq = assignStmt.indexOf('=');
    let body = assignStmt.slice(eq + 1).trim();
    if (body.endsWith(';')) body = body.slice(0, -1);
    return body;
}

// ===================================================================
// 결함 A — 문자판을 바꾸면 Storage slot이 멋대로 바뀐다 (2026-10-05)
// ===================================================================
console.log('결함 A: 문자판 변경이 슬롯을 건드리지 않는다');

// 계약이 **반대로** 뒤집혔다. 예전 테스트는 "슬롯이 따라간다"를 요구했는데,
//   사용자가 "Storage slot이 멋대로 바뀐다"고 보고 그 연동을 제거했다.
const handlerSrc = grabAssigned('els.script.onchange');
chk('els.script.onchange 핸들러가 존재한다', handlerSrc !== null);

if (handlerSrc) {
    const run = (scriptValue, slotValue) => {
        const calls = [];
        const els = { script: { value: scriptValue }, slot: { value: slotValue } };
        const scope = {
            els, parseInt, String,
            saveConfig: () => calls.push('save'),
            rebuildInventory: () => calls.push('inv'),
            charsForScript: () => 'CHARS',
            refreshPreviewCache: () => calls.push('cache'),
        };
        // eslint-disable-next-line no-new-func
        new Function(...Object.keys(scope), `(${stripAssign(handlerSrc)})()`)(
            ...Object.values(scope));
        return { calls, slot: els.slot.value };
    };

    // 슬롯이 어떤 값이든, 문자판을 바꿔도 **한 글자도** 바뀌지 않아야 한다
    chk('간체 슬롯 0 상태에서 번체를 골라도 슬롯이 그대로다', run('1', '0').slot === '0');
    chk('번체 슬롯 1 상태에서 간체를 골라도 슬롯이 그대로다', run('0', '1').slot === '1');
    chk('문자판을 골라도 재고 목록을 다시 세우지 않는다', !run('1', '0').calls.includes('inv'));
    chk('문자판을 고르면 저장은 한다 (이게 빠지면 5초 폴링이 되돌린다)',
        run('1', '1').calls.includes('save'));
    chk('문자판을 고르면 미리보기를 다시 그린다', run('1', '1').calls.includes('cache'));
}

// 계약: 전용 핸들러가 있는 필드는 일반 바인딩에서 **제외**돼야 한다.
//   포함되면 전용 핸들러가 덮어써져 어느 쪽이 이기는지 알 수 없다.
const bindLine = src.match(/CONFIG_FIELDS\.[a-zA-Z]+\(([^)]*)\)\s*\.forEach/);
chk('일반 바인딩이 script를 명시적으로 제외한다',
    bindLine !== null && bindLine[1].includes("'script'"));
chk('일반 바인딩이 slot도 제외한다 (전용 핸들러가 배지를 지워야 한다)',
    bindLine !== null && bindLine[1].includes("'slot'"));
chk('CONFIG_FIELDS에 presentation 행이 있다 (3단계 표시 방식)',
    CONFIG_FIELDS.some(f => f.key === 'presentation' && f.el === 'script'));

// ===================================================================
// 결함 B — 슬롯에 한 문자판만 올라가 두 설정이 서로를 못 쓴다 (2026-10-05)
// ===================================================================
console.log('결함 B: 슬롯에는 두 문자판의 합집합이 올라간다');

// 슬롯 하나가 어느 문자판이든 되려면 **합집합**이어야 한다.
//   문자판별로 34자씩 올리면 슬롯이 문자판에 종속돼 버린다(사용자 제안으로 확정된 방향).
chk('ALL_CHARS는 두 문자판의 합집합이다',
    CHARS_SIMPLIFIED.every(c => ALL_CHARS.includes(c))
    && CHARS_TRADITIONAL.every(c => ALL_CHARS.includes(c)));
chk('ALL_CHARS에 중복이 없다', new Set(ALL_CHARS).size === ALL_CHARS.length);
chk('합집합 크기는 37 (34 + 번체 전용 3)',
    ALL_CHARS.length === 37
    && CHARS_SIMPLIFIED.length === 34 && CHARS_TRADITIONAL.length === 34);
chk('ALL_CHARS는 펌웨어가 실제로 쓰는 37자와 같을 개수다',
    ALL_CHARS.length === 37);

// 업로드 목록은 문자판이 아니라 합집합이어야 한다
chk('초기 재고 목록이 ALL_CHARS다', /rebuildInventory\(ALL_CHARS\)/.test(src));
chk('업로드가 charsForScript(문자판별 34자)를 쓰지 않는다',
    !/rebuildInventory\(\s*charsForScript/.test(src));

// 배지·캐시·업로드·세로 기준이 **전부 같은 37자**를 봐야 한다.
//   셋 중 하나만 34자면 서로 어긋난다 — 배지 37개 중 3개는 영영 업로드되지 않고,
//   캐시 밖 글자는 업로드 시 라이브 렌더로 새겨 미리보기와 결과가 달라질 수 있다.
//   (실제 결함: 배지는 37자인데 업로드 루프가 34자만 돌았다)
chk('업로드 루프가 ALL_CHARS를 쓴다',
    /async function processAll\(\)\s*\{[\s\S]{0,200}const chars = ALL_CHARS;/.test(src));
chk('미리보기 캐시가 ALL_CHARS를 쓴다',
    /function refreshPreviewCache\(\)\s*\{[\s\S]{0,300}buildGlyphCache\(ALL_CHARS/.test(src));
chk('세로 기준 측정이 ALL_CHARS를 쓴다',
    /function computeGlyphBaselineFor[\s\S]{0,400}fontBaselineY\(ALL_CHARS/.test(src));

// 펌웨어 쪽에도 문자판→슬롯 규칙이 남아 있으면 안 된다 (서로를 엮지 않는다)
const cfg = {};
for (const line of readFileSync(join(ROOT, 'config.h'), 'utf8').split('\n')) {
    const m = line.match(/^\s*#define\s+([A-Z_0-9]+)\s+(-?\d+)/);
    if (m) cfg[m[1]] = Number(m[2]);
}
chk('config.h에 문자판→슬롯 상수가 남아 있지 않다',
    cfg.SCRIPT_SLOT_SIMPLIFIED === undefined && cfg.SCRIPT_SLOT_TRADITIONAL === undefined);
chk('웹에 SLOT_FOR_SCRIPT 미러가 남아 있지 않다', !/SLOT_FOR_SCRIPT/.test(src));

// ===================================================================
// 결함 C — 펌웨어 API가 표시 방식을 받거나 보내지 않는다
// ===================================================================
console.log('결함 C: 펌웨어 API가 표시 방식을 다루고 슬롯은 안 건드린다');

const wm = readFileSync(join(ROOT, 'web_manager.cpp'), 'utf8');
chk('/api/config 응답에 presentation이 있다', /\\"presentation\\":/.test(wm));
chk('presentation 입력을 검증한다', /applyPresentation/.test(wm));
chk('범위 밖 값은 거부한다 (0 ≤ v < PRESENTATION_COUNT)',
    /v < 0 \|\| v >= PRESENTATION_COUNT/.test(wm));
chk('본문 없는 POST에 400을 답한다', /hasArg\("plain"\)\)[\s\S]{0,120}send\(400/.test(wm));

// 두 저장 필드를 따로 받지 않는다 — 하나를 다른 길로 받으면 규칙이 두 벌이다.
chk('display_mode를 별도 입력으로 받지 않는다 (setDisplayMode 경로 없음)',
    !/parseVal\(body, "display_mode"\)/.test(wm) && !/setDisplayMode/.test(wm));
chk('script_type을 별도 입력으로 받지 않는다 (applyScriptType 없음)',
    !/applyScriptType/.test(wm));

const dm = readFileSync(join(ROOT, 'display_manager.cpp'), 'utf8');
// setPresentation 본문을 떼어 "같은 함수 안에서"를 거리 추측 없이 묶는다.
//   (거리 기반 정규식은 함수에 주석 한 줄만 넣어도 조용히 깨진다 — 실제로 한 번 깨졌다.)
const setPresentation = grabFrom(dm, 'DisplayManager::setPresentation');
chk('setPresentation을 찾는다', setPresentation !== null);
chk('표시 방식을 바꿀 때 슬롯을 옮기지 않는다',
    setPresentation !== null && !/setFontSlot/.test(setPresentation));

// ===================================================================
// 결함 D — 없는 글자로 넘어가 조용히 빈칸이 된다 (시 단위 글자 소실)
// ===================================================================
console.log('결함 D: 한자 모드 진입은 그 문자판의 글자가 있을 때만');

if (setPresentation) {
    // 계약: 슬롯을 옮기지 않으므로 남는 유일한 안전장치는 **검증**이다.
    //   순서가 뒤집히면(대입 후 검사) 이미 깨진 상태로 넘어간 뒤에 되돌릴 수 없다.
    const probeAt = setPresentation.indexOf('missingGlyphFor');
    const assignAt = setPresentation.indexOf('script_type = script');
    chk('missingGlyphFor로 검증한다', probeAt !== -1);
    chk('검증이 script_type 대입보다 먼저다',
        probeAt !== -1 && assignAt !== -1 && probeAt < assignAt);
    // 검증 실패 → 조기 반환. 그래야 캐시도 설정도 그대로다.
    chk('검증에 실패하면 즉시 반환한다',
        /missingGlyphFor\([^)]*\)[\s\S]*?\breturn;/.test(setPresentation));
    // 실패를 사용자에게 보이게 한다 — 조용히 실패하지 않는다
    chk('거부 사실을 사용자에게 알린다', /showStatus/.test(setPresentation));
    // 숫자 모드는 **폰트 없이도** 열려야 한다. 검증에 걸리면 폰트를 한 번도
    //   올리지 않은 기기에서 BTN2 버튼이 dead가 된다.
    chk('숫자 모드는 검증을 건너뛴다 (p != PRESENTATION_NUMERIC 가드)',
        /if \(p != PRESENTATION_NUMERIC\)/.test(setPresentation));
    chk('presentation() 역매핑이 숫자 모드를 display_mode에서 본다',
        /display_mode == CLOCK_MODE_NUMERIC\) return PRESENTATION_NUMERIC/.test(dm));
}

const rc = readFileSync(join(ROOT, 'renderer.cpp'), 'utf8');
// 판정 기준이 "지금 캐시에 있는가"로 통일돼 있어야 한다.
//   FS 조회나 문자집합 상수로 판정하면 펌웨어와 웹이 다른 말을 하게 된다.
chk('캐시 조회를 쓴다 (FS 스캔이 아니다)', /bool Renderer::hasGlyph/.test(rc));
chk('hasGlyph는 캐시를 바꾸지 않는다 (findChar만 본다)',
    /bool Renderer::hasGlyph[\s\S]{0,120}findChar/.test(rc));

const core = readFileSync(join(ROOT, 'chinese_time_core.h'), 'utf8');
chk('코어가 문자판 전용 글자 목록을 노출한다', /scriptOnlyGlyph/.test(core));

// ===================================================================
// 결함 E — 슬롯을 바꿔도 배지 상태가 이전 슬롯 것을 물려받는다 (2026-10-05)
// ===================================================================
console.log('결함 E: 슬롯을 바꾸면 배지 상태를 초기화한다');

// "업로드됨" 배지는 **슬롯마다 다른 사실**인데 배지 DOM 하나만 있다.
//   초기화하지 않으면 slot0에서 올린 글자가 slot1에도 있는 것처럼 보인다.
const slotHandler = grabAssigned('els.slot.onchange');
chk('els.slot.onchange 핸들러가 존재한다', slotHandler !== null);
if (slotHandler) {
    const run = (slotValue) => {
        const calls = [];
        const els = { slot: { value: slotValue } };
        const scope = {
            els, saveConfig: () => calls.push('save'),
            clearInventoryState: () => calls.push('clear'),
        };
        // eslint-disable-next-line no-new-func
        new Function(...Object.keys(scope), `(${stripAssign(slotHandler)})()`)(
            ...Object.values(scope));
        return calls;
    };
    const calls = run('1');
    chk('슬롯을 바꾸면 배지 상태를 지운다', calls.includes('clear'));
    chk('슬롯을 바꾸면 저장도 한다', calls.includes('save'));
    chk('슬롯을 바꾸면 재고를 다시 만들지 않는다 (지우기만)', !calls.includes('inv'));
}
chk('clearInventoryState가 존재한다', /function clearInventoryState\(\)/.test(src));
chk('clearInventoryState는 active 표시를 지운다',
    /function clearInventoryState\(\)[\s\S]{0,200}remove\('active'\)/.test(src));

// ===================================================================
// 결함 F — 글리프는 /f1에, 이름표는 /f0에 남는다 (2026-10-05)
// ===================================================================
console.log('결함 F: 폰트 이름이 업로드한 슬롯에 기록된다');

// 펌웨어는 이름표를 `configManager.font_slot`에 쓴다. 업로드는 `?slot=` 파라미터로
//   슬롯을 받는다. 둘이 같은 출처가 아니면(웹 select 값 vs 펌웨어 설정값) 어긋난다.
const processAll = grabFrom(src, 'async function processAll()');
chk('processAll을 찾는다', processAll !== null);
if (processAll) {
    // 슬롯을 **한 번만** 읽어 고정해야 한다. 37회 업로드 중 바뀔 수 있다.
    chk('업로드 대상 슬롯을 시작 시점에 한 번만 읽는다',
        processAll.indexOf('const targetSlot') !== -1);
    chk('모든 업로드가 고정한 슬롯을 쓴다',
        /fetch\(`\/upload\?slot=\$\{targetSlot\}/.test(processAll)
        && !/upload\?slot=\$\{els\.slot/.test(processAll));

    // [2026-10-05 2차] 이름은 **업로드와 같은 요청**에 실려야 한다.
    //   1차 수정은 이름을 별도 /api/config POST로 보냈고 그마저 font 파일이 선택된
    //   경우에만 나갔다 → 그 요청이 빠지면 슬롯에 글리프만 남고 이름표가 없어
    //   드롭다운이 "Empty Slot"으로 보였다(사용자 보고: 폰트를 넣었는데 비어 보인다).
    //   이제 ?font= 로 같은 요청에 실리므로 도착점이 어긋날 수 없다.
    chk('폰트 이름이 업로드와 **같은 요청**에 실린다',
        /upload\?slot=\$\{targetSlot\}&font=\$\{encodeURIComponent\(/.test(processAll));
    chk('이름도 슬롯처럼 시작 시점에 한 번만 읽어 고정한다',
        /const fontName = els\.fIn\.files\[0\]/.test(processAll));
    chk('폰트 미선택이면 업로드를 시작하지 않는다 (조용히 빈 슬롯을 만들지 않는다)',
        /if \(!fontName\)/.test(processAll) && /return;/.test(processAll));
    // 이름을 보낼 때 슬롯을 함께 실어 보내야 펌웨어가 같은 슬롯에 기록한다
    chk('font_name POST가 font_slot을 함께 보낸다',
        /JSON\.stringify\(\{[^}]*font_slot[^}]*font_name/.test(processAll));
}

// 펌웨어 — 이름표를 **업로드한 슬롯**에 쓴다. setFontName()은 현재 슬롯에 쓰므로
//   업로드 경로가 그걸 부르면 이름표가 엉뚱한 폴더로 간다(이 결함의 원래 형태).
const uploadData = grabFrom(wm, 'void WebManager::handleUploadData');
chk('업로드 핸들러에서 ?font= 를 읽는다',
    uploadData !== null && /server\.arg\("font"\)/.test(uploadData));
chk('업로드가 연 슬롯을 기억한다', uploadData !== null && /uploadSlot = \(uint8_t\)slot/.test(uploadData));
chk('이름표를 그 슬롯에 쓴다 (setFontName이 아니라 setSlotName)',
    uploadData !== null && /display\.setSlotName\(uploadSlot, uploadFontName\)/.test(uploadData));
chk('업로드 경로가 setFontName()을 부르지 않는다',
    uploadData !== null && !/display\.setFontName\(/.test(uploadData));
chk('이름표 거부를 조용히 넘기지 않는다 (로그를 남긴다)',
    uploadData !== null && /Slot name rejected/.test(uploadData));

const setSlotName = grabFrom(dm, 'bool DisplayManager::setSlotName');
chk('setSlotName이 슬롯 범위를 검사한다',
    setSlotName !== null && /slot >= FONT_SLOT_COUNT/.test(setSlotName));
chk('setSlotName이 이름표를 지정 슬롯 폴더에 쓴다',
    setSlotName !== null && /"\/f" \+ String\(slot\) \+ "\/name\.txt"/.test(setSlotName));
chk('setSlotName이 _slotNames를 갱신한다',
    setSlotName !== null && /_slotNames\[slot\] = name/.test(setSlotName));
chk('setSlotName은 현재 슬롯일 때만 config를 건드린다',
    setSlotName !== null && /font_slot == slot/.test(setSlotName));
chk('setSlotName이 이미 같은 값이면 파일을 다시 쓰지 않는다 (37회 호출)',
    setSlotName !== null && /_slotNames\[slot\] == name\) return true/.test(setSlotName));
chk('setFontName이 setSlotName에 위임한다',
    /void DisplayManager::setFontName[\s\S]{0,200}setSlotName\(configManager\.get\(\)\.font_slot/.test(dm));

// -------------------------------------------------------------------
// 결함 F-2 — 32바이트 상한이 실제 폰트 이름을 거부했다 (2026-10-05 실기)
// -------------------------------------------------------------------
// 실기 증상: 슬롯에 폰트를 올렸는데 드롭다운이 "Empty Slot". 시리얼이 원인을 말해 줬다 —
//   `[WEB] Slot name rejected: slot=0 name='MFXuanRen_Noncommercial-Regular.ttf' (len=35)`.
// 35바이트 > 31 이었다. 그 상한의 근거 주석("이름을 파일명으로도 쓴다")은 **사실이 아니었다** —
//   경로는 리터럴이고 이름은 파일의 **내용**으로만 들어간다. 그래서 상한을 config.h 한 곳으로 옮겼다.
const cfgH = readFileSync(join(ROOT, 'config.h'), 'utf8');
const nameMaxM = /#define\s+FONT_NAME_MAX_LEN\s+(\d+)/.exec(cfgH);
chk('이름 길이 상한이 config.h 한 곳(FONT_NAME_MAX_LEN)에 있다', nameMaxM !== null);
chk('이름 상한이 31바이트 시절로 되돌아가지 않았다 (len=35 이름이 실제로 거부됐다)',
    nameMaxM !== null && Number(nameMaxM[1]) >= 64);
chk('setSlotName이 상한 리터럴이 아니라 FONT_NAME_MAX_LEN을 쓴다',
    setSlotName !== null && /name\.length\(\) > FONT_NAME_MAX_LEN/.test(setSlotName)
    && !/name\.length\(\)\s*>=\s*32/.test(dm));
// 거부는 조용히 넘어가면 안 된다 — 이번 진단이 어려웠던 이유가 그 침묵이었다.
chk('이름표 거부 로그에 실제 길이와 상한이 함께 나온다',
    /Slot name rejected[\s\S]{0,200}len=%u[\s\S]{0,300}FONT_NAME_MAX_LEN/.test(wm));
// 이름이 안 왔을 때도 START에서 무조건 찍는다 (침묵 금지).
chk('업로드 시작 시 font 인자 도착 여부를 무조건 로그로 남긴다',
    /\[WEB\] upload start slot=%d fontArg=%d/.test(wm));

// 펌웨어 쪽 순서 계약 — 슬롯을 먼저 적용해야 이름표도 그 슬롯에 쓰인다.
const handleSetConfig = grabFrom(wm, 'void WebManager::handleSetConfig');
chk('handleSetConfig에서 슬롯을 이름보다 먼저 적용한다',
    handleSetConfig !== null
    && handleSetConfig.indexOf('applyIntSettings') !== -1
    && handleSetConfig.indexOf('applyIntSettings') < handleSetConfig.indexOf('applyFontName'));

// 슬롯을 옮길 때 _slotNames도 갱신되어야 /api/config의 slotNames가 사실과 맞는다.
const setFontSlot = grabFrom(dm, 'DisplayManager::setFontSlot');
chk('setFontSlot이 _slotNames를 갱신한다',
    setFontSlot !== null && /_slotNames\[slot\]\s*=/.test(setFontSlot));
chk('빈 슬롯 표기가 두 곳에서 같다 (setFontSlot vs 부팅 캐시)',
    setFontSlot !== null && /name = "Empty Slot"/.test(setFontSlot)
    && /_slotNames\[i\] = "Empty Slot"/.test(dm));

// ===================================================================
// 결함 G — BTN2 short에 숫자가 없다 / 3단계가 두 곳에 흩어져 있다 (2026-10-05)
// ===================================================================
console.log('결함 G: 표시 방식은 3단계 하나로, 버튼·웹이 같은 값을 쓴다');

// 펌웨어 상수와 웹 미러가 어긋나면 웹은 다른 말을 하게 된다.
chk('config.h에 PRESENTATION 3단계가 정의된다',
    cfg.PRESENTATION_SIMPLIFIED === 0 && cfg.PRESENTATION_TRADITIONAL === 1
    && cfg.PRESENTATION_NUMERIC === 2 && cfg.PRESENTATION_COUNT === 3);
chk('웹 PRESENTATION_*가 config.h와 같은 값이다',
    PRESENTATION_SIMPLIFIED === cfg.PRESENTATION_SIMPLIFIED
    && PRESENTATION_TRADITIONAL === cfg.PRESENTATION_TRADITIONAL
    && PRESENTATION_NUMERIC === cfg.PRESENTATION_NUMERIC
    && PRESENTATION_COUNT === cfg.PRESENTATION_COUNT);

// BTN2 short = 3칸 순환. 예전 "Font Required!" 가드가 남으면 숫자 칸이 dead가 된다.
const ino = readFileSync(join(ROOT, 'Chinese_Clock.ino'), 'utf8');
const btn2short = grabFrom(ino, 'void btn2_short()');
chk('btn2_short를 찾는다', btn2short !== null);
chk('BTN2 short가 PRESENTATION_COUNT로 순환한다',
    btn2short !== null && /\(display\.presentation\(\) \+ 1\) % PRESENTATION_COUNT/.test(btn2short));
chk('BTN2 short에 폰트 선행 가드가 없다 (숫자 모드는 폰트가 없어도 열린다)',
    btn2short !== null && !/isCacheLoaded/.test(btn2short));
chk('버튼과 웹이 같은 진입점을 쓴다 (setPresentation)',
    btn2short !== null && /display\.setPresentation\(next\)/.test(btn2short));

// 순환이 3칸을 실제로 도는지 — 상수만 있어서는 순환을 보장하지 못한다.
const cycle = [0, 1, 2].map(p => (p + 1) % 3);
chk('순환이 0→1→2→0으로 한 바퀴 돈다',
    cycle[0] === 1 && cycle[1] === 2 && cycle[2] === 0);

// 웹: 6번 "Display type" 항목은 없어지고 3번이 3단계다. 항목이 두 벌이면
//   같은 설정(표시 방식)이 두 곳에서 제어되어 어느 쪽이 이겼는지 알 수 없다.
chk('웹에 displayMode select이 남아 있지 않다', !/id="displayMode"/.test(src));
chk('웹에 els.disp가 남아 있지 않다', !/els\.disp/.test(src));
chk('CONFIG_FIELDS에 display_mode 행이 없다',
    !CONFIG_FIELDS.some(f => f.key === 'display_mode'));
chk('3번 항목이 3단계 옵션을 갖는다 (简体/繁體/數字)',
    (src.match(/<option value="0">简体/g) || []).length === 1
    && /<option value="1">繁體/.test(src)
    && /<option value="2">數字/.test(src));
chk('3번 항목 라벨이 BTN2 short를 알린다', /3\. Character set \(BTN2 short\)/.test(src));
chk('4번 항목 라벨이 BTN3 long을 알린다', /4\. Storage slot \(BTN3 long\)/.test(src));

// 번호가 1..11 으로 끊김 없이 이어져야 한다 (12번 "Display type" 삭제로 하나 줄었다).
const nums = [...src.matchAll(/<label>(\d+)\./g)].map(m => Number(m[1]));
chk('메뉴 번호가 1..11 으로 중복 없이 이어진다',
    nums.length === 11 && nums.every((n, i) => n === i + 1));

// 미리보기도 같은 3단계를 본다 — 여기서 어긋나면 웹이 기기와 다른 말을 한다.
chk('isWord는 표시 방식에서 온다 (disp select 아님)',
    /const isWord = isWordPresentation\(currentPresentation\(\)\)/.test(src));
chk('script_type은 표시 방식에서 환산된다',
    /const sc = scriptOfPresentation\(currentPresentation\(\)\)/.test(src));
chk('scriptOfPresentation은 숫자(數字)도 한 문자판을 물려받는다',
    scriptOfPresentation(PRESENTATION_NUMERIC) === scriptOfPresentation(PRESENTATION_SIMPLIFIED));
chk('isWordPresentation은 숫자만 false다',
    isWordPresentation(PRESENTATION_SIMPLIFIED) === true
    && isWordPresentation(PRESENTATION_TRADITIONAL) === true
    && isWordPresentation(PRESENTATION_NUMERIC) === false);

// ===================================================================
// 결함 H — Font Studio가 백지로 뜬다 (2026-10-05 실기 보고)
// ===================================================================
console.log('결함 H: PROGMEM 페이지를 힙에 복사하지 않는다');

// 실기 증상: 시리얼 로그에 [WEB] WebManager started까지 정상이고 크래시도 없는데
//   브라우저만 백지. 서버는 떴고 응답만 못 갔다.
const handleRoot = grabFrom(wm, 'void WebManager::handleRoot');
chk('handleRoot를 찾는다', handleRoot !== null);
chk('send_P로 보낸다 (67KB를 힙에 복사하지 않는다)',
    handleRoot !== null && /server\.send_P\(/.test(handleRoot));
// send()는 ESP32 WebServer 코어에서 `(String)content` 로 **전체 페이지를 힙에 복사**한다.
//   이 프로젝트는 Flash 99% · I2C 버퍼 4장 상시 점유 상태라 연속 67KB 복사가 실패한다.
chk('send()로 PROGMEM 페이지를 보내지 않는다 (백지 화면의 직접 원인)',
    handleRoot !== null && !/server\.send\(\s*200/.test(handleRoot));
chk('content_type도 PSTR로 준다',
    handleRoot !== null && /PSTR\("text\/html"\)/.test(handleRoot));

// 페이지가 PROGMEM인 사실 자체를 고정 — 일반 배열로 내려가면 send()가 정당해져
//   회귀 테스트가 조용히 통과해 버린다.
chk('font_studio_html이 PROGMEM 상수다',
    /const char font_studio_html\[\] PROGMEM/.test(readFileSync(join(ROOT, 'web_pages.h'), 'utf8')));

console.log(`web_fixes_test: ${pass} passed, ${fail} failed`);
process.exit(fail === 0 ? 0 : 1);