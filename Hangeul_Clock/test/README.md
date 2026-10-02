# Hangeul_Clock 테스트

호스트(g++)에서 돌리는 네이티브 단위 테스트 모음. 펌웨어 업로드 없이 로직을 검증한다.

## 실행 방법

```bash
cd test
g++ -std=c++11 -I.. test_tz_util.cpp ../tz_util.cpp -o /tmp/test_tz_util && /tmp/test_tz_util
```

## 파일 안내

| 파일 | 내용 |
|:--|:--|
| `test_tz_util.cpp` | POSIX TZ 문자열 화이트리스트 검증 단위 테스트 |

## 대상 모듈

호스트에서 빌드할 수 있는 모듈은 `tz_util`처럼 Arduino에 의존하지 않아야 한다.
눈 조립 픽셀 운동학은 `renderer.cpp` 안에 있어(아직 Arduino/U8g2 의존) 이 조건을 만족하지
못하므로 단위 테스트 대상이 아니다. 검증은 `web_pages.h`의 JS 미러와의 차분 테스트로 대신한다.

## 규칙

- 컴파일된 바이너리는 커밋하지 않는다 (빌드 산출물은 `/tmp`에 둔다).
- 펌웨어 코드를 바꾸면 대응 테스트 기대값도 함께 갱신한다.
- 새 코드에는 새 테스트 (AGENTS.md 테스트 규칙).
