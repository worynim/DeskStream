#!/usr/bin/env bash
# worynim@gmail.com
# 한글판 JS ↔ 펌웨어 배선 회귀 검증 (2026-10-05 신설).
#
# 중국어판 PLAN §📌 C: 이 판에는 test/js/ 하네스가 **없었다**. 그래서
#   · DOM 배선 결함(슬롯↔배지, 이름표 슬롯)을 잡을 방법이 없었고,
#   · <script> 블록이 통째로 죽는 부류(§12.6 #3)도 브라우저에서 처음 알았다.
#
# 사용: test/js/run_all.sh
# 전제: node 18+
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
rc=0
note() { printf '\n\033[1m== %s ==\033[0m\n' "$1"; }
mark_fail() { rc=1; printf '\033[31m[FAIL] %s\033[0m\n' "$1"; }

# --- 1. web_pages.h에서 JS 추출 (배포본을 그대로 검증한다) ---
note "web_pages.h에서 JS 추출"
if ! node "$HERE/extract_from_web_page.mjs"; then
    mark_fail "JS 추출 실패"; exit 1
fi

# --- 2. 삽입된 <script> 전문의 문법 검사 ---
# 문법 오류 하나로 블록 전체가 죽는다 — 정적 정규식 검사는 그때도 통과한다.
note "삽입된 <script> 문법 검사"
if ! node --check "$HERE/_full_script.mjs"; then
    mark_fail "web_pages.h의 <script> 블록에 문법 오류가 있다 (블록 전체가 죽는다)"
    exit 1
fi

# --- 3. 결함 회귀 (A-1·A-2·A-3·A-4·A-5·B-1) ---
note "폰트 업로드·슬롯 결함 회귀"
if node "$HERE/web_fixes_test.mjs"; then :; else mark_fail "web_fixes_test 실패"; fi

note "결과"
if [ "$rc" -eq 0 ]; then
    printf '\033[32m한글판 JS 검증 전체 통과\033[0m\n'
else
    printf '\033[31m실패한 검증이 있다\033[0m\n'
fi
exit "$rc"
