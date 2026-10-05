#!/usr/bin/env bash
# JS↔C++ 교차검증 실행 스크립트.
#
# [SYNC] 원본: ENG_Clock/test/js/run_all.sh
#
# 왜 별도인가:
#   test/run_all.sh는 **네이티브 C++ 단위 테스트** 러너다. 이 스크립트는 그 반대 —
#   브라우저에서 도는 JS가 펌웨어와 같은 결과를 내는지 본다. 두 언어 경계를 걸치는
#   검사는 네이티브 러너에 섞을 수 없다(JS를 C++로 번역해야 하므로 원문이 아니다).
#
# 순서:
#   1. extract_from_web_page.mjs — web_pages.h에서 심볼을 뽑는다 (검증 대상은 배포본이어야 한다)
#   2. zh_dump.cpp 빌드            — 펌웨어를 실제로 호출해 행을 낸다
#   3. layout_crosscheck.mjs      — JS 행과 펌웨어 행을 기계적으로 대조
#
# 사용: bash test/js/run_all.sh
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$(dirname "$HERE")")"
DUMPER=/tmp/zh_dump

fail=0

if ! node "$HERE/extract_from_web_page.mjs"; then
  echo "❌ 심볼 추출 실패"
  exit 1
fi

# 링크 대상은 ENG판 test/js와 같다 — 펌웨어 순수 모듈 3개.
#   [주의] zh_dump.cpp는 display_manager나 config에 의존하지 않는다. LAYOUT_MAX_CHARS는
#          layout_engine.h에 있는 컴파일 타점 상수라 헤더 include로 충분하다.
if ! g++ -std=c++17 -Wall -Wextra -I"$ROOT" -o "$DUMPER" \
     "$HERE/zh_dump.cpp" \
     "$ROOT/chinese_time_core.cpp" "$ROOT/layout_engine.cpp" "$ROOT/renderer_geometry.cpp" \
     2>"$HERE/zh_dump.log"; then
  echo "❌ zh_dump 빌드 실패"
  tail -20 "$HERE/zh_dump.log"
  exit 1
fi

if out=$(node "$HERE/layout_crosscheck.mjs" 2>&1); then
  printf '%s\n' "$out" | grep -E '커버리지|대조|통과' | sed 's/^/  /'
  echo "✅ crosscheck"
else
  echo "❌ crosscheck"
  printf '%s\n' "$out" | tail -30
  fail=1
fi

# 웹 결함 회귀 — 순수 함수 대조로는 못 잡는 배선·핸들러 동작을 본다.
if out=$(node "$HERE/web_fixes_test.mjs" 2>&1); then
  echo "$out" | grep -E 'passed' | sed 's/^/  /'
  echo "✅ web_fixes"
else
  echo "❌ web_fixes"
  printf '%s\n' "$out" | tail -30
  fail=1
fi

echo "---"
[ "$fail" -eq 0 ] && echo "JS 검증 통과" || echo "JS 검증 실패"
exit "$fail"