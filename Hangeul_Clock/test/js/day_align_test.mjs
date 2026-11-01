// worynim@gmail.com
/**
 * @file day_align_test.mjs
 * @brief [2026-11-01 사용자 보고] 24시간제 첫 화면(날짜)의 "일"만 우측 정렬되지 않던 문제의 회귀 검증
 *
 * @details
 * 24시간제 제목 화면은 날짜("24일")를 보여준다. 시·분·초 화면은 `layoutCharX()`가
 * 마지막 글자를 우측 고정 열(x=96)에 붙이는데, 제목 화면만 항상 가운데 정렬이어서
 * 날짜의 "일"만 시·분·초보다 왼쪽에 남아 정렬이 어긋나 보였다.
 * 이제 제목 화면의 **날짜만** 우측 정렬 갈래로 보낸다(12시간제의 "오전"/"오후"는 라벨이라
 * 가운데 정렬 유지). 판정은 순수 모듈 `layoutIsDayText()`가 하고, 웹 미리보기는
 * `isDayText()`/`isCenteredPreview()`/`getCharPositions()`가 같은 규칙을 미러한다.
 *
 * @note 이 파일은 **배포되는** web_pages.h의 JS를 extract_from_web_page.mjs로 뽑아 쓴다
 *       (미러 스크립트를 따로 두면 배포본과 어긋난 채 굳는다 — 기존 하네스 방침).
 */
import { readFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import { getCharPositions, isDayText, isCenteredPreview } from './_extracted.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, '..', '..');
const read = (f) => readFileSync(join(ROOT, f), 'utf8');

let pass = 0, fail = 0;
const chk = (name, ok) => {
    if (ok) pass++;
    else { fail++; console.error(`  FAIL  ${name}`); }
};

const LAYOUT_SCREEN_W = 128;   // renderer_layout.h
const LAYOUT_CELL_W   = 32;    // renderer_layout.h
const LAYOUT_RIGHT_COL_X = 96; // renderer_layout.h
const LAYOUT_MAX_CHARS = 4;    // renderer_layout.h

/** 펌웨어 renderer_layout.cpp의 layoutCharX() 공식 — 이 테스트의 기준선 */
function firmwareLayoutX(count, j, centered) {
    if (count <= 0) return 0;
    if (centered || count === 1) return (LAYOUT_SCREEN_W - count * LAYOUT_CELL_W) / 2 + j * LAYOUT_CELL_W;
    const startX = (LAYOUT_RIGHT_COL_X - (count - 1) * LAYOUT_CELL_W) / 2;
    return (j === count - 1) ? LAYOUT_RIGHT_COL_X : startX + j * LAYOUT_CELL_W;
}

/** hangeul_time.cpp의 convertToHangeul(num, "일") 미러 — 실제 날짜 문자열을 만든다 */
const TENS = ["", "십", "이십", "삼십", "사십", "오십"];
const ONES = ["", "일", "이", "삼", "사", "오", "육", "칠", "팔", "구"];
const hangulDay = (d) => TENS[Math.floor(d / 10)] + ONES[d % 10] + "일";
const numericDay = (d) => `${d}일`;

// ===================================================================
// 1. isDayText — 펌웨어 layoutIsDayText()와 같은 판정 표
//    (test/test_layout.cpp의 [신설 2026-11-01] 기대값과 **같은 표**를 쓴다)
// ===================================================================
{
    let allDays = true;
    for (let d = 1; d <= 31; d++) {
        if (!isDayText(hangulDay(d)) || !isDayText(numericDay(d))) {
            allDays = false;
            console.error(`  FAIL  ${d}일 판정 실패 (한글=${isDayText(hangulDay(d))}, 숫자=${isDayText(numericDay(d))})`);
        }
    }
    chk('1~31일 한글/숫자 날짜는 모두 "일"로 끝난다', allDays);

    chk('12시간제 제목 화면의 오전/오후는 날짜가 아니다',
        !isDayText("오전") && !isDayText("오후"));
    chk('시/분/초/정각은 날짜가 아니다',
        !isDayText("한시") && !isDayText("이십삼시")
        && !isDayText("오분") && !isDayText("이십구초") && !isDayText("정각"));
    chk('빈 문자열은 날짜가 아니다', !isDayText(""));
    chk('접미사만 본다: "일이"는 날짜가 아니다', !isDayText("일이"));
}

// ===================================================================
// 2. isCenteredPreview — 펌웨어 isCenteredText() 미러
// ===================================================================
{
    chk('제목 화면의 날짜는 우측 정렬 (가운데 아님)',
        isCenteredPreview(true, hangulDay(24)) === false
        && isCenteredPreview(true, numericDay(24)) === false);
    chk('12시간제 제목 화면(오전/오후)은 가운데 정렬 유지',
        isCenteredPreview(true, "오전") === true && isCenteredPreview(true, "오후") === true);
    chk('시·분·초는 우측 정렬 (가운데 아님)',
        isCenteredPreview(false, "이십삼시") === false
        && isCenteredPreview(false, "오분") === false
        && isCenteredPreview(false, "이십구초") === false);
    chk("'정각'은 화면과 무관하게 가운데 정렬",
        isCenteredPreview(false, "정각") === true && isCenteredPreview(true, "정각") === true);
}

// ===================================================================
// 3. getCharPositions — 우측 정렬 x 좌표가 펌웨어 layoutCharX()와 일치
// ===================================================================
{
    let sameAsFirmware = true;
    for (let d = 1; d <= 31; d++) {
        for (const text of [hangulDay(d), numericDay(d)]) {
            const count = Array.from(text).length;
            if (count > LAYOUT_MAX_CHARS) continue;   // 4자까지가 화면 한계 (LAYOUT_MAX_CHARS)
            const centered = isCenteredPreview(true, text);
            const got = getCharPositions(text, centered);
            for (let j = 0; j < count; j++) {
                const want = firmwareLayoutX(count, j, centered);
                if (got[j].x !== want) {
                    sameAsFirmware = false;
                    console.error(`  FAIL  "${text}" j=${j}: JS=${got[j].x} 펌웨어=${want}`);
                }
            }
        }
    }
    chk('날짜 1~31일의 x 좌표가 펌웨어 layoutCharX()와 같다', sameAsFirmware);

    // 이 수정의 핵심 회귀: 우측 정렬이면 마지막 글자가 시·분·초와 같은 열(x=96).
    // 가운데 정렬이면 2~3자에서 x=64/80에 남아 어긋났다 — 사용자가 보고한 증상.
    const day2 = getCharPositions("1일", false);
    const day3 = getCharPositions("이십일", false);
    const day4 = getCharPositions("이십일일", false);
    chk('2자 날짜의 "일" → x=96 (가운데 정렬이면 64)',
        day2[day2.length - 1].x === 96 && day2[day2.length - 1].x !== getCharPositions("1일", true)[1].x);
    chk('3자 날짜의 "일" → x=96 (가운데 정렬이면 80)',
        day3[day3.length - 1].x === 96 && day3[day3.length - 1].x !== getCharPositions("이십일", true)[2].x);
    chk('4자 날짜의 "일" → x=96 (정렬과 동일 — 화면 전체를 채운다)',
        day4[day4.length - 1].x === 96);

    // 사용자가 요구한 "통일성": 날짜의 "일"과 시·분·초의 단위 글자가 같은 열에 온다.
    const lastX = (text, isTitle) => {
        const p = getCharPositions(text, isCenteredPreview(isTitle, text));
        return p.length ? p[p.length - 1].x : null;
    };
    chk('날짜 "일"과 시/분/초 단위 글자가 같은 우측 열(x=96)에 정렬된다',
        lastX("24일", true) === 96 && lastX("이십사일", true) === 96
        && lastX("이십삼시", false) === 96 && lastX("오분", false) === 96 && lastX("구초", false) === 96);
}

// ===================================================================
// 4. 소스 배선 — 규칙이 한 곳(단일 진실원)에 있고 경로가 그것을 쓰는가
// ===================================================================
{
    const rlh = read('renderer_layout.h');
    const rlc = read('renderer_layout.cpp');
    const dm  = read('display_manager.cpp');
    const wp  = read('web_pages.h');

    chk('renderer_layout.h가 layoutIsDayText()를 선언한다',
        /bool layoutIsDayText\(const char\* text\);/.test(rlh));
    chk('layoutIsDayText()는 Arduino 없이 접미사(memcmp)로 판정한다',
        /memcmp\(text \+ \(textLen - unitLen\), DAY_UNIT_UTF8, unitLen\) == 0/.test(rlc));
    chk('display_manager.cpp의 우측 정렬 판정이 layoutIsDayText()를 쓴다',
        /static bool isRightAlignedText\(int idx, const String& text\) \{\s*\n\s*return isTitleScreenOf\(idx\) && layoutIsDayText\(text\.c_str\(\)\);/.test(dm));
    chk('isCenteredText()가 우측 정렬 갈래를 먼저 배제한다 (경로가 갈라지지 않는다)',
        /return !isRightAlignedText\(idx, text\) && \(isTitleScreenOf\(idx\) \|\| \(text == "정각"\)\);/.test(dm));
    chk('정적 경로·애니메이션 경로가 모두 isCenteredText()를 쓴다 (기존 규칙 유지)',
        (dm.match(/isCenteredText\(/g) || []).length >= 4);
    chk('웹 미리보기의 isC/isOC가 isCenteredPreview()를 쓴다 (펌웨어 미러)',
        /const isC = isCenteredPreview\(s === 0, targetTimeStrings\[s\]\);/.test(wp)
        && /const isOC = isCenteredPreview\(s === 0, lastTimeStrings\[s\]\);/.test(wp)
        && !/const isC = \(s === 0\) \|\|/.test(wp));
}

console.log(`\n=== 결과: ${pass} passed, ${fail} failed ===`);
process.exit(fail === 0 ? 0 : 1);
