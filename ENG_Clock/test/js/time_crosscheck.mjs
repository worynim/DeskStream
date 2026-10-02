// worynim@gmail.com
/**
 * @file time_crosscheck.mjs
 * @brief 웹 페이지의 영어 시간 표현 생성기가 english_time_core.cpp와 동일한지 전수 대조한다
 * @details 웹 프리뷰가 "TWELVE"를 보여주는데 OLED에는 "TWELV"가 뜨는 사고를 막는다.
 *
 * @note [SYNC] 대조 대상은 english_time_core.cpp와, Arduino 래퍼인 english_time.cpp의
 *             getNumericHour 규칙(12h면 12로 래핑)까지 포함한다.
 *             요일 숫자(getNumericDay)는 [수정할 사항 3]으로 제거되어 더 이상 대조하지 않는다.
 *
 * 전제: /tmp/eng_time_dump 이 time_dump.cpp로 빌드되어 있어야 한다.
 */
import { execFileSync } from 'child_process';
import {
    numberToWords, hourToWords, minuteToWords, secondToWords,
    amPm, twoDigit, dayName, numericHour, dateString,
} from './_extracted.mjs';

const DUMPER = '/tmp/eng_time_dump';

/** 펌웨어와 동일한 포맷: `태그\t성공여부\t값` (빈 문자열이면 FAIL) */
const render = (tag, value) => `${tag}\t${value === '' ? 'FAIL' : 'OK'}\t${value}`;

/**
 * 펌웨어 출력과 같은 순서·태그로 JS 결과를 만든다.
 * 순서가 어긋나면 줄 단위 비교가 오탐을 내므로 time_dump.cpp의 출력 순서를 그대로 따른다.
 * @returns {string[]}
 */
function buildJsLines() {
    const out = [];
    for (let h = 0; h < 24; h++) {
        for (let k = 0; k < 2; k++) {          // k=0: 12시간제, k=1: 24시간제
            out.push(render(`HOUR_${h}_${k}`, hourToWords(h, k === 1)));
            out.push(render(`NHOUR_${h}_${k}`, numericHour(h, k === 1)));
        }
        out.push(render(`AMPM_${h}`, amPm(h)));
    }
    for (let n = 0; n < 60; n++) {
        out.push(render(`MIN_${n}`, minuteToWords(n)));
        out.push(render(`SEC_${n}`, secondToWords(n)));
        out.push(render(`NMIN_${n}`, twoDigit(n)));
    }
    for (let d = 0; d < 7; d++) out.push(render(`DAY_${d}`, dayName(d)));

    // [수정할 사항 3] 날짜 — time_dump.cpp의 dates()와 같은 순서·태그
    const MDAYS = [1, 2, 9, 10, 12, 15, 21, 25, 28, 30, 31];
    for (let mon = 1; mon <= 12; mon++) {
        for (const md of MDAYS) {
            for (let k = 0; k < 2; k++) out.push(render(`DATE_${mon}_${md}_${k}`, dateString(mon, md, k === 1)));
        }
    }
    for (const mon of [0, 13, -1]) {
        for (const md of [0, 32, -1]) out.push(render(`BADDATE_${mon}_${md}`, dateString(mon, md, false)));
    }

    // 범위 밖 입력 — 펌웨어는 빈 문자열을 남기고 JS도 같아야 한다
    for (const n of [-1, 60, 999]) {
        out.push(render(`BAD_n2w_${n}`, numberToWords(n)));
        out.push(render(`BAD_min_${n}`, minuteToWords(n)));
        out.push(render(`BAD_day_${n}`, dayName(n)));
    }
    return out;
}

function main() {
    const js = buildJsLines();
    const cpp = execFileSync(DUMPER, []).toString().split('\n').filter(Boolean);

    if (js.length !== cpp.length) {
        console.error(`FAIL: 케이스 수 불일치 — JS ${js.length}건 vs 펌웨어 ${cpp.length}건`);
        process.exit(1);
    }

    let fail = 0;
    for (let i = 0; i < js.length; i++) {
        if (js[i] !== cpp[i]) {
            fail++;
            console.error(`FAIL:\n  JS:  ${js[i]}\n  C++: ${cpp[i]}`);
        }
    }

    console.log(fail === 0
        ? `시간 표현 대조 통과: ${js.length}건 (JS ↔ english_time_core)`
        : `시간 표현 대조 실패: ${fail}/${js.length}건`);
    process.exit(fail === 0 ? 0 : 1);
}

main();