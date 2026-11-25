**English** | [한국어](README.ko.md)

# Hangeul_Clock Tests

A collection of native unit tests that run on the host (g++). They verify the logic without uploading firmware.

> Since v2.11.0, the TZ validation, I2C retry policy, UTF-8 detection and NTP resync policy tests have moved to the shared library:
> `bash libraries/DeskStreamCore/test/run_all.sh` (from the repository root). This folder holds only tests specific to the Korean clock.

## How to Run

```bash
cd test
g++ -std=c++11 -I.. test_geometry.cpp ../renderer_geometry.cpp -o /tmp/test_geometry && /tmp/test_geometry
g++ -std=c++11 -I.. test_layout.cpp ../renderer_layout.cpp -o /tmp/test_layout && /tmp/test_layout
```

## Files

| File | Contents |
|:--|:--|
| `test_geometry.cpp` | Glyph cell geometry detection regression test (blocks over-reads of 257–511 byte files) |
| `test_layout.cpp` | Glyph x-coordinate placement regression test (bug where a single character went off screen) + date ("...일") right-alignment detection (`layoutIsDayText`) |

## JS ↔ Firmware Wiring Checks (test/js/)

Compares the JS shipped inside `web_pages.h` with the firmware wiring. **Added 2026-10-06** —
before that this edition had no JS harness, so DOM wiring defects (slot↔badge, name tag slot)
and whole `<script>` blocks dying could only be discovered in the browser.

```bash
test/js/run_all.sh
```

| File | Contents |
|:--|:--|
| `js/extract_from_web_page.mjs` | Extracts symbols from the `<script>` in `web_pages.h` and generates `_extracted.mjs`·`_full_script.mjs` (generated files are not committed) |
| `js/web_fixes_test.mjs` | A-1·A-2·A-3·A-4·A-5·B-1 regressions — added while propagating the language-neutral fixes from the Chinese edition PLAN §📌 to this edition |
| `js/ip_dot_test.mjs` | IP screen inline dot and ink bottom alignment regression (2026-10-21) |
| `js/day_align_test.mjs` | Compares 24-hour date ("...일") right alignment — firmware `layoutIsDayText()`/`isCenteredText()` ↔ JS `isDayText()`/`isCenteredPreview()`/`getCharPositions()` (2026-11-01) |
| `js/run_all.sh` | Extract → `<script>` **syntax** check (`node --check`) → regression tests |

## Target Modules

Modules that can be built on the host must not depend on Arduino, like `renderer_layout`.

The pixel kinematics of snow assembly and split flap live in `renderer.cpp` (still dependent on Arduino/U8g2), so they
do not meet this condition and are **not** unit-test targets. The same formulas exist **separately** in the firmware and in the
JS mirror in `web_pages.h`, so automatically comparing the two requires first splitting them into a pure module —
the `test/js/` harness is ready, so once the split is done, add the comparison to `NAMES` in `extract_from_web_page.mjs` and
to `web_fixes_test.mjs`. **That split itself is still an open task.**

## Rules

- Do not commit compiled binaries (keep build outputs in `/tmp`).
- When firmware code changes, update the matching test expectations too.
- New code gets new tests (AGENTS.md test rules).
