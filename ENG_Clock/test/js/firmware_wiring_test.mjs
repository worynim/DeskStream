// worynim@gmail.com
/**
 * @file firmware_wiring_test.mjs
 * @brief [수정할 사항 v2] 호스트에서 컴파일할 수 없는 펌웨어 파일의 회귀 검증
 *
 * @details
 * display_manager.cpp / ENG_Clock.ino / english_time.* 는 Arduino·U8g2에 붙어 있어
 * g++ 호스트 테스트 대상이 아니다. 그래서 순수 모듈(layout_engine, english_time_core,
 * renderer_geometry)만으로는 이 파일들의 수정이 검증되지 않는다.
 *
 * 이 테스트는 배포되는 소스에서 수정 지점을 **정적으로** 확인한다.
 * 숫자 모드 문자열처럼 계산이 독립적인 부분은 펌웨어 상수를 읽어 산식으로 다시 푼다.
 * (계산 자체를 복제하지 않고 상수만 가져와 식을 재현하므로, 상수가 바뀌면 함께 걸린다.)
 */
import { readFileSync } from 'fs';
import { linePitch } from './_extracted.mjs';
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
const ino = read('ENG_Clock.ino');
const eth = read('english_time.h');
const etcpp = read('english_time.cpp');
const web = read('web_pages.h');

/** config.h의 #define 정수 값을 읽는다 (계산을 테스트에서 재현하기 위함) */
const define = (name) => {
    const m = config.match(new RegExp(`^#define\\s+${name}\\s+(\\d+)`, 'm'));
    if (!m) throw new Error(`config.h 에서 ${name} 을 찾지 못했다`);
    return parseInt(m[1], 10);
};
const body = (src, re) => { const m = src.match(re); return m ? m[0] : ''; };

// ===================================================================
// 버그 3 — 2줄에서 애니메이션이 다른 줄 밴드를 넘지 않는다
// ===================================================================
console.log('버그 3: 펌웨어 애니메이션 줄 격리');

const SCREEN_HEIGHT = define('SCREEN_HEIGHT');
const LINE_HEIGHT = define('LINE_HEIGHT');
const SCREEN_WIDTH = define('SCREEN_WIDTH');
const GLYPH_H = define('GLYPH_H');

const pairFn = body(dm, /static void drawAnimPair\([\s\S]*?\n\}/);
const exitFn = body(dm, /static void drawAnimExit\([\s\S]*?\n\}/);
chk('drawAnimPair 정의 존재', pairFn.length > 0);
chk('drawAnimExit 정의 존재', exitFn.length > 0);

// 이동량: 16스텝을 채웠을 때 정확히 한 줄 높이여야 한다.
const STEP = 16;
const expected = LINE_HEIGHT;
const offAt = (step) => step * (LINE_HEIGHT / STEP);
chk(`16스텝 이동량 = 한 줄 높이 (${offAt(STEP)} = ${LINE_HEIGHT})`, offAt(STEP) === expected);
chk('이전 값(4px/스텝)은 화면 높이를 이동해 위줄 밴드를 훑는다',
    STEP * 4 === SCREEN_HEIGHT && SCREEN_HEIGHT > LINE_HEIGHT);

// 소스가 실제로 LINE_HEIGHT 기준으로 쓰는지
// [리뷰 §2.3] 매직넘버 16을 ANIM_STEPS로 이름 붙였다. 값은 여전히 16이어야 한다
//   (linePitch/lineStartX의 사다리 규칙과 무관하지만 웹 미리보기와 1:1 대응한다).
chk('ANIM_STEPS = 16으로 정의된다', /static const int ANIM_STEPS = 16;/.test(dm));
chk('drawAnimPair가 한 줄 높이를 스텝당 LINE_HEIGHT/ANIM_STEPS 으로 나눈다',
    /const int off = step \* \(LINE_HEIGHT \/ ANIM_STEPS\);/.test(pairFn)
    && /const int off = step \* \(LINE_HEIGHT \/ ANIM_STEPS\);/.test(exitFn));
chk('애니메이션 종료 판정도 ANIM_STEPS를 쓴다',
    /_animState\.currentStep > ANIM_STEPS/.test(dm));
chk('스크롤 퇴장 시점이 ANIM_STEPS를 쓴다 (하드코딩 16 남아있으면 안 됨)',
    !/step [<>]=? ?16/.test(pairFn) && !/step [<>]=? ?16/.test(exitFn));
// 이전 코드: baseY + SCREEN_HEIGHT - (step*4) → 아래줄 글자가 y<32(위줄 밴드)로 들어간다.
// [v5] SCREEN_HEIGHT는 밴드 정책(oneLine 판별·bandH)에서 쓸 수 있다 — **이동량**에서는 안 된다.
const movement = (fn) => fn.split('\n')
    .filter(l => !/oneLine|bandTop|bandH/.test(l)).join('\n');
chk('drawAnimPair가 화면 높이만큼 이동하지 않는다 (밴드 정책 줄 제외)',
    !/SCREEN_HEIGHT/.test(movement(pairFn)) && !/SCREEN_HEIGHT/.test(movement(exitFn)));
chk('스크롤업 진입점이 한 줄 높이를 쓴다', /baseY \+ LINE_HEIGHT - off/.test(pairFn));
chk('스크롤다운 진입점이 한 줄 높이를 쓴다', /baseY - LINE_HEIGHT \+ off/.test(pairFn));
chk('스크롤 드로잉이 자기 줄 밴드(baseY→band.top, band.h)를 넘긴다',
    pairFn.includes('baseY - off, band.top, band.h)')
    && pairFn.includes('baseY + LINE_HEIGHT - off, band.top, band.h)')
    && exitFn.includes('baseY - off, band.top, band.h)')
    && exitFn.includes('baseY + off, band.top, band.h)'));

