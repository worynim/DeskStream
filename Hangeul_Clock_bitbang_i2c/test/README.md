# Hangeul_Clock_bitbang_i2c 테스트

호스트(g++)에서 돌리는 네이티브 단위 테스트 모음. 펌웨어 업로드 없이 로직을 검증한다.

> [v1.0.9] `test_i2c_retry_policy.cpp`는 **삭제**했다. 그 테스트가 검증하던 dirty 마스크
> 장부(`preparePageUpdate`/`pageMarkResult`/`mergeDirtyRange`)가 캡처·재생 방식으로
> 전송 계층을 바꾸면서 **전부 사라졌기** 때문이다. 검증 대상이 없는 테스트는 남기지 않는다.

## 실행 방법

```bash
cd test
g++ -std=c++11 -I.. test_bitbang_core.cpp -o /tmp/test_bitbang_core && /tmp/test_bitbang_core
g++ -std=c++11 -I.. test_tz_util.cpp ../tz_util.cpp -o /tmp/test_tz_util && /tmp/test_tz_util
g++ -std=c++11 -I.. test_utf8_len.cpp -o /tmp/test_utf8_len && /tmp/test_utf8_len
g++ -std=c++11 -I.. test_geometry.cpp ../renderer_geometry.cpp -o /tmp/test_geometry && /tmp/test_geometry
g++ -std=c++11 -I.. test_layout.cpp ../renderer_layout.cpp -o /tmp/test_layout && /tmp/test_layout
```

## 파일 안내

| 파일 | 내용 |
|:--|:--|
| `test_bitbang_core.cpp` | BitBang 인코딩 순수 로직. 비트 순서(MSB 먼저), ACK 극성(LOW가 ACK), 4버스 동시 전송의 열 구간 합집합. 인코더 출력을 **복조해 원바이트로 왕복**시키는 검증 포함 |
| `test_tz_util.cpp` | POSIX TZ 문자열 화이트리스트 검증 단위 테스트 |
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

**이 조건을 만족시키는 분리 방식의 예**: 비트뱅 드라이버(`bitbang_i2c.cpp`)는
`Arduino.h`·`soc/gpio_struct.h`에 의존해 테스트할 수 없지만, 그중 **판단 로직**
(비트 순서 · ACK 극성 · 4버스 열 구간 합집합)만 `bitbang_i2c_core.h`로 떼어냈다.
`test_bitbang_core.cpp`가 그 파일만 include 하므로 하드웨어 없이 검증된다.
새 로직을 추가할 때도 "GPIO를 만지는 코드"와 "값을 계산하는 코드"를 이렇게 가른다.

눈 조립·분할 플랩의 픽셀 운동학은 `renderer.cpp` 안에 있어(아직 Arduino/U8g2 의존) 이 조건을
만족하지 못하므로 단위 테스트 대상이 **아니다**. 펌웨어와 `web_pages.h`의 JS 미러에 같은 수식이
**각각 따로** 존재하므로, 두 운동학을 자동 대조하려면 먼저 순수 모듈로 분리해야 한다 —
`test/js/` 하네스는 만들어 두었으므로, 분리가 끝나면 `extract_from_web_page.mjs`의 `NAMES`와
`web_fixes_test.mjs`에 대조를 추가하면 된다. **그 분리 자체는 아직 남은 과제다.**

## 규칙

- 컴파일된 바이너리는 커밋하지 않는다 (빌드 산출물은 `/tmp`에 둔다).
- 펌웨어 코드를 바꾸면 대응 테스트 기대값도 함께 갱신한다.
- 새 코드에는 새 테스트 (AGENTS.md 테스트 규칙).
