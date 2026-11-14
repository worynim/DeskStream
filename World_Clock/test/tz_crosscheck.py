#!/usr/bin/env python3
# worynim@gmail.com
"""도시 표의 시간대 규칙을 시스템 시간대 데이터(zoneinfo)와 대조한다.

tz_dump 의 출력(표준 입력)을 읽어, 도시마다 대응하는 IANA 시간대(city_iana.json, 목록 순서)의
실제 UTC 차이와 다르면 실패한다. 서머타임 규칙이 바뀌었는지 확인하는 용도이기도 하다.
"""
import json
import os
import sys
from datetime import datetime, timezone
from zoneinfo import ZoneInfo

names = json.load(open(os.path.join(os.path.dirname(__file__), "city_iana.json")))
zones = [ZoneInfo(n) for n in names]
bad = total = 0
for line in sys.stdin:
    idx, t, off = map(int, line.split())
    want = datetime.fromtimestamp(t, timezone.utc).astimezone(zones[idx]).utcoffset().total_seconds() / 60
    total += 1
    if want != off:
        bad += 1
        if bad <= 10:
            print("FAIL %s %s: got %d, zoneinfo %d" % (names[idx], datetime.fromtimestamp(t, timezone.utc), off, want))
print("tz_crosscheck: %d samples, %d mismatches" % (total, bad))
sys.exit(1 if bad else 0)
