# ENG_Clock 테스트

호스트(g++ / node)에서 돌리는 단위·교차 검증 모음. 펌웨어 업로드 없이 로직을 검증한다.

## 실행 방법

```bash
# JS ↔ 펌웨어 교차 검증 전체 (추천 — 한 번에 다 돌린다)
bash test/js/run_all.sh

# 네이티브 C++ 단위 테스트 (개별)
cd test
g++ -std=c++11 -I.. test_english_time.cpp ../english_time_core.cpp -o /tmp/test_english_time && /tmp/test_english_time
g++ -std=c++11 -I.. test_layout.cpp ../layout_engine.cpp ../renderer_geometry.cpp ../english_time_core.cpp -o /tmp/test_layout && /tmp/test_layout
g++ -std=c++11 -I.. test_tz_util.cpp ../tz_util.cpp -o /tmp/test_tz_util && /tmp/test_tz_util
g++ -std=c++11 -I.. test_geometry.cpp ../renderer_geometry.cpp -o /tmp/test_geometry && /tmp/test_geometry
```

## 파일 안내

| 파일 | 내용 |
|:--|:--|
| `test_english_time.cpp` | 순수 시간 변환기(`english_time_core`) 단위 테스트 |
| `test_layout.cpp` | 어절 단위 2줄 레이아웃 단위 테스트 |
| `test_tz_util.cpp` | POSIX TZ 문자열 검증 단위 테스트 |
| `test_geometry.cpp` | 글자 기하(192B/384B) 판별 단위 테스트 |
| `js/` | 웹 페이지 JS ↔ 펌웨어 C++ 교차 검증 (아래 표) |

## js/ — 교차 검증 스크립트

`run_all.sh`가 (1) 펌웨어 덤퍼 빌드 → (2) `web_pages.h`에서 배포 JS 추출 → (3) 대조 실행의 순서로 돌린다.
웹 미리보기 JS와 펌웨어 렌더러가 같은 결과를 내는지 **숫자로** 대조한다 (위험 #2 방어선).

| 스크립트 | 검증 내용 |
|:--|:--|
| `layout_crosscheck.mjs` | `layoutWrap` 어절 2줄 분리 대조 |
| `time_crosscheck.mjs` | 영어 시간 표현 대조 |
| `packGlyph_test.mjs` | 래스터라이저 (TTF → 비트맵) |
| `preview_cache_test.mjs` | 미리보기 캐시 (버그 회귀) |
| `web_fixes_test.mjs` | 웹 수정 회귀 (버그 1·2·3) |
| `firmware_wiring_test.mjs` | 펌웨어 배선 회귀 (수정 1·2·3) |
| `extract_from_web_page.mjs` | `web_pages.h` → `_extracted.mjs` 추출 (배포본 그대로 검증) |
| `make_baseline_check.mjs` | 세로 중앙 정렬 수동 검증 페이지 → `/tmp/eng_bias_check.html` |
| `make_browser_check.mjs` | 실제 브라우저 픽셀 검증 페이지 → `/tmp/eng_browser_check.html` |

`make_*_check.mjs`가 만든 HTML은 브라우저로 열어 확인한다 (puppeteer 등으로 자동화 가능).
`_extracted.mjs`는 생성물이므로 커밋하지 않는다 (`js/.gitignore` 참고).

## 규칙

- 컴파일된 바이너리는 커밋하지 않는다 (빌드 산출물은 `/tmp`에 두거나 실행 후 삭제).
- 펌웨어 코드를 바꾸면 대응 교차 검증 기대값도 함께 갱신한다.
- 새 코드에는 새 테스트 (AGENTS.md 테스트 규칙).