// 줄 밴드 클립 — 슬라이드뿐 아니라 스케일/디더/줌도 같은 문제를 가진다.
// 다만 후자들은 설계상 이미 [baseY, baseY+GLYPH_H) 안에만 그리므로 스크롤만 잘라도 된다.
// (U8g2 의 setClipWindow/setDrawWindow 은 이 빌드에서 그리기에 반영되지 않는다 —
//  u8g2_IsIntersection()이 user_* 만 본다. 그래서 클립은 프로젝트 렌더러가 직접 한다.)
chk('U8g2 클립 API에 기대지 않는다 (이 빌드에서 무효)',
    !/setDrawWindow|setClipWindow/.test(dm));
const animFrame = body(dm, /void DisplayManager::renderAnimFrame\([\s\S]*?\n\}/);
// 주석까지 세지 않도록 실제 호출부만 센다.
const CALL = 'renderer.drawSingleCharClipped(';
const clipped = (dm.split(CALL).length - 1);
// 스크롤 업/다운 × (쌍=이전+새) 4곳 + (퇴장) 2곳 + (정적 2줄, v5 drawCenterText) 1곳 = 7곳
chk(`스크롤·2줄 정적 드로잉 7곳이 모두 줄 밴드로 잘라 그린다 (${clipped}곳)`, clipped === 7);
chk('스크롤 드로잉이 자기 줄 밴드를 넘긴다 (쌍 4곳, 퇴장 2곳 — 밴드 인자 전달)',
    (pairFn.split(CALL).length - 1) === 4 && (exitFn.split(CALL).length - 1) === 2);
