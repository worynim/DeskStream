#!/usr/bin/env bash
# 네이티브(C++17) 단위 테스트 전체 실행 스크립트.
#
# [SYNC] 원본: ENG_Clock/test/js/run_all.sh (JS 전용이었던 것을 C++ 대상으로 확장).
#
# 이 스크립트가 존재해야 하는 이유:
#   순수 모듈(Arduino 의존 없음)만 네이티브로 검증한다 (AGENTS.md 테스트 규칙).
#   각 테스트를 손으로 컴파일하다 보면 **어떤 .cpp를 링크했는지**가 사看上만 남는다.
#   아래 표가 그 대응표를 단일 진실원으로 고정한다 — 소스를 추가할 때 이 표도 같이 고칠 것.
#
# 사용:  bash test/run_all.sh
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$HERE")"
BUILD="$HERE/.build"
mkdir -p "$BUILD"

# 테스트 이름 | 링크해야 하는 순수 모듈 .cpp
# header-only 모듈(utf8_len, i2c_retry_policy)은 .cpp가 없다.
TESTS=(
  "test_tz_util|tz_util.cpp"
  "test_utf8_len|"
  "test_i2c_retry_policy|"
  "test_chinese_time|chinese_time_core.cpp"
  "test_geometry|renderer_geometry.cpp"
  "test_layout|layout_engine.cpp renderer_geometry.cpp chinese_time_core.cpp"
)

fail=0
pass=0

for entry in "${TESTS[@]}"; do
  name="${entry%%|*}"
  deps="${entry#*|}"

  src="$HERE/$name.cpp"
  if [ ! -f "$src" ]; then
    echo "❌ $name — 소스 없음: $src"
    fail=$((fail + 1))
    continue
  fi

  objs=("$src")
  for d in $deps; do
    if [ ! -f "$ROOT/$d" ]; then
      echo "❌ $name — 링크 대상 없음: $ROOT/$d"
      fail=$((fail + 1))
      continue 2
    fi
    objs+=("$ROOT/$d")
  done

  bin="$BUILD/$name"
  if ! g++ -std=c++17 -Wall -I"$ROOT" -o "$bin" "${objs[@]}" 2>"$BUILD/$name.log"; then
    echo "❌ $name — 컴파일 실패"
    tail -20 "$BUILD/$name.log"
    fail=$((fail + 1))
    continue
  fi

  if out=$("$bin" 2>&1); then
    summary=$(printf '%s\n' "$out" | grep -iE 'passed|ok|assert' | tail -1)
    echo "✅ $name — ${summary:-통과}"
    pass=$((pass + 1))
  else
    echo "❌ $name"
    printf '%s\n' "$out" | tail -20
    fail=$((fail + 1))
  fi
done

echo "---"
echo "통과 $pass / 실패 $fail (네이티브 C++ 단위 테스트)"

# JS↔C++ 교차검증 — 브라우저 JS 미러가 펌웨어와 같은 값을 내는지 본다.
#   별도 프로세스라 위 표에 넣을 수 없다(다른 언어). 실패해도 C++ 결과를 가리지 않도록
#   **마지막에** 붙이고 별도로 집계한다.
echo "---"
echo "JS 교차검증 (test/js/run_all.sh)"
if bash "$HERE/js/run_all.sh"; then
  echo "통과 $((pass + 1)) / 실패 $fail"
else
  echo "통과 $pass / 실패 $((fail + 1))"
  fail=$((fail + 1))
fi

[ "$fail" -eq 0 ] || exit 1