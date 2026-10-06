# Hangeul_Clock 테스트

호스트(g++)에서 돌리는 네이티브 단위 테스트 모음. 펌웨어 업로드 없이 로직을 검증한다.

## 실행 방법

```bash
cd test
g++ -std=c++11 -I.. test_tz_util.cpp ../tz_util.cpp -o /tmp/test_tz_util && /tmp/test_tz_util
g++ -std=c++11 -I.. test_i2c_retry_policy.cpp -o /tmp/test_i2c_retry_policy && /tmp/test_i2c_retry_policy
g++ -std=c++11 -I.. test_utf8_len.cpp -o /tmp/test_utf8_len && /tmp/test_utf8_len
g++ -std=c++11 -I.. test_geometry.cpp ../renderer_geometry.cpp -o /tmp/test_geometry && /tmp/test_geometry
g++ -std=c++11 -I.. test_layout.cpp ../renderer_layout.cpp -o /tmp/test_layout && /tmp/test_layout
```

## 파일 안내

| 파일 | 내용 |
|:--|:--|
| `test_tz_util.cpp` | POSIX TZ 문자열 화이트리스트 검증 단위 테스트 |
| `test_i2c_retry_policy.cpp` | I2C dirty 마스크 갱신 정책 회귀 테스트 (전송 실패 시 페이지 재시도) |
| `test_utf8_len.cpp` | UTF-8 리딩 바이트 판정 회귀 테스트 (4바이트 문자 3바이트 오판정 버그) |
| `test_geometry.cpp` | 글자 셀 기하 판별 회귀 테스트 (257~511바이트 파일의 초과 읽음 차단) |
| `test_layout.cpp` | 글자 x 좌표 배치 회귀 테스트 (1자일 때 화면 밖으로 나가던 버그) |

## JS ↔ 펌웨어 배선 검증 (test/js/)

`web_pages.h`에 실려 배포되는 JS와 펌웨어 배선을 대조한다. **2026-10-06 신설** —
그전까지 이 판에는 JS 하네스가 없어, DOM 배선 결함(슬롯↔배지, 이름표 슬롯)과
`<script>` 블록이 통째로 죽는 부류를 브라우저에서 처음 알 수밖에 없었다.

```bash
test/js/run_all.sh
```

| 파일 | 내용 |
|:--|:--|
| `js/extract_from_web_page.mjs` | `web_pages.h`의 `<script>`에서 심볼을 뽑아 `_extracted.mjs`·`_full_script.mjs` 생성 (생성물은 커밋하지 않는다) |
| `js/web_fixes_test.mjs` | A-1·A-2·A-3·A-4·A-5·B-1 회귀 — 중국어판 PLAN §📌 의 언어 중립 수정을 이 판에 전파하며 추가 |
| `js/run_all.sh` | 추출 → `<script>` **문법** 검사(`node --check`) → 회귀 테스트 |

## 대상 모듈

호스트에서 빌드할 수 있는 모듈은 `tz_util`처럼 Arduino에 의존하지 않아야 한다.

눈 조립·분할 플랩의 픽셀 운동학은 `renderer.cpp` 안에 있어(아직 Arduino/U8g2 의존) 이 조건을
만족하지 못하므로 단위 테스트 대상이 **아니다**. 펌웨어와 `web_pages.h`의 JS 미러에 같은 수식이
**각각 따로** 존재하므로, 두 운동학을 자동 대조하려면 먼저 순수 모듈로 분리해야 한다 —
`test/js/` 하네스는 만들어 두었으므로, 분리가 끝나면 `extract_from_web_page.mjs`의 `NAMES`와
`web_fixes_test.mjs`에 대조를 추가하면 된다. **그 분리 자체는 아직 남은 과제다.**

## 규칙

- 컴파일된 바이너리는 커밋하지 않는다 (빌드 산출물은 `/tmp`에 둔다).
- 펌웨어 코드를 바꾸면 대응 테스트 기대값도 함께 갱신한다.
- 새 코드에는 새 테스트 (AGENTS.md 테스트 규칙).