chk('정적 글자는 클립 없이 제자리에 그린다 (이미 밴드 안)',
    /if \(isStatic\) renderer\.drawSingleChar\(/.test(animFrame));

// 클립 구현 — 밴드와 겹치는 행만 그려야 한다
const rc = read('renderer.cpp');
const clipFn = body(rc, /void Renderer::drawSingleCharClipped\([\s\S]*?\n\}/);
chk('Renderer::drawSingleCharClipped 정의 존재', clipFn.length > 0);
chk('글자 영역과 밴드의 교집합을 구한다',
    /if \(top < bandTop\) top = bandTop;/.test(clipFn)
    && /if \(bottom > bandTop \+ bandH\) bottom = bandTop \+ bandH;/.test(clipFn));
chk('교집합이 없으면 아무것도 그리지 않는다', /if \(bottom <= top\) return;/.test(clipFn));
chk('잘라낸 시작 행에 맞춰 비트맵 포인터를 옮긴다',
    /data \+ \(skip \* g\.bytesPerRow\)/.test(clipFn));
chk('drawBitmap 에 잘라낸 시작 y 와 행 수를 넘긴다',
    /drawBitmap\(x \+ g\.xOffset, top, g\.bytesPerRow, bottom - top,/.test(clipFn));

// 아래로 사라지는 글자(스크롤업의 이전 글자)는 위줄 밴드로 나가지 않아야 한다
const bottomBaseY = LINE_HEIGHT;   // 2줄에서 아래줄 baseY
chk('스크롤업 이전 글자는 자기 밴드 위로 나가지 않는다 (2줄 기준)',
    Array.from({ length: STEP + 1 }, (_, s) => bottomBaseY - offAt(s))
        .every(y => y + GLYPH_H <= bottomBaseY + LINE_HEIGHT));
chk('스크롤업 새 글자는 자기 밴드 아래로 나가지 않는다',
    Array.from({ length: STEP + 1 }, (_, s) => bottomBaseY + LINE_HEIGHT - offAt(s))
        .every(y => y + GLYPH_H > bottomBaseY));
chk('이전 구현(화면 높이 이동)은 위줄 밴드를 침범한다 — 회귀 기준',
    bottomBaseY - STEP * 4 < LINE_HEIGHT);

// ===================================================================
// [bug 3 수정] IP 도트는 마지막 숫자 옆("192.")에, 숫자 하단에 맞춰 찍힌다
// ===================================================================
console.log('\nIP 도트: 숫자 옆 인라인 배치');

const IP_DOT_SIZE = define('IP_DOT_SIZE');
const IP_DOT_GAP = define('IP_DOT_GAP');

const ipFn = body(dm, /void DisplayManager::showLargeIP\([\s\S]*?\n\}/);
chk('showLargeIP 정의 존재', ipFn.length > 0);

// 산식을 테스트에서 다시 푼다 (config.h 상수를 그대로 사용)
// 숫자 세그먼트 "192"(3자) + 도트 묶음을 가로 중앙에 놓는다.
// [v5] drawSingleChar의 y_offset은 **밴드 상단**이다(래스터를 밴드 중앙에 놓는다).
//      IP 화면은 1줄 밴드 상단(LINE_HEIGHT/2)을 넘기므로 래스터 상단 = digitY + (LINE_HEIGHT − GLYPH_H)/2,
//      어떤 기하든 래스터 중앙(=잉크 중앙)이 화면 세로 중앙에 온다.
const GLYPH_W_IP = define('GLYPH_W');
const digitY = Math.floor(LINE_HEIGHT / 2);
const digitTop = digitY + Math.floor((LINE_HEIGHT - GLYPH_H) / 2);
const digitBottom = digitTop + GLYPH_H;                                   // 래스터 하단
const dotY = Math.floor((SCREEN_HEIGHT + GLYPH_H) / 2) - IP_DOT_SIZE;    // 래스터 하단 정렬
const segmentLen = 3;                                   // "192" / "168" 등 최빈 형태
const blockW = segmentLen * GLYPH_W_IP + IP_DOT_GAP + IP_DOT_SIZE;
const startX = Math.floor((SCREEN_WIDTH - blockW) / 2);
const dotX = startX + segmentLen * GLYPH_W_IP + IP_DOT_GAP;
chk(`도트 하단이 래스터 하단과 같다 (숫자 끝=${digitBottom}, 도트 끝=${dotY + IP_DOT_SIZE})`,
    dotY + IP_DOT_SIZE === digitBottom);
chk(`도트가 세로로 화면 안에 있다 (끝=${dotY + IP_DOT_SIZE} ≤ ${SCREEN_HEIGHT})`,
    dotY >= 0 && dotY + IP_DOT_SIZE <= SCREEN_HEIGHT);
chk(`도트가 가로로 화면 안에 있다 (끝=${dotX + IP_DOT_SIZE} ≤ ${SCREEN_WIDTH})`,
    dotX + IP_DOT_SIZE <= SCREEN_WIDTH);
chk(`숫자 래스터가 화면 세로 중앙이다 (중앙 y=${(digitTop + digitBottom) / 2})`,
    (digitTop + digitBottom) / 2 === SCREEN_HEIGHT / 2);
chk(`신형 384B(래스터 64px)에서도 도트가 화면 안에 있다`,
    (() => { const b = Math.floor((SCREEN_HEIGHT + 64) / 2);
             return b === SCREEN_HEIGHT && b - IP_DOT_SIZE >= 0; })());
chk(`도트가 숫자 영역 아래에서 시작한다 (도트 위=${dotY} ≥ 숫자 위=${digitTop})`, dotY >= digitTop);
// 묶음(숫자+간격+도트)이 가로 중앙이다: 왼쪽 여백과 오른쪽 여백이 같다.
chk(`묶음의 좌우 여백이 같다 (좌 ${startX} / 우 ${SCREEN_WIDTH - (dotX + IP_DOT_SIZE)})`,
    startX === SCREEN_WIDTH - (dotX + IP_DOT_SIZE));
chk('도트가 숫자 오른쪽에 있다 (간격 이후)',
    dotX === startX + segmentLen * GLYPH_W_IP + IP_DOT_GAP && dotX > startX);
chk(`숫자 래스터(=잉크)는 세로 중앙에 있다 (y=${digitY})`,
    digitY === Math.floor(LINE_HEIGHT / 2) && digitY >= 0);

// 소스가 위 산식을 그대로 쓰는지 — 이전 "숫자 아래 도트" 산식이 남으면 잡는다.
chk('숫자를 1줄 밴드 상단 y 에 넘긴다 (v5: y_offset = 밴드 상단)',
    /digitY\s*=\s*LINE_HEIGHT \/ 2/.test(ipFn));
chk('도트를 래스터 하단에 맞춘다 (구 산식 digitY + glyphH − SIZE는 구 형식 한정)',
    /dotY\s*=\s*\(SCREEN_HEIGHT \+ g\.glyphH\) \/ 2 - IP_DOT_SIZE/.test(ipFn) && !/blockH/.test(ipFn));
chk('도트 앵커가 실제 숫자 기하를 따른다 (geometryOf 조회, 폴백 = defaultGeometry)',
    /renderer\.geometryOf\(/.test(ipFn) && /dg \? \*dg : defaultGeometry\(\)/.test(ipFn));
chk('drawSingleChar 에 세로 중앙 y 를 넘긴다 (하드코딩 0 이 아님)',
    /drawSingleChar\(i, segment\.substring\(j, j \+ 1\), startX \+ \(j \* pitch\), digitY\)/.test(ipFn));
chk('도트를 묶음 좌표에 둔다 (하드코딩 120, 56 이 아님)',
    /drawBox\(dotX, dotY, IP_DOT_SIZE, IP_DOT_SIZE\)/.test(ipFn)
    && !/drawBox\(120, 56/.test(ipFn));
chk('가로 중앙은 도트 포함 묶음 기준이다',
    /\(SCREEN_WIDTH - blockW\) \/ 2/.test(ipFn) && /hasDot \? IP_DOT_GAP \+ IP_DOT_SIZE : 0/.test(ipFn));

// 웹 미리보기와 기하가 어긋나지 않아야 한다
// [v5] 웹의 GLYPH_H는 **래스터 높이**(64)다 — 펌웨어 config.h의 GLYPH_H(32)는 구 형식
// 글리프 높이이고, 웹의 LINE_HEIGHT가 그 짝이다.
const jsG = web.match(/const GLYPH_H = (\d+)/);
const jsLine = web.match(/const LINE_HEIGHT = (\d+)/);
const jsScreenH = web.match(/const SCREEN_H = (\d+)/);
chk('웹 래스터 높이 GLYPH_H = SCREEN_HEIGHT (48×64 전체 화면)',
    jsG && parseInt(jsG[1], 10) === SCREEN_HEIGHT);
chk('웹 래스터가 줄 밴드보다 높다 (밴드 중앙 정렬이 의미를 가진다)',
    jsG && parseInt(jsG[1], 10) > LINE_HEIGHT);
chk('펌웨어와 웹의 LINE_HEIGHT 가 같다', jsLine && parseInt(jsLine[1], 10) === LINE_HEIGHT);
chk('펌웨어와 웹의 SCREEN_H 가 같다', jsScreenH && parseInt(jsScreenH[1], 10) === SCREEN_HEIGHT);

// ===================================================================
// 수정할 사항 2 — 숫자 모드에 H / M / S (수정할 사항 1: 숫자와 단위 사이 공백 한 칸)
// ===================================================================
console.log('\n수정 2: 숫자 모드 단위 표기 (공백 한 칸)');

const update = body(ino, /void handleClockUpdate\([\s\S]*?\n\}/);
chk('handleClockUpdate 정의 존재', update.length > 0);
chk('시 에 " H" 가 붙는다 (공백 포함)', /getNumericHour\(h, is24H\) \+ " H"/.test(update));
chk('분 에 " M" 이 붙는다 (공백 포함)', /getNumericMinute\(m\)\s*\+ " M"/.test(update));
chk('초 에 " S" 가 붙는다 (공백 포함)', /getNumericSecond\(s\)\s*\+ " S"/.test(update));
// 단위와 공백이 붙어도 배치되는 글자는 3자(42px) + 빈 공백 셀 한 칸이다.
const MAX_CHARS_PER_LINE = define('MAX_CHARS_PER_LINE');
const GLYPH_W = define('GLYPH_W');
chk(`"13 H" 가 한 줄에 들어간다 (글자 3자 × ${GLYPH_W}px + 공백 셀 ≤ ${MAX_CHARS_PER_LINE}자 한도)`,
    4 <= MAX_CHARS_PER_LINE && 3 * GLYPH_W <= SCREEN_WIDTH);

// 공백이 어절을 2개로 만들므로, H/M/S 화면은 singleLine 플래그로 2줄 분할을 건너뛴다.
// (플래그가 없으면 "02"와 "H"가 위아래로 찢어진다)
const updateAllFn = body(dm, /void DisplayManager::updateAll\([\s\S]*?\n\}/);
chk('updateAll 정의 존재', updateAllFn.length > 0);
// [리뷰 §1.1/§2.2/§2.4] 판정식이 7곳에 복제돼 refreshNow()가 정적 경로와 **반대**인
//   값을 넘기는 버그가 났다. 이제 isSingleLineLayout() 한 곳에 있고, 모든 경로가 이걸 쓴다.
//   아래 검사는 "규칙이 정의돼 있고" + "모든 경로가 그 함수를 부른다"를 확인한다.
const titleRuleFn = body(dm, /static bool isTitleScreenOf\([\s\S]*?\n\}/);
const singleLineRuleFn = body(dm, /static bool isSingleLineLayout\([\s\S]*?\n\}/);
chk('isTitleScreenOf이 제목 화면 규칙을 정의한다 (is_flipped → 3번/0번)',
    /configManager\.get\(\)\.is_flipped \? \(idx == 3\) : \(idx == 0\)/.test(titleRuleFn));
chk('isSingleLineLayout이 숫자 모드(isWord)를 판단한다',
    /configManager\.get\(\)\.display_mode == CLOCK_MODE_WORD\) && renderer\.isCacheLoaded\(\)/.test(singleLineRuleFn)
    && /return !isWord && !isTitleScreenOf\(idx\);/.test(singleLineRuleFn));
// 회귀의 핵심: refreshNow()가 정적 경로와 **같은 규칙**을 써야 한다.
const refreshNowFn = body(dm, /void DisplayManager::refreshNow\([\s\S]*?\n\}/);
chk('refreshNow도 isSingleLineLayout을 쓴다 (버그 회귀 — 정반대 값 금지)',
    /drawCenterText\(i, lastTexts\[i\], isSingleLineLayout\(i\)\)/.test(refreshNowFn)
    && !/isTitleScreen\)/.test(refreshNowFn));
chk('제목 판정을 인라인으로 복제한 곳이 남아 있지 않다',
    !/is_flipped\) \(i == 3\)/.test(dm) && !/is_flipped\) \(idx == 3\)/.test(dm));
