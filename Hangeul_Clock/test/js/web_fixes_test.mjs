// worynim@gmail.com
/**
 * @file web_fixes_test.mjs
 * @brief 한글판 폰트 업로드·슬롯 결함(2026-10-06 전파)의 회귀 검증
 *
 * @details 중국어판 PLAN §📌 의 언어 중립 수정을 한글판에 옮기며 만든 **첫 JS 하네스**다
 *          (이 판에는 `test/js/` 자체가 없었다 — PLAN §📌 C).
 *
 *   A-1  `handleRoot()`가 PROGMEM 페이지를 `send()`로 힙에 복사 → 실기 백지 화면
 *   A-5  본문 없는 POST에 아무 응답도 없어 `fetch()`가 저장 실패처럼 보인다
 *   A-2  폰트 이름표가 **업로드한 슬롯**이 아니라 **현재 슬롯**에 쓰인다 (①~④)
 *   A-3  `setFontSlot()`이 `_slotNames[]`를 갱신하지 않아 `slot_names`가 부팅 값으로 멈춘다
 *   A-4  빈 슬롯 표기가 `"Empty"` / `"Empty Slot"` 두 갈래로 갈라져 있다
 *   B-1  슬롯을 바꿔도 "업로드됨" 배지가 이전 슬롯 것을 물려받는다
 *
 * @note 순수 함수는 `_extracted.mjs`에서, 배선은 **핸들러를 떼어내 실제로 돌려** 본다.
 *       정적 정규식만으로는 "호출이 조건문 안에 들어 있어 실행되지 않았다"를 못 잡는다 —
 *       중국어판에서 실제로 그랬다(§12.7 결함 A).
 */
import { readFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import { UNIQ_CHARS } from './_extracted.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, '..', '..');
const src   = readFileSync(join(ROOT, 'web_pages.h'), 'utf8');
const wm    = readFileSync(join(ROOT, 'web_manager.cpp'), 'utf8');
const dm    = readFileSync(join(ROOT, 'display_manager.cpp'), 'utf8');
const cfgH  = readFileSync(join(ROOT, 'config.h'), 'utf8');

let pass = 0, fail = 0;
const chk = (name, ok) => {
    if (ok) pass++;
    else { fail++; console.error(`  FAIL  ${name}`); }
};

/** `marker` 위치부터 **균형 괄호**로 블록을 잘라낸다 (거리 기반 정규식은 주석 한 줄에 깨진다) */
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
// A-1 — Font Studio가 백지로 뜬다 (중국어판 §12.12 실기 보고)
// ===================================================================
console.log('A-1: PROGMEM 페이지를 힙에 복사하지 않는다 (send_P)');

// 실기 증상: 서버는 정상 기동하고 크래시도 없는데 브라우저만 백지.
//   ESP32 WebServer 코어의 send(code, type, const char*)는 첫 줄에서
//   `const String passStr = (String)content;`로 **전체 페이지를 힙에 복사**한다.
//   이 프로젝트는 I2C 버퍼 4장을 상시 점유하므로 연속 40~70KB 할당이 실패하기 쉽다.
const handleRoot = grabFrom(wm, 'void WebManager::handleRoot');
chk('handleRoot를 찾는다', handleRoot !== null);
chk('send_P로 보낸다 (PROGMEM을 힙에 복사하지 않는다)',
    handleRoot !== null && /server\.send_P\(200, PSTR\("text\/html"\), font_studio_html\)/.test(handleRoot));
chk('send()로 PROGMEM 페이지를 보내지 않는다 (백지 화면의 직접 원인)',
    handleRoot !== null && !/server\.send\(/.test(handleRoot));
chk('font_studio_html이 PROGMEM 상수다',
    /const char font_studio_html\[\] PROGMEM/.test(src));

// ===================================================================
// A-5 — 본문 없는 POST에 아무 응답도 없다 (중국어판 §12.7 결함 B)
// ===================================================================
console.log('A-5: 본문 없는 POST는 400으로 명시적으로 거절한다');

const handleSetConfig = grabFrom(wm, 'void WebManager::handleSetConfig');
chk('handleSetConfig를 찾는다', handleSetConfig !== null);
chk('본문 없는 POST에 400 Missing body로 답한다',
    handleSetConfig !== null
    && /else \{[\s\S]{0,400}send\(400, "text\/plain", "Missing body"\)/.test(handleSetConfig));
chk('본문이 있으면 200 OK를 그대로 답한다',
    handleSetConfig !== null && /send\(200, "text\/plain", "OK"\)/.test(handleSetConfig));

// ===================================================================
// B-1 — 슬롯을 바꿔도 배지가 이전 슬롯 것을 물려받는다 (중국어판 §12.9 보고 ③)
// ===================================================================
console.log('B-1: 슬롯을 바꾸면 배지 상태를 초기화한다');

const slotHandler = grabFrom(src, 'els.slot.onchange');
chk('els.slot.onchange 핸들러가 존재한다', slotHandler !== null);
if (slotHandler) {
    const run = () => {
        const calls = [];
        const els = { slot: { value: '1' } };
        const scope = {
            els, saveConfig: () => calls.push('save'),
            clearInventoryState: () => calls.push('clear'),
        };
        // eslint-disable-next-line no-new-func
        new Function(...Object.keys(scope), `(${stripAssign(slotHandler)})()`)(
            ...Object.values(scope));
        return calls;
    };
    const calls = run();
    chk('슬롯을 바꾸면 배지 상태를 지운다', calls.includes('clear'));
    chk('슬롯을 바꾸면 저장도 한다 (전용 핸들러가 saveConfig를 대신한다)', calls.includes('save'));
}
chk('clearInventoryState가 존재한다', /function clearInventoryState\(\)/.test(src));
chk('clearInventoryState는 active 표시를 지운다',
    /function clearInventoryState\(\)[\s\S]{0,250}remove\('active'\)/.test(src));

// 계약: 전용 핸들러가 있는 필드는 일반 바인딩에서 **제외**돼야 한다.
//   포함되면 전용 핸들러가 덮어써져 어느 쪽이 이기는지 알 수 없다.
//   (ENG판이 이 함정을 §12.7 결함 A로 밟았고, 이 판은 CONFIG_FIELDS 없이 배열 바인딩을 쓴다.)
const bindLine = src.match(/\[([^\]]*)\]\.forEach\(el => el\.onchange = saveConfig\)/);
chk('일반 바인딩 배열에서 els.slot을 제외한다',
    bindLine !== null && !bindLine[1].includes('els.slot'));
chk('일반 바인딩에 나머지 컨트롤은 그대로 있다',
    bindLine !== null && ['els.anim', 'els.disp', 'els.hour', 'els.chime', 'els.flip', 'els.invert']
        .every(s => bindLine[1].includes(s)));

// 배지 상태 추적의 전제 — 문자집합에 중복이 있으면 `b_<char>` 조회가 엉뚱한 배지를 잡는다.
chk('UNIQ_CHARS에 중복이 없다 (배지 id가 1:1이다)',
    UNIQ_CHARS.length > 0 && new Set(UNIQ_CHARS).size === UNIQ_CHARS.length);
chk('업로드 루프가 `b_<char>` id 규약을 쓴다',
    /getElementById\('b_'\+char\)\.classList\.add\('active'\)/.test(src));

// ===================================================================
// A-2 — 글리프는 /f1에, 이름표는 /f0에 남는다 (중국어판 §12.13/§12.14)
// ===================================================================
console.log('A-2: 폰트 이름이 업로드한 슬롯에 기록된다');

const processAll = grabFrom(src, 'async function processAll()');
chk('processAll을 찾는다', processAll !== null);
if (processAll) {
    // 슬롯을 **한 번만** 읽어 고정해야 한다. 40회 업로드 중 바뀔 수 있다.
    chk('업로드 대상 슬롯을 시작 시점에 한 번만 읽는다',
        /const targetSlot = els\.slot\.value/.test(processAll));
    chk('모든 업로드가 고정한 슬롯을 쓴다',
        /upload\?slot=\$\{targetSlot\}&font=/.test(processAll)
        && !/upload\?slot=\$\{els\.slot/.test(processAll));

    // 이름은 **업로드와 같은 요청**에 실려야 한다. 별도 POST로만 보내면 그 요청이
    //   빠지거나 늦을 때 슬롯에 글리프만 남고 이름표가 없어 "Empty Slot"이 된다.
    chk('폰트 이름이 업로드와 **같은 요청**에 실린다',
        /upload\?slot=\$\{targetSlot\}&font=\$\{encodeURIComponent\(fontName\)\}/.test(processAll));
    chk('이름도 슬롯처럼 시작 시점에 한 번만 읽어 고정한다',
        /const fontName = els\.fIn\.files\[0\]/.test(processAll));
    chk('폰트 미선택이면 업로드를 시작하지 않는다 (조용히 빈 슬롯을 만들지 않는다)',
        /if \(!fontName\)/.test(processAll) && /return;/.test(processAll));
    chk('font_name POST가 font_slot을 함께 보낸다',
        /JSON\.stringify\(\{[^}]*font_slot[^}]*font_name/.test(processAll));
    chk('업로드 시작 시 배지 상태를 비운다', /clearInventoryState\(\)/.test(processAll));
}

// 펌웨어 — 이름표를 **업로드한 슬롯**에 쓴다. setFontName()은 현재 슬롯에 쓰므로
//   업로드 경로가 그걸 부르면 이름표가 엉뚱한 폴더로 간다(이 결함의 원래 형태).
const uploadData = grabFrom(wm, 'void WebManager::handleUploadData');
chk('업로드 핸들러에서 ?font= 를 읽는다',
    uploadData !== null && /server\.arg\("font"\)/.test(uploadData));
chk('업로드가 연 슬롯을 기억한다',
    uploadData !== null && /uploadSlot = \(uint8_t\)slot/.test(uploadData));
chk('이름표를 그 슬롯에 쓴다 (setFontName이 아니라 setSlotName)',
    uploadData !== null && /display\.setSlotName\(uploadSlot, uploadFontName\)/.test(uploadData));
chk('업로드 경로가 setFontName()을 부르지 않는다',
    uploadData !== null && !/display\.setFontName\(/.test(uploadData));
chk('이름표 거부를 조용히 넘기지 않는다 (로그를 남긴다)',
    uploadData !== null && /Slot name rejected/.test(uploadData));
chk('업로드 시작 시 font 인자 도착 여부를 무조건 로그로 남긴다 (침묵 금지)',
    /\[WEB\] upload start slot=%d fontArg=%d/.test(wm));
chk('저장에 성공한 경우에만 이름표를 쓴다 (wroteFile 가드)',
    uploadData !== null && /wroteFile && uploadFontName\.length\(\) > 0/.test(uploadData));

const setSlotName = grabFrom(dm, 'bool DisplayManager::setSlotName');
chk('setSlotName이 슬롯 범위를 검사한다',
    setSlotName !== null && /slot >= FONT_SLOT_COUNT/.test(setSlotName));
chk('setSlotName이 이름표를 지정 슬롯 폴더에 쓴다',
    setSlotName !== null && /"\/f" \+ String\(slot\) \+ "\/name\.txt"/.test(setSlotName));
chk('setSlotName이 _slotNames를 갱신한다',
    setSlotName !== null && /_slotNames\[slot\] = name/.test(setSlotName));
chk('setSlotName은 현재 슬롯일 때만 config를 건드린다',
    setSlotName !== null && /font_slot == slot/.test(setSlotName));
chk('setSlotName이 이미 같은 값이면 파일을 다시 쓰지 않는다 (40회 호출)',
    setSlotName !== null && /_slotNames\[slot\] == name\) return true/.test(setSlotName));
chk('setFontName이 setSlotName에 위임한다',
    /void DisplayManager::setFontName[\s\S]{0,250}setSlotName\(configManager\.get\(\)\.font_slot/.test(dm));

// --- A-2④ — 32바이트 상한이 실제 폰트 이름을 거부했다 (중국어판 §12.14 실기) ---
// 상한의 근거("이름을 파일명으로도 쓴다")는 **사실이 아니었다** — 경로는 리터럴이고
//   이름은 그 파일의 **내용**으로만 들어간다. 라이선스 접미사가 붙으면 ASCII 이름도
//   금방 31바이트를 넘는다(`-Noncommercial-Regular` = 24바이트).
const nameMaxM = /#define\s+FONT_NAME_MAX_LEN\s+(\d+)/.exec(cfgH);
chk('이름 길이 상한이 config.h 한 곳(FONT_NAME_MAX_LEN)에 있다', nameMaxM !== null);
chk('이름 상한이 31바이트 시절로 되돌아가지 않았다',
    nameMaxM !== null && Number(nameMaxM[1]) >= 64);
chk('setSlotName이 상한 리터럴이 아니라 FONT_NAME_MAX_LEN을 쓴다',
    setSlotName !== null && /name\.length\(\) > FONT_NAME_MAX_LEN/.test(setSlotName)
    && !/name\.length\(\)\s*>=\s*32/.test(dm));
chk('이름표 거부 로그에 실제 길이와 상한이 함께 나온다',
    /Slot name rejected[\s\S]{0,250}len=%u[\s\S]{0,350}FONT_NAME_MAX_LEN/.test(wm));

// 펌웨어 쪽 순서 계약 — 슬롯을 먼저 적용해야 이름표도 그 슬롯에 쓰인다.
chk('handleSetConfig에서 슬롯을 이름보다 먼저 적용한다',
    handleSetConfig !== null
    && handleSetConfig.indexOf('"font_slot"') !== -1
    && handleSetConfig.indexOf('"font_slot"') < handleSetConfig.indexOf('FONT_JSON_KEY'));

// ===================================================================
// A-3 / A-4 — 슬롯 이름 캐시와 빈 슬롯 표기
// ===================================================================
console.log('A-3/A-4: 슬롯 이름 캐시 갱신과 빈 슬롯 표기 통일');

// setFontSlot이 _slotNames를 갱신하지 않으면 /api/config의 slot_names가 부팅 시점 값으로 멈춘다.
const setFontSlot = grabFrom(dm, 'DisplayManager::setFontSlot');
chk('setFontSlot이 _slotNames를 갱신한다',
    setFontSlot !== null && /_slotNames\[slot\]\s*=/.test(setFontSlot));
chk('빈 슬롯 표기가 두 곳에서 같다 (setFontSlot vs 부팅 캐시)',
    setFontSlot !== null && /name = "Empty Slot"/.test(setFontSlot)
    && /_slotNames\[i\] = "Empty Slot"/.test(dm));
chk('빈 슬롯 표기가 "Empty"로 갈라져 있지 않다 (구 표기 재발 방지)',
    !/_slotNames\[i\] = "Empty";/.test(dm));

console.log(`\n=== 결과: ${pass} passed, ${fail} failed ===`);
process.exit(fail === 0 ? 0 : 1);
