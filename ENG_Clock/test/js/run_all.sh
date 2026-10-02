#!/usr/bin/env bash
# worynim@gmail.com
# JS ↔ 펌웨어 교차 검증 전체 실행.
#
# 위험 #2(JS/펌웨어 로직 이중화)의 방어선이다. 웹 페이지의 미리보기 JS와
# 펌웨어 C++가 같은 결과를 내는지 숫자로 대조한다.
#
# 사용: test/js/run_all.sh
# 전제: g++ (호스트), node 18+
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
CXX="${CXX:-g++}"
CXXFLAGS="-std=c++11 -Wall -Wextra -I$ROOT"

rc=0
note() { printf '\n\033[1m== %s ==\033[0m\n' "$1"; }
mark_fail() { rc=1; printf '\033[31m[FAIL] %s\033[0m\n' "$1"; }

# 프로젝트 경로에 공백이 있으므로 include 경로를 반드시 따옴표로 감싼다.
# (CXXFLAGS를 통째로 인용하면 -Wall 등이 인자로 전달되어 실패한다)
CXXFLAGS=(-std=c++11 -Wall -Wextra "-I$ROOT")

# --- 1. 펌웨어 더umper 빌드 (JS가 호출할 실제 C++ 로직) ---
note "펌웨어 더umper 빌드"
if ! $CXX "${CXXFLAGS[@]}" "$HERE/layout_dump.cpp" "$ROOT/layout_engine.cpp" \
        "$ROOT/renderer_geometry.cpp" -o /tmp/eng_layout_dump; then
    mark_fail "layout_dump 빌드 실패"; exit 1
fi
if ! $CXX "${CXXFLAGS[@]}" "$HERE/time_dump.cpp" "$ROOT/english_time_core.cpp" \
        -o /tmp/eng_time_dump; then
    mark_fail "time_dump 빌드 실패"; exit 1
fi
echo "빌드 완료: /tmp/eng_layout_dump, /tmp/eng_time_dump"

# --- 2. web_pages.h에서 JS 추출 (배포본을 그대로 검증한다) ---
note "web_pages.h에서 JS 추출"
if ! node "$HERE/extract_from_web_page.mjs"; then
    mark_fail "JS 추출 실패"; exit 1
fi

# --- 3. 대조 테스트 ---
run() {
    note "$1"
    if node "$2"; then :; else mark_fail "$1 실패"; fi
}
run "레이아웃 대조 (layoutWrap)"      "$HERE/layout_crosscheck.mjs"
run "시간 표현 대조 (english_time)"    "$HERE/time_crosscheck.mjs"
run "래스터라이저 (packGlyph)"         "$HERE/packGlyph_test.mjs"
run "미리보기 캐시 (버그 회귀)"        "$HERE/preview_cache_test.mjs"
run "웹 수정 회귀 (버그 1·2·3)"        "$HERE/web_fixes_test.mjs"
run "펌웨어 배선 회귀 (수정 1·2·3)"    "$HERE/firmware_wiring_test.mjs"

note "결과"
if [ "$rc" -eq 0 ]; then
    printf '\033[32mJS ↔ 펌웨어 교차 검증 전체 통과\033[0m\n'
else
    printf '\033[31m실패한 검증이 있다\033[0m\n'
fi
exit "$rc"