chk('정적 경로(drawCenterText)에 singleLine 을 넘긴다',
    /drawCenterText\(i, texts\[i\], isSingleLineLayout\(i\)\)/.test(updateAllFn));
chk('잔상 정리 경로도 같은 규칙을 쓴다',
    /drawCenterText\(i, lastTexts\[i\], isSingleLineLayout\(i\)\)/.test(updateAllFn));
chk('애니메이션 경로(getCharData)에 singleLine 을 넘긴다',
    /const bool singleLine = isSingleLineLayout\(i\);/.test(updateAllFn)
    && /getCharData\(texts\[i\], _animState\.screens\[i\]\.newChars,\s*\n\s*_animState\.screens\[i\]\.newCount, singleLine\)/.test(updateAllFn));
const leSrc = read('layout_engine.cpp');
chk('layoutWrap이 singleLine 이면 2줄 강제 규칙을 건너뛴다',
    /seenWord && !singleLine && lines < LAYOUT_MAX_LINES/.test(leSrc));
chk('웹 미리보기도 같은 규칙으로 singleLine 을 넘긴다',
    /layoutWrap\(targetTimeStrings\[s\], !isWordPreview && s !== 0\)/.test(web)
    && /layoutWrap\(lastTimeStrings\[s\], !isWordPreview && s !== 0\)/.test(web));
chk('웹 JS layoutWrap도 singleLine 파라미터를 받는다 (펌웨어 미러)',
    /function layoutWrap\(text, singleLine = false, inkWidth = previewInkWidth\)/.test(web));

// ===================================================================
// [간격 확장 §6.16] 글자 수에 따른 피치 — 웹 ↔ 펌웨어 배선
// ===================================================================
console.log('\n간격 확장 (§6.16): 잉크 폭 → 피치');

chk('펌웨어가 inkWidthOf()로 잉크 폭을 잰다',
    /inkWidthOf\(getCharDataPtr\(&cc\), cc\.geom->bytesPerRow, cc\.geom->glyphH\)/.test(rc)
    && /measureMaxInkWidth\(\);/.test(rc));
// [§6.16b] 펌웨어는 줄별 잉크를 조회하는 trampoline까지 함께 넘겨야 한다.
//   이것이 없으면 펌웨어는 폰트 최대로만 상한을 걸어 **웹보다 피치가 넓어진다.**
chk('getCharData가 잉크 폭과 줄별 조회 함수를 layoutWrap에 넘긴다',
    /laidCount,\s*\n\s*singleLine, maxInkWidth,\s*\n\s*&Renderer::inkOfTrampoline, this\)/.test(rc));
