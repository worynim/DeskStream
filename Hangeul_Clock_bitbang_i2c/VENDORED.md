# 벤더링한 외부 라이브러리 (Vendored Third-Party Code)

이 스케치 폴더에는 외부 라이브러리 **Multi_BitBang**의 원본 소스가 그대로 들어 있다.
[`개요.md`](./개요.md) 20~24행의 지시에 따라 두 라이브러리를 검토한 뒤,
그중 Multi_BitBang만 실제로 사용한다.

## 1. Multi_BitBang — 원본 그대로 포함 (수정 없음)

| 항목 | 값 |
|:--|:--|
| 저장소 | https://github.com/bitbank2/Multi_BitBang |
| 커밋 | `40bb8210b8423d5daf0cd90062e5b8e4404a7a96` (master, 2026-10-10 취득) |
| 저작자 | Larry Bank &lt;bitbank@pobox.com&gt; · Copyright (c) 2019 BitBank Software, Inc. |
| 라이선스 | **GPLv3** — 전문은 [`Multi_BitBang_LICENSE.txt`](./Multi_BitBang_LICENSE.txt) |

| 이 폴더의 파일 | 원본 경로 |
|:--|:--|
| `Multi_BitBang.h` | `src/Multi_BitBang.h` |
| `Multi_BitBang.cpp` | `src/Multi_BitBang.cpp` |
| `Multi_BitBang_LICENSE.txt` | `LICENSE` |
| `Multi_BitBang.library.properties` | `library.properties` |

### 왜 스케치 루트에 평탄화했는가

Arduino 스케치는 **스케치 루트의 소스만** 컴파일하고, 하위 폴더는 `src/`만 재귀적으로
컴파일한다. 그런데 `src/`는 include 검색 경로에 추가되지 않아, 원본
`Multi_BitBang.cpp`의 `#include <Multi_BitBang.h>`(각괄호)가 해결되지 않는다.

원본을 **한 줄도 고치지 않기 위해** 소스를 스케치 루트로 옮겼다. 덕분에 Arduino IDE와
`arduino-cli` 양쪽에서 별도 설치 없이 그대로 빌드된다.

> `Multi_BitBang.library.properties`는 `.properties` 확장자를 유지하면 Arduino가
> 스케치 폴더를 라이브러리로 오인할 수 있어 이름 뒤에 확장자를 붙여 두었다(내용은 원본 그대로).

### 어디에 쓰는가

| 사용처 | API | 목적 |
|:--|:--|:--|
| `i2c_platform.cpp` `scanBusesForDiagnostics()` | `Multi_I2CInit` / `Multi_I2CScan` | 부팅 시 4버스에 0x3C가 응답하는지 확인 (브링업 진단) |

프레임 전송은 원본 API를 쓰지 않는다 — 원본은 `SetBus()` 전역 상태 기반의 **순차 전용**이라
4버스 동시 전송이 불가능하다. 그 확장은 프로젝트 자체 파일
[`bitbang_i2c.h`](./bitbang_i2c.h) / [`bitbang_i2c.cpp`](./bitbang_i2c.cpp)가 담당한다.
(순수 판단 로직은 [`bitbang_i2c_core.h`](./bitbang_i2c_core.h)로 분리해 호스트 테스트 대상으로 두었다.)

> ⚠ **호출 순서 주의**: `Multi_I2CInit()`은 `pinMode(INPUT/OUTPUT)`으로 핀을 잡는다.
> 반드시 `bitBang.begin()` **전에** 끝내야 한다. 나중에 부르면 BitBang 드라이버가 설정한
> Open-Drain + 입력활성 설정을 덮어쓴다.

## 2. Multi_OLED — 포함하지 않음 (검토만)

| 항목 | 값 |
|:--|:--|
| 저장소 | https://github.com/bitbank2/Multi_OLED |
| 판정 | ❌ **미채택** |

이유:

1. **결정적 이유 — 자체 폰트가 6x8 / 8x8 / 16x32 세 종류로 고정**이다. 이 펌웨어의
   핵심 자산인 **커스텀 64px 한글 비트맵 글리프 · 8종 애니메이션 · 웹 Font Studio
   미리보기**를 전혀 소화하지 못한다. 전면 채택은 렌더링 파이프라인 전체 재작성을
   뜻하며, 이는 `개요.md`의 "처음부터 새로 만들지 말고 복사한 뒤 수정" 방침과 정면
   배치된다.
2. **SSD1315를 지원하지 않는다.** Multi_OLED는 SSD1306/SH1106 명령 집합만 다룬다.
   (실기 패널은 SH1106으로 확인되었으므로 이 이유만으로는 기각되지 않는다 —
   그래도 1번이 결정적이다.)

대신 렌더링은 **U8g2를 유지**하고 전송 계층만 교체했다. U8g2에는 컨트롤러별 전용
드라이버가 있어 `config.h`의 `OLED_DRIVER` 한 줄로 고른다(SH1106 / SSD1315 / SSD1306).

## 3. 라이선스 주의

Multi_BitBang은 **GPLv3**다. 이 스케치를 배포할 때는 GPLv3 조건이 함께 적용된다.
GPL을 피해야 한다면 `scanBusesForDiagnostics()`를 자체 진단 코드로 대체하고
위 네 파일을 삭제하면 된다 — 프레임 전송 경로는 원본 라이브러리에 의존하지 않는다.
