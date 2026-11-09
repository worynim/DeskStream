#!/usr/bin/env bash
# DeskStreamCore 네이티브(C++17) 단위 테스트 전체 실행 스크립트.
#
# 순수 모듈(Arduino 의존 없음)만 호스트에서 검증한다.
# 아래 표가 "테스트 ↔ 링크할 .cpp" 대응의 단일 진실원이다 — 모듈을 추가하면 이 표도 같이 고칠 것.
# 출처: Chinese_Clock/test/run_all.sh (JS 교차검증 부분은 프로젝트 전용이라 제외)
#
# 사용:  bash libraries/DeskStreamCore/test/run_all.sh
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$(dirname "$HERE")/src"
BUILD="$HERE/.build"
mkdir -p "$BUILD"

# 테스트 이름 | 링크해야 하는 src/ 기준 .cpp (header-only 모듈은 비운다)
TESTS=(
  "test_tz_util|net/ds_tz_util.cpp"
  "test_utf8_len|"
  "test_i2c_retry_policy|"
  "test_time_sync_policy|"
  "test_json_lite|util/ds_json_lite.cpp"
  "test_button_fsm|"
  "test_canvas|display/ds_canvas.cpp"
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
    if [ ! -f "$SRC/$d" ]; then
      echo "❌ $name — 링크 대상 없음: $SRC/$d"
      fail=$((fail + 1))
      continue 2
    fi
    objs+=("$SRC/$d")
  done

  bin="$BUILD/$name"
  if ! g++ -std=c++17 -Wall -I"$SRC" -o "$bin" "${objs[@]}" 2>"$BUILD/$name.log"; then
    echo "❌ $name — 컴파일 실패"
    tail -20 "$BUILD/$name.log"
    fail=$((fail + 1))
    continue
  fi

  if out=$("$bin" 2>&1); then
    summary=$(printf '%s\n' "$out" | grep -iE 'passed' | tail -1)
    echo "✅ $name — ${summary:-통과}"
    pass=$((pass + 1))
  else
    echo "❌ $name"
    printf '%s\n' "$out" | tail -20
    fail=$((fail + 1))
  fi
done

echo "---"
echo "통과 $pass / 실패 $fail (DeskStreamCore 네이티브 단위 테스트)"

[ "$fail" -eq 0 ] || exit 1