chk('clearCache가 잉크 폭을 0으로 되돌린다 (캐시 없음 = 확장 없음)',
    /maxInkWidth = 0;/.test(rc));
// §6.16b — 글자별 표는 로드 시점에만 유효하므로, 조기 반환 전에 함께 비운다.
chk('글자별 잉크 표(inkByChar)를 로드·초기화 경로에서 함께 비운다',
    /inkByChar\.clear\(\);/.test(rc));
chk('layoutWrap이 잉크 폭과 조회 함수를 인자로 받는다',
    /int& outCount, bool singleLine = false, int inkWidth = 0,\s*\n\s*InkWidthFn inkFn = nullptr, void\* inkCtx = nullptr\)/.test(read('layout_engine.h')));
chk('웹 미리보기도 잉크 폭을 잰다 (알파 > 128 = packPixels와 같은 임계값)',
    /px\[\(y \* RASTER_W \+ x\) \* 4 \+ 3\] > 128/.test(web)
    && /previewInkWidth = maxInkWidthOf\(bitmapCache\);/.test(web));
// §6.16b — 웹도 글자별 표를 채운다. 이것이 없으면 웹이 폰트 최대로 묶여 좁게 나온다.
chk('웹도 글자별 잉크 표를 채운다 (펌웨어 inkByChar의 미러)',
    /previewInkByChar = \{\};/.test(web)
    && /for \(const ch of UNIQ_CHARS\) previewInkByChar\[ch\] = glyphInkWidth/.test(web));
// [사용자 지정 사다리] 총폭을 (n+1) 글자분으로 — 양쪽 식이 어긋나면 조용히 간격이 갈린다
chk('양쪽 모두 총폭을 (n+1)글자분으로 맞춘다 (사용자 지정 사다리)',
    /const int total = \(charCount \+ 1 > maxLine\) \? maxLine \* cellW : \(charCount \+ 1\) \* cellW;/.test(leSrc)
    && /const total = \(charCount \+ PITCH_SUM_OFFSET > MAX_PER_LINE\)/.test(web)
    && /\? MAX_PER_LINE \* GLYPH_W : \(charCount \+ PITCH_SUM_OFFSET\) \* GLYPH_W;/.test(web));
chk('사다리 오프셋 상수가 양쪽에 같다',
    /#define PITCH_SUM_OFFSET 1/.test(read('layout_engine.h'))
    && /const PITCH_SUM_OFFSET = 1;/.test(web));
// [핵심 회귀] 잉크 하단은 **목표가 아니라 하한**이다. 한때 완화를 이 값으로
// 되돌려 간격을 겹치기 직전에 딱 붙였고, 그게 "AM/ONE/FORTY가 너무 붙어"의 원인이었다.
chk('잉크 하단은 올리기만 한다 (목표로 좁히지 않는다)',
    /if \(floor > spread\) spread = floor;/.test(leSrc)
    && /if \(floor > spread\) spread = floor;/.test(web));
