#!/bin/bash
# worynim@gmail.com
# World_Clock 호스트 테스트: 단위 테스트 + 시간대 규칙을 시스템 zoneinfo 와 대조 (g++, python3 필요)
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/world_clock_test"
mkdir -p "$OUT"
g++ -std=c++17 -Wall -Wextra -O1 test_tz_city.cpp ../tz_calc.cpp ../city_table.cpp -o "$OUT/test_tz_city"
"$OUT/test_tz_city"
g++ -std=c++17 -Wall -Wextra -O1 tz_dump.cpp ../tz_calc.cpp ../city_table.cpp -o "$OUT/tz_dump"
"$OUT/tz_dump" | python3 tz_crosscheck.py
