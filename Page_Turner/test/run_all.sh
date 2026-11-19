#!/bin/bash
# worynim@gmail.com
# Page_Turner 호스트 테스트: 하드웨어와 무관한 순수 로직 (g++ 필요)
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/page_turner_test"
mkdir -p "$OUT"
g++ -std=c++17 -Wall -Wextra -O1 test_logic.cpp ../model.cpp ../menu.cpp ../shutter_timer.cpp ../slots.cpp -o "$OUT/test_logic"
"$OUT/test_logic"