chk('양쪽 모두 겹침 없는 하단(measureLineFloor)을 잰다',
    /int measureLineFloor\(/.test(read('layout_engine.h'))
    && /const int floor = \(lineFloor > cellW\) \? lineFloor/.test(leSrc)
    && /function measureLineFloor\(lineText, fallback\)/.test(web)
    && /const floor = \(lineFloor > GLYPH_W\) \? lineFloor/.test(web));
// [웹-기기 일치] 잉크는 **업로드할 바이트**에서 져야 한다. 캔버스를 다시 읽으면
//   premultiplied alpha / GPU 반올림으로 1~2px 어긋나 기기만 간격이 벌어진다.
chk('웹 잉크를 캔버스 아닌 업로드 바이트에서 잰다 (웹-기기 간격 일치)',
    /const bm = packGlyph\(canvas\);/.test(web)
    && /inkWidthOf\(getCharDataPtr\(&cc\)/.test(rc));
// 화면 적합 상한(잘림 방지)이 양쪽에 같아야 한다
// [리뷰 §6.1] C++가 floorDiv()로, JS가 Math.floor로 나눗셈을 감싼다 —
//   음수 나눗셈은 두 언어가 다른 값을 내기 때문이다(JS는 내림, C++는 0쪽 절삭).
chk('화면 적합 상한이 양쪽에 같다 (양쪽 모두 floor 나눗셈)',
    /const int fit = floorDiv\(screenWidth - inkCap, charCount - 1\);/.test(leSrc)
    && /const fit = Math\.floor\(\(SCREEN_W - inkCap\) \/ \(charCount - 1\)\);/.test(web));
// lineStartX의 두 나눗셈도 같은 이유로 감싸야 한다 (펌웨어만 floor를 쓰고 JS는 안 쓰는 역전이 없어야 한다).
chk('lineStartX의 두 나눗셈 모두 floor로 감싸져 있다 (JS와 의미 일치)',
    /floorDiv\(screenWidth - inkSpan, 2\) \+ floorDiv\(lineInk - cellW, 2\)/.test(leSrc)
    && /Math\.floor\(\(SCREEN_W - inkSpan\) \/ 2\) \+ Math\.floor\(\(lineInk - GLYPH_W\) \/ 2\)/.test(web));
// 9자는 총폭 126 = 9 × glyphW 라서 **어떤 잉크에서도** 14여야 한다 (사용자 요구사항).
//   문자열 일치 대신 산식을 직접 돌려 확인한다 — 식이 바뀌면 이게 먼저 깨진다.
chk('펌웨어 linePitch: 9자 → 14 (= glyphW, 잉크와 무관하게)',
    (() => {
        for (const ink of [0, 11, 14, 24, 43]) {
            if (linePitch(9, ink, ink, ink) !== 14) return false;
        }
        return true;
    })());
chk('펌웨어 lineStartX: 확장이 없으면 기존 셀 중앙 정렬 경로로 분기',
    /if \(pitch <= cellW\) return \(screenWidth - charCount \* pitch\) \/ 2;/.test(leSrc)
    && /if \(pitch <= GLYPH_W\) return Math\.floor\(\(SCREEN_W - charCount \* pitch\) \/ 2\);/.test(web));
chk('음수 나눗셈을 JS Math.floor에 맞춘다 (교차 검증이 정수를 비교한다)',
    /static int floorDiv\(int a, int b\)/.test(leSrc)
    && /Math\.floor\(\(SCREEN_W - inkSpan\) \/ 2\)/.test(web));

// ===================================================================
// 수정할 사항 3 — 숫자 모드 첫 화면의 요일은 영문
// ===================================================================
console.log('\n수정 3: 요일 영문 표기');

chk('getDay(영문)를 모드와 무관하게 쓴다', /String dayStr = EnglishTimeConverter::getDay\(d\);/.test(update));
// 주석이 아니라 실제 호출이 남아 있는지를 본다.
chk('getNumericDay 호출이 남아 있지 않다', !/EnglishTimeConverter::getNumericDay\(/.test(ino));
// 제거된 API 가 선언·정의·미러 어디에도 남지 않았는지 (미러가 어긋나면 미리보기만 틀린다)
chk('영문 시간 변환기에서 getNumericDay 선언이 제거됨', !/static String getNumericDay/.test(eth));
chk('영문 시간 변환기에서 getNumericDay 정의가 제거됨', !/EnglishTimeConverter::getNumericDay/.test(etcpp));
chk('웹 페이지에서 numericDay 가 제거됨', !/function numericDay/.test(web) && !/: numericDay\(/.test(web));
chk('테스트 대조기가 numericDay 를 참조하지 않는다',
    !/numericDay/.test(read('test/js/time_crosscheck.mjs'))
    && !/numericDay/.test(read('test/js/extract_from_web_page.mjs')));
// 24시간제 첫 화면이 "날짜 + 공백 + 요일" 2줄로 유지되는지
chk('첫 화면은 "날짜 요일" 로 space-join 된다', /date \+ " " \+ dayStr/.test(update));
// 2줄 × 9자 = 18. "10/2"(4) + "WEDNESDAY"(9) = 공백 포함 13자 → 4 + 9 로 나뉘어 들어간다.
const MAX_LAYOUT_CHARS = define('MAX_LAYOUT_CHARS');
const sample = '10/2 WEDNESDAY';
chk(`"${sample}" 가 레이아웃 한도에 들어간다 (${sample.length}자 / 한도 ${MAX_LAYOUT_CHARS})`,
    sample.length <= MAX_LAYOUT_CHARS);
chk('각 줄이 MAX_CHARS_PER_LINE 안에 들어간다',
    sample.split(' ').every(w => w.length <= MAX_CHARS_PER_LINE));

// ===================================================================
// bug 2 — 펌웨어가 설정한 POSIX TZ를 실제로 적용한다 (원인 2 회귀 방지)
// ===================================================================
console.log('\nbug 2: applyTimezone이 TZ를 유지한다');

const applyTzFn = body(dm, /void DisplayManager::applyTimezone\(\) \{[\s\S]*?\n\}/);
chk('applyTimezone 정의 존재', applyTzFn.length > 0);
// 코드만 대상으로 한다 — 함수 안의 설명 주석에도 원인 설명으로 configTime이 언급된다.
const applyTzCode = applyTzFn.split('\n')
    .filter(l => !l.trim().startsWith('//')).join('\n');
// configTime(오프셋, ...)은 코어(esp32-hal-time.c setTimeZone)가 내부에서
// setenv("TZ", "UTC0DST0")로 TZ를 **덮어써서** 어떤 타임존을 골라도 UTC가 된다.
chk('applyTimezone이 configTime(오프셋)을 쓰지 않는다 (TZ 덮어쓰기 원인)',
    !/configTime\s*\(/.test(applyTzCode));
chk('applyTimezone이 configTzTime(tz, ...)을 쓴다',
    /configTzTime\(\s*tz\s*,\s*NTP_SERVER1/.test(applyTzCode));
// 설정된 TZ 문자열을 그대로 넘기는지 (새 변수로 속이지 않는다)
chk('configTzTime에 configManager의 timezone을 넘긴다',
    /const char\* tz = configManager\.get\(\)\.timezone;/.test(applyTzFn));

// ===================================================================
// [여백 수정 v4/v5 — PLAN §6.13/§6.14] 확장 래스터 GEOM_192B·GEOM_384B (웹 ↔ 펌웨어 기하 대조)
// ===================================================================
console.log('\n여백 v4/v5: GEOM_192B·GEOM_384B (웹과 펌웨어 기하 일치)');

const rg = read('renderer_geometry.cpp');
const parseGeom = (name) => {
    const m = rg.match(new RegExp(`${name}\\s*=\\s*\\{\\s*(\\d+),\\s*(\\d+),\\s*(\\d+),\\s*(\\d+),\\s*(-?\\d+),\\s*(\\d+)\\s*\\}`));
    return m ? m.slice(1).map(Number) : null;
};
const webGlyphW = parseInt(web.match(/const GLYPH_W = (\d+)/)[1], 10);
const webRasterW = parseInt(web.match(/const RASTER_W = (\d+)/)[1], 10);
const webBpr = parseInt(web.match(/const BYTES_PER_ROW = (\d+)/)[1], 10);
const webMpl = parseInt(web.match(/const MAX_PER_LINE = (\d+)/)[1], 10);
const webRasterH = parseInt(web.match(/const GLYPH_H = (\d+)/)[1], 10);

const g192 = parseGeom('GEOM_192B');
chk('GEOM_192B 초기화자 존재', !!g192);
if (g192) {
    const [gw, gh, bpr, dw, xo, mpl] = g192;
    chk('피치 glyphW = 웹 GLYPH_W (14, 레이아웃 불변)', gw === webGlyphW && gw === 14);
    chk('래스터 폭 drawW = 웹 RASTER_W (48)', dw === webRasterW && dw === 48);
    chk('bytesPerRow = 웹 BYTES_PER_ROW (6, 48px → 192B/글자)',
        bpr === webBpr && bpr === 6 && dw === bpr * 8);
    chk('xOffset = −X_OFFSET (잉크 중앙 = 피치 중앙)',
        xo === -((dw - gw) / 2));
    chk('maxPerLine = 웹 MAX_PER_LINE (9, 레이아웃 호환)', mpl === webMpl && mpl === 9);
    chk(`파일 크기 ${bpr * gh}B = 192 (web packGlyph 출력과 같다)`, bpr * gh === 192);
    chk('geometryForSize가 192를 매핑한다', /if \(size == 192\)/.test(rg));
}

// [v5 — PLAN §6.14] 48×64 래스터. 잉크 합집합이 32px를 넘는 큰 폰트에서 위가 잘리지 않는다.
const g384 = parseGeom('GEOM_384B');
chk('GEOM_384B 초기화자 존재', !!g384);
if (g384) {
    const [gw, gh, bpr, dw, xo, mpl] = g384;
    chk('384B: 피치 glyphW = 웹 GLYPH_W (14, 레이아웃 불변)', gw === webGlyphW && gw === 14);
    chk('384B: 래스터 폭 drawW = 웹 RASTER_W (48)', dw === webRasterW && dw === 48);
    chk('384B: bytesPerRow = 웹 BYTES_PER_ROW (6)', bpr === webBpr && bpr === 6 && dw === bpr * 8);
    chk('384B: 래스터 높이 glyphH = 웹 GLYPH_H (64 = SCREEN_HEIGHT)',
        gh === webRasterH && gh === SCREEN_HEIGHT && gh > LINE_HEIGHT);
    chk('384B: xOffset = −X_OFFSET (잉크 중앙 = 피치 중앙)', xo === -((dw - gw) / 2));
    chk('384B: maxPerLine = 웹 MAX_PER_LINE (9, 레이아웃 호환)', mpl === webMpl && mpl === 9);
    chk(`384B: 파일 크기 ${bpr * gh}B = 384 (web packGlyph 출력과 같다)`, bpr * gh === 384);
    chk('geometryForSize가 384를 매핑한다', /if \(size == 384\)/.test(rg));
}
chk('기존 64B 엔트리 유지 (구 슬롯 호환)', /GEOM_64B\s*=\s*\{\s*14,\s*32,\s*2,/.test(rg));
chk('192B와 384B는 높이가 달라 크기(파일 크기)로 배타 판별된다',
    !!g192 && !!g384 && g192[1] !== g384[1]);
// 업로드 크기 한도가 새 형식을 수용하는지
chk('MAX_BITMAP_SIZE ≥ 384 (신형 업로드가 잘리지 않는다)', define('MAX_BITMAP_SIZE') >= 384);

// 웹 미리보기 상수도 펌웨어 기하와 짝인지 (반대 방향 대조)
chk('웹 X_OFFSET = (RASTER_W − GLYPH_W)/2',
    /const X_OFFSET = \(RASTER_W - GLYPH_W\) \/ 2;/.test(web));

// 넓은 래스터로 bx가 음수가 될 수 있으므로 Bayer 위상 나머지를 보정해야 한다
const ditherFn = body(rc, /void Renderer::drawDitheredChar\([\s\S]*?\n\}/);
chk('drawDitheredChar 정의 존재', ditherFn.length > 0);
chk('Bayer 위상이 음수 나머지를 보정한다 ((bx%4)+4)%4',
    /\(\(bx % 4\) \+ 4\) % 4/.test(ditherFn));
chk('보정 전의 음수 가능 인덱스가 남아 있지 않다',
    !/bayer_matrix\[\(r \+ y_offset\) % 4\]\[\(bx \+ px\) % 4\]/.test(ditherFn));
// 래스터가 넓어도 drawBitmap은 g.xOffset·bytesPerRow를 그대로 쓴다 (기하 테이블 일원화)
chk('drawSingleChar가 xOffset·bytesPerRow를 기하에서 가져온다',
    /u8g2->drawBitmap\(x \+ g\.xOffset, rasterTopY\(y_offset, g\), g\.bytesPerRow, g\.glyphH, data\)/.test(rc));

// ===================================================================
// [여백 수정 v5 — PLAN §6.14] 밴드 중앙 배치(rasterTopY) + 1줄/2줄 밴드 정책
// ===================================================================
console.log('\n여백 v5: 밴드 중앙 배치와 1줄/2줄 밴드 정책');

chk('rasterTopY가 래스터를 밴드 중앙에 놓는다',
    /return y_offset \+ \(LINE_HEIGHT - \(int\)g\.glyphH\) \/ 2;/.test(rc));
chk('drawSingleChar가 rasterTopY 로 그린다', /rasterTopY\(y_offset, g\)/.test(rc));
// 산식 재현: glyphH=32 → 그리기 y = y_offset (구 형식 불변), glyphH=64 → y_offset − 16
const yTopAt = (yo, gh) => yo + (LINE_HEIGHT - gh) / 2;
chk('구 형식(32px)은 그리기 y가 y_offset 그대로다 (기존 슬롯 렌더링 불변)',
    yTopAt(0, 32) === 0 && yTopAt(16, 32) === 16 && yTopAt(32, 32) === 32);
chk('신형(64px)은 1줄 밴드(16)에서 화면 전체(0..64)를 쓴다',
    yTopAt((SCREEN_HEIGHT - LINE_HEIGHT) / 2, 64) === 0
    && yTopAt((SCREEN_HEIGHT - LINE_HEIGHT) / 2, 64) + 64 === SCREEN_HEIGHT);

// 디더: y 위상 보정 + 밴드 행 클램프
chk('Bayer 위상 y도 음수 나머지를 보정한다 (((r+yTop)%4)+4)%4',
    /\(\(\(r \+ yTop\) % 4\) \+ 4\) % 4/.test(ditherFn));
chk('디더가 밴드와 겹치는 행만 그린다 (2줄 누수 방지)',
    /rFirst < bandTop - yTop/.test(ditherFn) && /rLast > bandTop \+ bandH - yTop/.test(ditherFn));
chk('디더 행 범위가 래스터 안으로 한 번 더 잘린다',
    /rLast > \(int\)g\.glyphH\) rLast = g\.glyphH/.test(ditherFn));

// 줌: 확대 래스터 중앙 = 밴드 중앙, 행 클램프
const zoomFn = body(rc, /void Renderer::drawZoomedChar\([\s\S]*?\n\}/);
chk('줌이 확대 래스터를 밴드 중앙에 놓는다',
    /start_y = y_offset \+ \(LINE_HEIGHT - target_h\) \/ 2;/.test(zoomFn));
chk('줌도 밴드와 겹치는 행만 그린다',
    /rFirst < bandTop - start_y/.test(zoomFn) && /rLast > bandTop \+ bandH - start_y/.test(zoomFn));

// 플립(drawScaledChar): 래스터 중앙의 LINE_HEIGHT 창을 h 높이로 눌러 밴드 중앙에 놓는다
const scaledFn = body(rc, /void Renderer::drawScaledChar\([\s\S]*?\n\}/);
chk('플립이 래스터 중앙의 LINE_HEIGHT 창을 샘플링한다 (64 → winTop 16)',
    /const int winTop = \(\(int\)g\.glyphH - LINE_HEIGHT\) \/ 2;/.test(scaledFn));
chk('플립이 창 안에서 h 높이로 눌러 그린다',
    /int src_y = winTop \+ \(i \* LINE_HEIGHT\) \/ h;/.test(scaledFn));
chk('플립이 결과를 밴드 중앙에 놓는다',
    /start_y = y_offset \+ \(LINE_HEIGHT - h\) \/ 2;/.test(scaledFn));
chk('구 형식(32px)에서는 창이 래스터 전체다 (기존 동작 불변)',
    Math.floor((32 - LINE_HEIGHT) / 2) === 0);

// display_manager: 1줄 = 화면 전체 밴드 / 2줄 = 자기 줄 밴드
// [리뷰 §2.3] 판정식이 drawAnimPair/drawAnimExit 두 곳에 복제돼 있었다.
//   이제 bandFor() 한 곳이고 두 함수가 그것을 부른다.
const bandForFn = body(dm, /static Band bandFor\([\s\S]*?\n\}/);
chk('bandFor가 1줄 레이아웃을 판별한다 (쌍/퇴장 모두)',
    /const bool oneLine = \(baseY == \(SCREEN_HEIGHT - LINE_HEIGHT\) \/ 2\);/.test(bandForFn)
    && /const Band band = bandFor\(baseY\);/.test(pairFn)
    && /const Band band = bandFor\(baseY\);/.test(exitFn));
chk('1줄은 화면 전체 밴드, 2줄은 자기 줄 밴드다',
    /return \{ oneLine \? 0 : baseY, oneLine \? SCREEN_HEIGHT : LINE_HEIGHT \};/.test(bandForFn));
chk('밴드 판정을 인라인으로 복제한 곳이 남아 있지 않다',
    !/bandTop/.test(pairFn) && !/bandTop/.test(exitFn));
chk('디더·줌에도 밴드 인자가 전달된다 (쌍 8곳, 퇴장 4곳)',
    (pairFn.match(/, band\.top, band\.h\)/g) || []).length === 8
    && (exitFn.match(/, band\.top, band\.h\)/g) || []).length === 4);

// 정적 경로(drawCenterText): 2줄은 줄 밴드로 잘라 그리고, 1줄은 클립 없이 그린다
const centerFn = body(dm, /void DisplayManager::drawCenterText\([\s\S]*?\n\}/);
chk('drawCenterText 정의 존재', centerFn.length > 0);
chk('drawCenterText가 2줄 레이아웃을 판별한다', /chars\[i\]\.line == 1/.test(centerFn));
chk('2줄이면 각 줄 밴드(chars[i].y, LINE_HEIGHT)로 잘라 그린다',
    /renderer\.drawSingleCharClipped\(idx, chars\[i\]\.c, chars\[i\]\.x, chars\[i\]\.y,\s*\n\s*chars\[i\]\.y, LINE_HEIGHT\)/.test(centerFn));
chk('1줄은 클립 없이 그린다 (큰 폰트도 온전히 보인다)',
    /renderer\.drawSingleChar\(idx, chars\[i\]\.c, chars\[i\]\.x, chars\[i\]\.y\)/.test(centerFn));

// IP 화면 도트 앵커가 실제 숫자 기하를 따른다 — 접근자 선언·정의 존재
chk('geometryOf 접근자가 선언·정의로 존재한다',
    /const CellGeometry\* geometryOf\(const String& s\);/.test(read('renderer.h'))
    && /const CellGeometry\* Renderer::geometryOf/.test(rc));

console.log(`\n=== 결과: ${pass} passed, ${fail} failed ===`);
process.exit(fail === 0 ? 0 : 1);
