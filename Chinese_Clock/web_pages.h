// worynim@gmail.com
/**
 * @file web_pages.h
 * @brief 웹 설정 대시보드 및 폰트 스튜디오 리소스
 * @details HTML, CSS, JavaScript 등으로 구성된 임베디드 웹 페이지 리소스 관리 (PROGMEM 활용)
 *
 * @warning JS 로직은 펌웨어를 **행 단위로 미러링**한다. 아래 상수와 함수를 대응시키지 않으면
 *          미리보기와 실제 OLED 출력이 어긋난다.
 *
 *   JS                          펌웨어 (반드시 같이 수정)
 *   -------------------------   ------------------------------------------
 *   GLYPH_W/GLYPH_H/RASTER_W/   renderer_geometry.cpp GEOM_288B {32,48,6,48,-8,4}
 *   BYTES_PER_ROW/MAX_PER_LINE  renderer_geometry.cpp geometryForSize(288)
 *   X_OFFSET                    renderer_geometry.cpp xOffset(−8)  = −X_OFFSET
 *   ANIM_STEPS                  display_manager.cpp  ANIM_STEPS(16)
 *   layoutLine()                layout_engine.cpp    layoutLine()
 *   buildNumber()·hourToChars()· chinese_time_core.cpp chtime::*
 *   msToChars()·weekdayToChars()
 *   withUnit()                  chinese_time.cpp     withUnit()
 *   getChineseTimeStrings()     Chinese_Clock.ino    handleClockUpdate()
 *   drawDitheredChar()          renderer.cpp         drawDitheredChar()
 *   TRAD_MAP                    chinese_time_core.cpp TRADITIONAL_MAP
 *
 * @note [Step 10 완료] ENG판 JS는 **복사하지 않았다** — 어절 2줄 줄바꿈, 피치 사다리
 *       (linePitch/measureLineInk/measureLineFloor), 잉크 폭 측정 (glyphInkWidth/maxInkWidthOf/
 *       inkOfChar), 영어 시간 변환 (numberToWords 등)은 전부 죽은 코드라 옮기지 않고
 *       대체했다. 중국어판은 표현이 최장 4자이고 한자가 정사각형이므로
 *       4 × 32 = 128 = 화면 폭이라 줄바꿈도 피치 계산도 필요 없다 (PLAN §5·§6.11).
 */
#ifndef WEB_PAGES_H
#define WEB_PAGES_H

#include <pgmspace.h>

// === 영어판 Font Studio HTML ===
const char font_studio_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-Hans">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>中文字典钟 Font Studio</title>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Outfit:wght@400;600&display=swap');
        :root { --primary: #00f2fe; --secondary: #4facfe; --bg: #0b0e14; --card: rgba(255, 255, 255, 0.05); }
        body { background: var(--bg); color: #fff; font-family: 'Outfit', system-ui, sans-serif; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
        .glass { background: var(--card); backdrop-filter: blur(15px); border: 1px solid rgba(255,255,255,0.1); border-radius: 24px; padding: 30px; width: 100%; max-width: 800px; box-shadow: 0 20px 50px rgba(0,0,0,0.5); }
        h1 { font-weight: 600; font-size: 2.2rem; background: linear-gradient(135deg, #00f2fe 0%, #4facfe 100%); -webkit-background-clip: text; -webkit-text-fill-color: transparent; text-align: center; margin-top:0; }
        .desc { text-align: center; color: #888; font-size: 0.9rem; margin-bottom: 12px; }
        .hint { text-align: center; color: #6a6a6a; font-size: 0.78rem; margin-bottom: 22px; line-height: 1.5; }
        .hint b { color: var(--primary); font-weight: 600; }
        .setup-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin-bottom: 25px; }
        .field { display: flex; flex-direction: column; gap: 8px; }
        label { font-size: 0.85rem; color: #aaa; font-weight: 500; }
        input[type="file"], input[type="range"] { background: rgba(0,0,0,0.3); border: 1px solid rgba(255,255,255,0.1); border-radius: 10px; padding: 10px; color: #fff; }

        button { padding: 15px; border-radius: 12px; border: none; font-weight: 600; cursor: pointer; transition: 0.2s; }
        .btn-apply { background: var(--primary); color: #000; margin-top: 15px; width: 100%; }
        .btn-apply:hover { box-shadow: 0 0 15px var(--primary); transform: translateY(-2px); }
        .btn-apply:disabled { opacity: 0.3; cursor: not-allowed; }

        .status-msg { text-align: center; font-size: 0.85rem; color: var(--primary); margin: 15px 0; min-height: 1.2rem; }
        .progress-wrap { width: 100%; height: 6px; background: rgba(255,255,255,0.1); border-radius: 3px; display: none; overflow: hidden; margin-top: 5px; }
        .progress-fill { height: 100%; width: 0%; background: var(--primary); transition: width 0.2s; }

        select { background: rgba(0,0,0,0.3); border: 1px solid rgba(255,255,255,0.1); border-radius: 10px; padding: 12px; color: #fff; width: 100%; cursor: pointer; outline: none; appearance: none; }
        select:focus { border-color: var(--primary); background: rgba(0,0,0,0.5); }
        option { background: #1a1e26; color: #fff; }

        .preview-list { display: flex; flex-wrap: wrap; gap: 10px; margin: 15px 0; justify-content: center; }
        .preview-item { display: flex; flex-direction: column; align-items: center; gap: 8px; background: rgba(255,255,255,0.04); padding: 12px 8px; border-radius: 12px; border: 1px solid #333; width: 145px; }
        .preview-label { width: 100%; font-size: 0.7rem; color: #888; text-align: center; }
        canvas { background: #000; border-radius: 4px; image-rendering: pixelated; }

        .inventory { display: flex; flex-wrap: wrap; gap: 4px; margin-top: 25px; justify-content: center; }
        .badge { width: 28px; height: 28px; display: flex; align-items: center; justify-content: center; font-size: 0.75rem; background: rgba(255,255,255,0.03); border-radius: 4px; color: #444; border: 1px solid transparent; }
        .badge.active { color: var(--primary); border-color: rgba(0,242,254,0.3); background: rgba(0,242,254,0.1); }
        .footer { margin-top: 30px; text-align: center; border-top: 1px solid rgba(255,255,255,0.05); padding-top: 20px; }
        .footer a { color: #555; text-decoration: none; font-size: 0.75rem; letter-spacing: 0.05em; transition: all 0.3s ease; }
        .footer a:hover { color: var(--primary); text-shadow: 0 0 8px rgba(0,242,254,0.5); }
    </style>
</head>
<body>
    <div class="glass">
        <h1>中文字典钟 Font Studio</h1>
        <p class="desc">DeskStream Project &middot; Current font: <span id="curFont" style="color:var(--primary)">-</span></p>

        <div class="setup-grid">
            <div class="field">
                <label>1. Font file (.ttf, .otf)</label>
                <input type="file" id="fIn" accept=".ttf,.otf">
            </div>
            <div class="field">
                <label>2. Font size: <span id="sVal">32</span>px <span style="color:#666">(pitch is 32px — wider overlaps)</span></label>
                <input type="range" id="sIn" min="20" max="40" value="32">
            </div>
            <div class="field">
                <label>3. Character set (BTN2 short)</label>
                <select id="scriptType">
                    <option value="0">简体 &mdash; Simplified</option>
                    <option value="1">繁體 &mdash; Traditional</option>
                    <option value="2">數字 &mdash; Numeric (13:05)</option>
                </select>
            </div>
            <div class="field">
                <label>4. Storage slot (BTN3 long)</label>
                <select id="fontSlot">
                    <option value="0">Slot 0 (existing font)</option>
                    <option value="1">Slot 1</option>
                    <option value="2">Slot 2</option>
                    <option value="3">Slot 3</option>
                    <option value="4">Slot 4</option>
                </select>
            </div>
        </div>

        <div class="setup-grid">
            <div class="field">
                <label>5. Animation (BTN3 short)</label>
                <select id="animMode">
                    <option value="0">0. OFF</option>
                    <option value="1">1. Scroll Up</option>
                    <option value="2">2. Scroll Down</option>
                    <option value="3">3. Vertical Flip</option>
                    <option value="4">4. Dithered Fade</option>
                    <option value="5">5. Zoom In/Out</option>
                </select>
            </div>
            <div class="field">
                <label>6. Hour format (BTN2 long)</label>
                <select id="hourFormat">
                    <option value="0">12-hour (上午 / 下午)</option>
                    <option value="1">24-hour (星期三 / 0&ndash;23)</option>
                </select>
            </div>
            <div class="field">
                <label>7. Hourly chime (BTN1 short)</label>
                <select id="chime">
                    <option value="0">OFF</option>
                    <option value="1">ON</option>
                </select>
            </div>
            <div class="field">
                <label>8. Screen flip (BTN1 long)</label>
                <select id="flipMode">
                    <option value="0">NORMAL</option>
                    <option value="1">FLIP</option>
                </select>
            </div>
            <div class="field">
                <label>9. Colour invert (BTN4 long)</label>
                <select id="invertMode">
                    <option value="0">NORMAL (Black BG)</option>
                    <option value="1">INVERT (White BG)</option>
                </select>
            </div>
            <div class="field">
                <label>10. OLED brightness: <span id="brVal">1</span></label>
                <input type="range" id="brIn" min="1" max="255" value="1">
            </div>
        </div>

        <div class="setup-grid">
            <div class="field">
                <label>11. Timezone (DST is applied automatically)</label>
                <select id="tzSel">
                    <option value="KST-9">Asia/Seoul (UTC+9)</option>
                    <option value="JST-9">Asia/Tokyo (UTC+9)</option>
                    <option value="CST-8">Asia/Shanghai (UTC+8)</option>
                    <option value="IST-5:30">Asia/Kolkata (UTC+5:30)</option>
                    <option value="GMT0BST,M3.5.0/1,M10.5.0">Europe/London (UTC+0/+1)</option>
                    <option value="CET-1CEST,M3.5.0,M10.5.0/3">Europe/Berlin (UTC+1/+2)</option>
                    <option value="AEST-10AEDT,M10.1.0,M4.1.0/3">Australia/Sydney (UTC+10/+11)</option>
                    <option value="EST5EDT,M3.2.0,M11.1.0">America/New_York (UTC-5/-4)</option>
                    <option value="CST6CDT,M3.2.0,M11.1.0">America/Chicago (UTC-6/-5)</option>
                    <option value="MST7MDT,M3.2.0,M11.1.0">America/Denver (UTC-7/-6)</option>
                    <option value="PST8PDT,M3.2.0,M11.1.0">America/Los_Angeles (UTC-8/-7)</option>
                    <option value="UTC0">UTC (no offset)</option>
                    <option value="CUSTOM">Custom&hellip; (type a POSIX TZ below)</option>
                </select>
            </div>
            <div class="field">
                <label>POSIX TZ string <span id="tzHint" style="color:#666">(read-only)</span></label>
                <input type="text" id="tzCustom" maxlength="47" spellcheck="false" readonly
                       style="background:rgba(0,0,0,0.3);border:1px solid rgba(255,255,255,0.1);border-radius:10px;padding:12px;color:#fff;font-family:ui-monospace,monospace;width:100%;">
            </div>
        </div>

        <div class="preview-list">
            <div class="preview-item"><div class="preview-label">SCREEN 1</div><canvas id="p0" width="128" height="64"></canvas></div>
            <div class="preview-item"><div class="preview-label">SCREEN 2</div><canvas id="p1" width="128" height="64"></canvas></div>
            <div class="preview-item"><div class="preview-label">SCREEN 3</div><canvas id="p2" width="128" height="64"></canvas></div>
            <div class="preview-item"><div class="preview-label">SCREEN 4</div><canvas id="p3" width="128" height="64"></canvas></div>
        </div>

        <div class="progress-wrap" id="pWrap"><div class="progress-fill" id="pFill"></div></div>
        <div id="status" class="status-msg">Loading settings&hellip;</div>

        <button class="btn-apply" id="apply" onclick="processAll()" disabled>Upload Font Set</button>

        <div class="inventory" id="inv"></div>
        <div class="footer">
            <a href="https://fonts.google.com/" target="_blank">Free fonts (Google Fonts)</a>
        </div>
    </div>

    <script>
        // ==== 펌웨어 기하 상수와 1:1 대응 (renderer_geometry.cpp GEOM_288B) ====
        // 하나당 한 줄: 테스트 하네스가 이 상수들을 심볼 단위로 뽑아 대조하기 때문이다.
        const GLYPH_W = 32;   // 피치(셀 간격) — 레이아웃은 이 값만 쓴다 (GEOM_288B glyphW)
        const GLYPH_H = 48;   // 래스터 높이 — 한자 잉크 32px + 여유 (GEOM_288B glyphH)
        const RASTER_W = 48;  // 래스터 폭 — 잉크가 여기 중앙에 놓인다 (GEOM_288B drawW)
        const BYTES_PER_ROW = 6;   // 48px → 6B/행 × 48행 = 288B/글자 (GEOM_288B bytesPerRow)
        const GLYPH_SIZE = BYTES_PER_ROW * GLYPH_H;   // 288 — 업로드하는 파일 크기
        const MAX_PER_LINE = 4;   // 4 × 32 = 128 = 화면 폭 (GEOM_288B maxPerLine)
        const LINE_HEIGHT = 48;   // 1줄 = 래스터 높이 (config.h LINE_HEIGHT)
        const SCREEN_W = 128;
        const SCREEN_H = 64;
        // [TDZ] 이 줄은 SCREEN_H보다 **아래에 있어야 했다.** 위에서 쓰면
        //   const의 시간적 사각(TDZ) 때문에 브라우저가 ReferenceError를 던지고
        //   <script> 블록 전체가 죽는다 — 뒤의 함수 선언이 아무것도 실행되지 않는다.
        //   하네스가 이 순서를 첫걸음에 잡았다.
        const LINE_TOP_Y = (SCREEN_H - LINE_HEIGHT) / 2;   // 8 — renderer.h LINE_TOP_Y
        // 래스터가 피치보다 넓은 만큼의 보정 — 그릴 때 x − X_OFFSET 이고,
        // 펌웨어 xOffset(−8) = −X_OFFSET 이다.
        // [ENG판 차이] ENG판은 잉크가 옆 글자 위로 겹쳐도 됐지만, 중국어판은 한자 판독이
        //   깨지므로 48px 래스터는 32px 피치 안에서 잘린다 (PLAN §5.4).
        const X_OFFSET = (RASTER_W - GLYPH_W) / 2;   // 8
        // 애니메이션 프레임 수 — display_manager.cpp ANIM_STEPS
        const ANIM_STEPS = 16;

        // ==== 간체 문자집합 — 34자 (PLAN §4.1) ====
        // 숫자 10 + 零一二三四五六七八九十 11 + 两 1 + 点/分/秒/时 4 + 整/半 2 + 上/下/午 3 + 星/期/日 3
        //   = 34자. **업로드는 문자판별로 이 34자가 아니라 아래 ALL_CHARS(37자)를 쓴다** —
        //   슬롯 하나가 두 문자판을 다 받게 하려는 것(§4.2b)이라 37 × 288B = 10,656 B/슬롯.
        // 공백은 넣지 않는다 — layoutLine()이 어절 사이 공백을 빈 칸 한 칸으로 만든다.
        //
        // ⚠ PLAN §6.11a의 리터럴은 `...两点时分秒时...` 으로 되어 있어 **时가 두 번** 나온다
        //   (점/분/초/시 4자여야 하는데 5자가 된다) → 35자, 번체도 35자로 잘못 나간다.
        //   JS↔C++ 전수 대조에서 발견했다. 아래는 수정본이다.
        const CHARS_SIMPLIFIED = ("0123456789零一二三四五六七八九十两点分秒时整半上下午星期日").split("");
        // ==== 번체 문자집합 — 34자 (간체와 같은 개수) ====
        // ⚠ 2벌 문자열을 직접 적지 않는다 (PLAN §6.11a — 오타 위험. 실제로 한 번 틀렸다).
        //   치환 규칙은 펌웨어 chinese_time_core.cpp TRADITIONAL_MAP과 **같은 3쌍뿐**이며,
        //   test/js/layout_crosscheck.mjs가 두 표를 소스 텍스트에서 뽑아 정적으로 대조한다.
        const TRAD_MAP = { "点": "點", "时": "時", "两": "兩" };
        const CHARS_TRADITIONAL = CHARS_SIMPLIFIED.map(c => TRAD_MAP[c] || c);

        // ==== 업로드할 글자집합 — **두 문자판의 합집합 37자** ====
        //   [사용자 제안 2026-10-05] 슬롯 하나가 어느 문자판이든 되게 미리 다 넣어 둔다.
        //   문자판을 고를 때만 34자씩 올리면 슬롯이 문자판에 종속되어 버린다
        //   (한 슬롯 = 한 문자판). 그러면 문자판↔슬롯을 서로 분리할 수 없다.
        //   차이는 3글자(點·時·兩) = 864 B뿐이고, 덕분에 연동 규칙 자체가 필요 없어진다.
        //   삽입 순서를 유지해 간체 34자 → 번체 추가 3자 순이 된다 (Set은 삽입 순서를 보존한다).
        const ALL_CHARS = [...new Set([...CHARS_SIMPLIFIED, ...CHARS_TRADITIONAL])];

        // 슬라이더 범위 — config.h FONT_SLIDER_MIN/MAX/DEFAULT
        const SLIDER_MIN = 20, SLIDER_MAX = 40, SLIDER_DEFAULT = 32;

        /** 펌웨어 저장 필드 script_type(0=간체 1=번체) → 글자 목록 */
        function charsForScript(scriptType) {
            return scriptType === 1 ? CHARS_TRADITIONAL : CHARS_SIMPLIFIED;
        }

        let bitmapCache = {};
        // 한자 잉크 합집합을 래스터 세로 중앙에 놓기 위한 fillText y (정렬점 절대 y).
        // 주의: "이동량(shift)"이 아니라 **그릴 y 값 자체**다 (ENG v4 회귀, PLAN §6.13e).
        let glyphBaselineY = GLYPH_H / 2;
        // [미사용 — 잉크 폭·피치 사다리 제거]
        //   ENG판은 잉크 폭을 재서 피치를 늘렸다(§6.16). 중국어판은 한자가 정사각형이고
        //   피치가 32로 고정이라 잉크 폭을 잴 이유가 없다. 잘라 쓰는 쪽이 없으므로
        //   previewInkWidth / previewInkByChar / inkWidthOf를 옮기지 않았다.
        const els = {
            anim: document.getElementById('animMode'),
            hour: document.getElementById('hourFormat'),
            chime: document.getElementById('chime'),
            flip: document.getElementById('flipMode'),
            invert: document.getElementById('invertMode'),
            status: document.getElementById('status'),
            curFont: document.getElementById('curFont'),
            slot: document.getElementById('fontSlot'),
            apply: document.getElementById('apply'),
            pFill: document.getElementById('pFill'),
            pWrap: document.getElementById('pWrap'),
            sIn: document.getElementById('sIn'),
            fIn: document.getElementById('fIn'),
            brIn: document.getElementById('brIn'),
            brVal: document.getElementById('brVal'),
            script: document.getElementById('scriptType'),
            tzSel: document.getElementById('tzSel'),
            tzCustom: document.getElementById('tzCustom'),
            tzHint: document.getElementById('tzHint')
        };

        /**
         * 문자패드(인벤토리 배지)를 현재 문자판에 맞춰 다시 그린다
         * @param {string[]} chars 문자판의 글자 목록
         * @note 폰트나 문자판이 바뀌면 집합이 달라지므로 배지도 다시 만들어야 한다.
         *       ENG판은 집합이 고정이라 한 번만 그렸지만, 중국어판은 간체/번체가 다르다.
         */
        function rebuildInventory(chars) {
            const inv = document.getElementById('inv');
            inv.textContent = '';
            for (const c of chars) {
                const d = document.createElement('div');
                d.className = 'badge'; d.id = 'b_' + c; d.innerText = c;
                inv.appendChild(d);
            }
        }
        rebuildInventory(ALL_CHARS);

        /**
         * 배지의 "업로드됨" 표시를 **전부** 지운다
         * @note 배지 상태는 **슬롯마다 다르다**. 그런데 우리는 지금 슬롯에 어떤 글자가
         *       들어 있는지 펌웨어에 물어볼 수 없다(그런 API가 없다). 그래서 슬롯을 바꾸면
         *       이전 슬롯의 표시를 물려받은 **lie**(없는 글자를 이미 있는 것처럼 보임)를
         *       지우는 게 정직하다 — "모른다"를 "있다"로 표시하지 않는다.
         * @note 업로드(processAll)는 슬롯을 바꾸지 않고 끝까지 진행되므로, 이 초기화는
         *       슬롯을 **고르는** 순간에만 일어난다.
         */
        function clearInventoryState() {
            document.querySelectorAll('#inv .badge').forEach(b => b.classList.remove('active'));
        }

        /**
         * [버그 2 수정] 설정 필드 한 장의 정의.
         * 폴링(fetchConfig)과 저장(saveConfig)이 **같은 표**를 보게 만들어
         * 한쪽만 고쳐지는 사고(설정을 바꿨는데 반영이 안 됨)를 구조적으로 막는다.
         * key = 펌웨어 web_manager.cpp 가 파싱하는 JSON 키, el = els 의 키.
         */
        const CONFIG_FIELDS = [
            { key: 'anim_mode',     el: 'anim',      def: 1 },
            { key: 'hour_format',   el: 'hour',      def: 0 },
            { key: 'chime_enabled', el: 'chime',     def: false, isBool: true },
            { key: 'is_flipped',    el: 'flip',      def: false, isBool: true },
            { key: 'is_inverted',   el: 'invert',    def: false, isBool: true },
            { key: 'font_slot',     el: 'slot',      def: 0 },
            { key: 'brightness',    el: 'brIn',      def: 1 },
            // 표시 방식 (简体/繁體/數字). 펌웨어 저장 필드는 script_type + display_mode
            //   두 개지만 **사용자에게는 하나의 선택지**다 — 3번 항목 하나로 합친다.
            //   POST도 GET도 이 값 하나만 쓴다(script_type/display_mode를 따로 안 보낸다).
            //   정수로 보낸다 — 문자열 파싱 실패를 원천 봉쇄.
            { key: 'presentation',  el: 'script',    def: 0 },
        ];

        // [표시 방식 — 펌웨어 config.h의 PRESENTATION_* 미러]
        const PRESENTATION_SIMPLIFIED = 0;
        const PRESENTATION_TRADITIONAL = 1;
        const PRESENTATION_NUMERIC = 2;
        const PRESENTATION_COUNT = 3;

        /**
         * [버그 2 수정] 5초 폴링이 이 필드를 덮어써도 되는가?
         *
         * 원인: saveConfig()가 변경 **전체**를 한 번에 POST하는데, 그 응답이 도착하기 전에
         *       도착한 폴링 응답이 옛 값을 컨트롤에 써 버린다. 사용자는 그제서야 바뀐 UI를 보고
         *       다른 항목을 건드리면, 되돌아간 값으로 다시 저장해 변경이 증발한다.
         *       시간대가 아니라 display_mode 같은 항목에서도 같은 일이 일어났다.
         *
         * @param {string} key JSON 키 (CONFIG_FIELDS[].key 또는 'timezone')
         * @param {Element} el  해당 컨트롤
         * @param {{saving:boolean, pending:Set<string>, active:Element|null}} state 현재 상태
         * @returns {boolean} true면 서버 값으로 덮어써도 된다
         */
        function pollCanOverwrite(key, el, state) {
            if (state.saving) return false;              // 저장 중 → 서버 값은 아직 옛값일 수 있다
            if (state.pending.has(key)) return false;    // 서버 반영을 아직 확인 못 함
            if (el && el === state.active) return false;  // 사용자가 조작 중인 컨트롤
            return true;
        }

        /** 서버 값(숫자/불리언) → <select>/<input> 의 .value 문자열 */
        function fieldToControl(raw, isBool) {
            if (isBool) return raw ? "1" : "0";
            return raw.toString();
        }

        // 아직 서버 반영을 확인하지 못한 필드. 저장이 성공하면 비워진다.
        let saving = false;
        const pending = new Set();

        async function fetchConfig() {
            try {
                const res = await fetch('/api/config');
                const data = await res.json();
                const state = { saving: saving, pending: pending, active: document.activeElement };

                for (const f of CONFIG_FIELDS) {
                    const el = els[f.el];
                    if (!pollCanOverwrite(f.key, el, state)) continue;
                    const raw = (data[f.key] !== undefined && data[f.key] !== null) ? data[f.key] : f.def;
                    el.value = fieldToControl(raw, f.isBool);
                    if (f.el === 'brIn') els.brVal.innerText = el.value;
                }

                // [개선] 시간대: 저장된 POSIX 문자열과 일치하는 옵션을 선택하고,
                // 목록에 없는 문자열이면 Custom 모드로 입력칸을 연다.
                if (typeof data.timezone === 'string' && data.timezone.length
                    && pollCanOverwrite('timezone', els.tzCustom, state)
                    && els.tzSel !== state.active) {
                    let matched = false;
                    for (const opt of els.tzSel.options) {
                        if (opt.value !== 'CUSTOM' && opt.value === data.timezone) {
                            els.tzSel.value = opt.value; matched = true; break;
                        }
                    }
                    if (!matched) els.tzSel.value = 'CUSTOM';
                    setTzEditable(els.tzSel.value === 'CUSTOM', data.timezone);
                }

                if (data.slot_names) {
                    for (let i = 0; i < 5; i++) {
                        const opt = els.slot.options[i];
                        if (opt) opt.innerText = `Slot ${i} (${data.slot_names[i]})`;
                    }
                }

                els.curFont.innerText = `Slot ${data.font_slot}: ${data.font_name || 'System Default'}`;
                els.status.innerText = "Settings synced";
            } catch (e) { els.status.innerText = "Failed to load settings"; }
        }

        /** Custom 모드 여부에 따라 TZ 입력칸을 편집 가능/읽기전용으로 전환한다 */
        function setTzEditable(editable, value) {
            els.tzCustom.readOnly = !editable;
            els.tzHint.innerText = editable ? "(editable — send to apply)" : "(read-only)";
            if (value !== undefined) els.tzCustom.value = value;
        }

        async function saveConfig() {
            els.status.innerText = "Saving...";
            const body = { timezone: els.tzCustom.value };
            for (const f of CONFIG_FIELDS) {
                const el = els[f.el];
                pending.add(f.key);
                body[f.key] = f.isBool ? (el.value === "1") : parseInt(el.value);
            }
            pending.add('timezone');
            saving = true;
            try {
                const res = await fetch('/api/config', { method: 'POST', body: JSON.stringify(body) });
                // [버그 2] 응답을 확인하지 않으면 거절된 설정(예: 잘못된 시간대)도
                // "Settings saved" 로 보인다. fetch는 네트워크 단절에서만 reject 된다.
                if (!res.ok) throw new Error(`HTTP ${res.status}`);
                // 서버가 값을 받았다고 확인됐으므로 이제 폴링이 덮어써도 된다.
                for (const f of CONFIG_FIELDS) pending.delete(f.key);
                pending.delete('timezone');
                els.status.innerText = "Settings saved";
            } catch (e) {
                // 실패하면 pending를 지우지 않는다. 그래야 다음 폴링이 사용자 값을
                // 지워버리지 않고, 사용자가 재시도할 수 있다.
                els.status.innerText = `Save failed (${e.message}) — check the value and retry`;
            } finally { saving = false; }
        }

        // [개선] 시간대 선택: 옵션의 value가 곧 POSIX TZ 문자열이므로 별도 매핑 테이블이 없다.
        els.tzSel.onchange = () => {
            if (els.tzSel.value === 'CUSTOM') setTzEditable(true);   // 값은 사용자가 입력
            else { setTzEditable(false, els.tzSel.value); saveConfig(); }
        };
        els.tzCustom.onchange = saveConfig;

        // [버그 2 수정] 바꾸는 즉시 pending 에 넣는다. saveConfig() 응답이 오기 전까지
        // 5초 폴링이 이 값을 옛 값으로 되돌리지 않는다.
        // `script`·`slot`은 여기서 바인딩하지 않는다 — 둘 다 전용 핸들러가 더 한다.
        //   여기서도 바인딩하면 그 핸들러가 덮어쓴다.
CONFIG_FIELDS.filter(f => f.el !== 'script' && f.el !== 'slot')
              .forEach(f => { els[f.el].onchange = saveConfig; });
        // 슬롯 변경 = 저장 + **배지 상태 초기화**.
        //   [사용자 보고 2026-10-05] 슬롯만 바꿨는데 slot0에서 올렸던 글자가 여전히
        //   "업로드됨"으로 표시돼, slot1에 없는 글자가 있는 것처럼 보였다.
        //   배지 상태를 슬롯에 묶지 않으면 매번 이런 오독이 생긴다.
        els.slot.onchange = () => {
            saveConfig();
            clearInventoryState();
        };
        // 표시 방식 변경 = **표현만** 바뀐다. 슬롯도, 재고 목록도 건드리지 않는다.
        //   [사용자 보고 2026-10-05] 예전엔 여기서 els.slot.value를 덮어써서
        //   "Storage slot"이 멋대로 바뀌어 보였고, 그 결과 슬롯↔문자판이 묶여 버렸다.
        //   슬롯에는 두 문자판 글자를 **합집합 37자**로 미리 넣어 두므로 슬롯은 그대로여도 된다.
        els.script.onchange = () => {
            // saveConfig()는 CONFIG_FIELDS 전체를 한 번에 POST한다.
            //   슬롯을 안 바꾸더라도 presentation은 저장해야 하므로 **항상** 불러야 한다.
            saveConfig();
            refreshPreviewCache();
        };
        els.brIn.oninput = () => { els.brVal.innerText = els.brIn.value; };
        fetchConfig();
        setInterval(fetchConfig, 5000);

        const pCtx = [0, 1, 2, 3].map(i => document.getElementById(`p${i}`).getContext('2d'));
        let fontLoaded = false;

        // ==== 래스터라이저 ====
        // [bug 1 수정] 슬라이더 크기를 그대로 쓴다. 이전에는 잉크를 실측해 셀(14×32)에
        // 들어올 때까지 0.92배씩 줄이는 자동 축소(fitSize/commonSize)가 있었는데,
        // 이 때문에 크기 슬라이더를 올려도 실제 렌더 크기가 변하지 않았다.
        // 이제는 사용자가 고른 크기를 그대로 적용한다 — 셀보다 잉크가 넓으면
        // 인접 글자와 겹치지만, 그것이 사용자의 의도적 선택으로 본다 (요구사항 명시).

        // ==== [여백 수정 v4] 잉크 박스 중앙 정렬 (순수 함수 — 테스트 추출 대상) ====
        // 원인: em 박스(textBaseline="middle") 중앙은 대문자 잉크 중앙과 다르다.
        //   잉크가 래스터 위로 쏠려 위 여백이 작아지고, 크기를 키우면 위쪽만 잘렸다.

        /**
         * 잉크 좌우 중앙이 래스터 중앙(RASTER_W/2)에 오도록 하는 fillText x.
         * actualBoundingBoxLeft/Right는 정렬점 기준 왼쪽/오른쪽 잉크 폭이다.
         * 글자별로 적용하므로 넓은 글자(W 등)는 옆 글자 위로 겹친다 (사용자 요구).
         */
        function glyphCenterX(m) {
            return RASTER_W / 2 + (m.actualBoundingBoxLeft - m.actualBoundingBoxRight) / 2;
        }

        /**
         * 전달된 metrics 집합의 잉크 **합집합**을 래스터 세로 중앙에 놓는 fillText y
         * (정렬점 절대 y). 기준 집합은 순수 함수가 정하지 않는다 —
         * 호출자가 정한다(현재: 대문자 A–Z, v6 PLAN §6.15).
         * 글자별 세로 중앙 정렬은 아포스트로피가 셀 한가운데 뜨는 문제가 있으므로
         * 폰트 단위로 한 번만 계산한다 — 글자들의 상대 위치(베이스라인)는 유지된다.
         * em 중앙(middle) 정렬점 기준 잉크는 [-ascent, +descent] 범위에 있으므로,
         * 정렬점 y = (래스터높이−합집합높이)/2 + ascent. **이 값이 곧 그릴 y다** —
         * 여기에 GLYPH_H/2를 더해선 안 된다 (v4 초판 회귀, PLAN §6.13e).
         * metrics 미지원 브라우저면 em 중앙을 돌려준다.
         */
        function fontBaselineY(metricsList) {
            let top = Infinity, bot = -Infinity;
            for (const m of metricsList) {
                top = Math.min(top, -m.actualBoundingBoxAscent);
                bot = Math.max(bot, m.actualBoundingBoxDescent);
            }
            if (!isFinite(top) || !isFinite(bot)) return GLYPH_H / 2;
            return Math.round((GLYPH_H - (bot - top)) / 2) - top;
        }

        /**
         * [여백 수정 v5 — PLAN §6.14] 글자 1개를 RASTER_W×GLYPH_H(48×64) 래스터로 렌더.
         * 배경을 채우지 않는다 — 잉크(흰색)만 알파로 남고 나머지는 투명하다.
         *   - 패킹: packPixels()가 알파 채널을 잉크로 본다 (불투명 배경과 같은 50% 임계값).
         *   - 미리보기: 불투명 래스터를 그리면 넓은 래스터(48px > 피치 14px)가
         *     **앞서 그린 글자의 잉크를 검은 사각형으로 지워버린다** ("AM"에서 M만
         *     보이던 원인). 투명 배경이 겹침 표시의 전제다.
         *  세로: 대문자(A–Z) 합집합 중앙 = 래스터 중앙 (v6 — computeGlyphBaselineFor)
         *  가로: 글자별 잉크 중앙 = 래스터 중앙 (glyphCenterX) */
        function renderGlyph(ch, size) {
            const c = document.createElement('canvas'); c.width = RASTER_W; c.height = GLYPH_H;
            const x = c.getContext('2d');
            x.fillStyle = "#fff";
            // size는 호출자가 currentGlyphSize()로 넘긴 슬라이더 값 그대로다.
            // 글자마다 크기를 다시 조정하면 글자 크기가 갈라지므로 하지 않는다.
            x.font = `${size}px ClockFont, sans-serif`;
            x.textAlign = "center"; x.textBaseline = "middle";
            const m = x.measureText(ch);
            x.fillText(ch, glyphCenterX(m), glyphBaselineY);
            return c;
        }

        /**
         * RASTER_W×GLYPH_H 픽셀 배열(RGBA) → 384바이트 비트맵 (6 bytes/row × 64 rows).
         * DOM을 건드리지 않는 순수 함수로 분리한 이유: test/js/packGlyph_test.mjs가
         * 이 함수를 그대로 뽑아 U8g2 규약과 대조하기 때문이다. (캔버스 로직과 섞으면 검증 불가)
         *
         * [여백 수정 v5] 잉크 판정은 **알파 채널** > 128 (면적 50% 초과)로 한다.
         * 래스터 배경이 투명해졌으므로(v5) 알파 = 잉크 커버리지 그 자체다 — 알파 채널은
         * 프리멀티플라이되지 않아 값이 정확하다. 이전(불투명 검은 배경 + 흰 잉크)의
         * R > 128 임계값과 정확히 같은 커버리지 기준이다 (R = 255×커버리지).
         *
         * 펌웨어 renderer.cpp의 drawBitmap(x + xOffset, yTop, bytesPerRow, glyphH, data)와
         * 정확히 대응한다 — row-major, MSB-first: 바이트 = x>>3, 비트 = 7-(x&7).
         */
        function packPixels(px) {
            const bm = new Uint8Array(BYTES_PER_ROW * GLYPH_H);   // 6 × 64 = 384
            for (let y = 0; y < GLYPH_H; y++) {
                for (let x = 0; x < RASTER_W; x++) {
                    if (px[(y * RASTER_W + x) * 4 + 3] > 128) {
                        bm[y * BYTES_PER_ROW + (x >> 3)] |= (1 << (7 - (x & 7)));
                    }
                }
            }
            return bm;
        }

        /** 캔버스를 384바이트 비트맵으로 변환한다 (DOM 접근은 여기만) */
        function packGlyph(canvas) {
            const px = canvas.getContext('2d').getImageData(0, 0, RASTER_W, GLYPH_H).data;
            return packPixels(px);
        }

        /**
         * [버그 수정] 미리보기 캐시를 **현재 폰트·현재 크기**로 통째로 다시 만든다.
         *
         * 기존 코드에서 bitmapCache는 processAll()(=업로드) 안에서만 채워졌다.
         * 그 결과 폰트를 새로 불러오거나 크기 슬라이더를 움직여도 미리보기는
         * 마지막 업로드 시점의 글리프를 계속 그렸다. (drawChar()가 캐시가 있으면
         * 캐시를, 없을 때만 라이브 폰트를 쓰기 때문에 드러나지 않았다)
         *
         * 순수 함수로 분리한 이유: test/js/preview_cache_test.mjs가 이 함수를 그대로
         * 뽑아 "크기가 바뀌면 캐시가 전부 새로 만들어지는가"를 검증한다.
         *
         * @param {string[]} chars  렌더링할 문자 목록
         * @param {(ch: string) => HTMLCanvasElement} renderFn 글자 1자를 캔버스로 그리는 함수
         * @returns {Object<string, HTMLCanvasElement>} 새 캐시 (이전 캐시는 건드리지 않는다)
         */
        function buildGlyphCache(chars, renderFn) {
            const cache = {};
            for (const ch of chars) cache[ch] = renderFn(ch);
            return cache;
        }

        // [제거 — 잉크 폭 측정 계열 (ENG §6.16)]
        //   ENG판은 잉크 폭을 재서 피치를 늘렸다(glyphInkWidth/maxInkWidthOf/inkOfChar).
        //   중국어판은 한자가 정사각형이고 피치가 32로 고정이라 잉크 폭을 잴 이유가 없다.
        //   펌웨어도 inkWidthOf()를 layout 경로에서 쓰지 않는다 (renderer.cpp: 주석 참조).

        /**
         * [bug 1 수정] 렌더링에 쓸 글자 크기 = 슬라이더 값 그대로.
         * 자동 축소(commonSize)가 있던 시절에는 이 값이 슬라이더와 무관하게 고정됐다.
         * 메모이제이션도 필요 없다 — 잴 것이 없기 때문이다.
         */
        function currentGlyphSize() {
            return parseInt(els.sIn.value);
        }

        /** 현재 선택된 표시 방식 (3번 항목 값) */
        function currentPresentation() {
            return parseInt(els.script.value);
        }

        /**
         * 표시 방식 → 펌웨어 저장 필드 script_type (0 간체 / 1 번체)
         * @param {number} p 표시 방식
         * @returns {number} script_type 값
         * @details 펌웨어 setPresentation()이 이 변환을 한다. 웹은 **미러**일 뿐이라
         *          두 벌이 어긋나면 미리보기와 기기가 다른 말을 하게 된다.
         *          숫자(數字)도 한 문자판을 물려받는다 — 한자로 돌아오면 되살아나야 하니까.
         */
        function scriptOfPresentation(p) {
            return p === PRESENTATION_TRADITIONAL ? 1 : 0;
        }

        /** 표시 방식이 한자 모드인가 (숫자면 false) — 펌웨어 isWord 미러 */
        function isWordPresentation(p) {
            return p !== PRESENTATION_NUMERIC;
        }

        /** 현재 표시 방식의 글자 목록 (CONFIG_FIELDS.presentation 연동) */
        function currentChars() {
            return charsForScript(scriptOfPresentation(currentPresentation()));
        }

        /** 폰트가 로드된 상태에서만 캐시를 갱신한다 (미로드면 라이브 폰트로 그린다)
         *  glyphBaselineY도 여기서만 계산한다 — 업로드(processAll)와 미리보기가
         *  반드시 같은 잉크 중앙 정렬 결과를 쓰게 하기 위해서다. */
        function refreshPreviewCache() {
            if (!fontLoaded) return;
            const size = currentGlyphSize();
            glyphBaselineY = computeGlyphBaselineFor(size);
            bitmapCache = buildGlyphCache(ALL_CHARS, ch => renderGlyph(ch, size));
        }

        /**
         * 한자 잉크 합집합을 래스터 세로 중앙에 놓는 기준을 측정한다 (브라우저 전용).
         *   시계 화면의 지배 글자는 한자다. 기준을 37자 전체(두 문자판 합집합)로 삼으면
         *   `一`(가로획 1줄)이나 `二`처럼 낮은 글자의 잉크가 합집합을 좁혀
         *   다른 한자가 래스터 중앙보다 아래로 처진다 ("글자가 약간 아래에 표시된다").
         *   ENG판이 대문자(A–Z)만으로 기준을 좁혔던 것과 같은 목적이지만,
         *   **CJK에는 그에 해당하는 "대문자"가 없다** — 한자는 표의 자형에 따라
         *   세로 범위가 모두 다르므로, 여기서는 문자판 전체를 기준으로 삼는다.
         *   기준을 좁힐 수 있는 묶음을 임의로 정하는 것보다, 넓게 잡은 뒤
         *   슬라이더 20~40px에서 잘림이 없는 편이 낫다 (검증 게이트에서 눈으로 확인).
         */
        function computeGlyphBaselineFor(size) {
            const c = document.createElement('canvas'); c.width = RASTER_W; c.height = GLYPH_H;
            const x = c.getContext('2d');
            x.font = `${size}px ClockFont, sans-serif`;
            x.textAlign = "center"; x.textBaseline = "middle";
            return fontBaselineY(ALL_CHARS.map(ch => x.measureText(ch)));
        }

        els.fIn.onchange = async (e) => {
            const file = e.target.files[0]; if (!file) return;
            const buffer = await file.arrayBuffer();
            const font = new FontFace("ClockFont", buffer);
            await font.load(); document.fonts.add(font);
            fontLoaded = true; els.apply.disabled = false;
            refreshPreviewCache();          // ← 새 폰트로 미리보기를 즉시 갱신
            els.status.innerText = "Font ready — check the preview.";
        };
        els.sIn.oninput = () => {
            document.getElementById('sVal').innerText = els.sIn.value;
            refreshPreviewCache();          // ← 크기를 바꾸면 미리보기도 즉시 갱신
        };

        // ==== 프리뷰 그리기 ====
        // [여백 수정 v5 — PLAN §6.14] 래스터는 줄 밴드 중앙에 놓인다 (펌웨어 rasterTopY와 1:1):
        //   그리기 상단 y = yOffset + (LINE_HEIGHT − GLYPH_H) / 2
        //   GLYPH_H=64 래스터는 밴드 위·아래로 16px씩 나간다 — 1줄 화면은 화면 전체를 쓰고
        //   (drawByLine의 1줄 규칙), 2줄 화면은 밴드로 잘라낸다. 잉크는 래스터 중앙에
        //   정렬돼 있으므로 밴드 중앙에 온다. 가로는 x − X_OFFSET (펌웨어 xOffset −17).

        /** 잉크 마스크(알파)를 유지한 채 색만 바꾼 캔버스. 반전 표시에 쓴다.
         *  [버그 수정] 이전 반전 경로(흰 배경을 채운 뒤 합성 모드로 잉크를 파는 방식)는
         *  래스터가 불투명하던 시절부터 배경째로 전부 지워져 아무것도 그리지 않았다.
         *  알파 마스크 + source-in 재생색으로 고친다. */
        function tintedGlyph(img, color) {
            const t = document.createElement('canvas'); t.width = RASTER_W; t.height = GLYPH_H;
            const tx = t.getContext('2d');
            tx.drawImage(img, 0, 0);
            tx.globalCompositeOperation = 'source-in';
            tx.fillStyle = color; tx.fillRect(0, 0, RASTER_W, GLYPH_H);
            return t;
        }

        function drawChar(ctx, ch, x, yOffset) {
            const img = bitmapCache[ch];
            const isInverted = els.invert.value === "1";
            const top = yOffset + (LINE_HEIGHT - GLYPH_H) / 2;
            if (img) {
                if (isInverted) ctx.drawImage(tintedGlyph(img, "#000"), x - X_OFFSET, top);
                else            ctx.drawImage(img, x - X_OFFSET, top);
            } else {
                // 폴백(라이브 폰트): 캐시 래스터와 같은 잉크 중앙 정렬을 흉내 낸다
                ctx.fillStyle = isInverted ? "#000" : "#fff";
                ctx.font = `${currentGlyphSize()}px ClockFont, sans-serif`;
                ctx.textAlign = "center"; ctx.textBaseline = "middle";
                const m = ctx.measureText(ch);
                ctx.fillText(ch, x - X_OFFSET + glyphCenterX(m), top + glyphBaselineY);
            }
        }

        /** 펌웨어 Renderer::drawScaledChar — 밴드 안에서 높이만 조절 (0 ↔ LINE_HEIGHT).
         *  래스터 중앙의 LINE_HEIGHT 창을 h 높이로 눌러 밴드 중앙에 그린다 (펌웨어 winTop 규칙). */
        function drawScaledChar(ctx, ch, x, baseY, h) {
            if (h <= 0) return;
            const img = bitmapCache[ch];
            const isInverted = els.invert.value === "1";
            if (!img) { drawChar(ctx, ch, x, baseY); return; }
            const src = isInverted ? tintedGlyph(img, "#000") : img;
            const top = baseY + (LINE_HEIGHT - h) / 2;
            ctx.drawImage(src, 0, (GLYPH_H - LINE_HEIGHT) / 2, RASTER_W, LINE_HEIGHT,
                          x - X_OFFSET, top, RASTER_W, h);
        }

        /** 펌웨어 Renderer::drawZoomedChar — 가로세로 동률 확대 */
        function drawZoomedChar(ctx, ch, x, baseY, scale) {
            if (scale <= 0) return;
            const img = bitmapCache[ch];
            const isInverted = els.invert.value === "1";
            const tw = RASTER_W * scale, th = GLYPH_H * scale;
            const left = x - X_OFFSET + (RASTER_W - tw) / 2, top = baseY + (LINE_HEIGHT - th) / 2;
            if (!img) {
                // 폴백(라이브 폰트): 밴드 중앙(= 잉크 중앙)을 기준으로 확대한다
                ctx.save();
                ctx.translate(x - X_OFFSET + RASTER_W / 2, baseY + LINE_HEIGHT / 2); ctx.scale(scale, scale);
                ctx.fillStyle = isInverted ? "#000" : "#fff";
                ctx.font = `${currentGlyphSize()}px ClockFont, sans-serif`;
                ctx.textAlign = "center"; ctx.textBaseline = "middle";
                const m = ctx.measureText(ch);
                // 로컬 y는 이동 중심(baseY + LINE_HEIGHT/2) 기준 상대값이다
                ctx.fillText(ch, glyphCenterX(m) - RASTER_W / 2,
                             glyphBaselineY - GLYPH_H / 2); ctx.restore();
                return;
            }
            const src = isInverted ? tintedGlyph(img, "#000") : img;
            ctx.drawImage(src, left, top, tw, th);
        }

        // UTF-8 변환기 — 한 번만 만들어 재사용한다 (layoutLine이 프레임마다 호출된다).
        const ENCODER = new TextEncoder();
        const DECODER = new TextDecoder('utf-8');

        /**
         * 펌웨어 layout_engine.cpp `layoutLine()`의 미러 — **1줄 고정 피치 배치**.
         *
         * [ENG판과 뭐가 다른가]
         *   ENG판은 어절을 2줄로 접고(linePitch) 잉크 폭에 따라 피치를 늘렸다(§6.16).
         *   중국어판은 표현이 최장 4자이고 한자가 정사각형이므로 둘 다 불필요하다:
         *     - 4 × 32 = 128 = 화면 폭 → 줄바꿈 없음
         *     - 피치 = 32 고정 → 잉크 폭을 잴 이유 없음
         *   그래서 어절·피치·잉크 계열을 옮기지 않고 **이 함수 하나만** 가져왔다.
         *
         * @param {string} text  배치할 문자열
         * @param {number} cap   셀 배열 용량 (펌웨어의 outCapacity)
         * @returns {{chars: Array<{c,x,y}>, ok: boolean}}
         *          ok=false면 펌웨어가 빈 화면을 그린다 (5자 이상 또는 용량 초과)
         *
         * 펌웨어 규칙 (layout_engine.cpp scanCells + layoutLine):
         *   1. 공백은 **어절 사이에 한 칸**, 앞/뒤 공백과 복수 공백은 버린다
         *   2. 깨진 바이트(잘린 멀티바이트·컨티뉴레이션·도중 NUL)는 **빈 칸 한 칸**
         *   3. 셀 수가 4를 넘으면 잘린다 — 반쪽 표현을 만들지 않는다
         *   4. 가로 중앙 정렬: startX = (128 − n×32) / 2 → 4자 0, 3자 16, 2자 32, 1자 48
         *   5. 세로는 항상 LINE_TOP_Y (1줄)
         *
         * @note **규칙 2는 여기서 항상 발동하지 않는다 (구조적 한계).**
         *   펌웨어는 raw 바이트열을 받으므로 깨진 입력이 실제로 가능하다.
         *   브라우저의 문자열은 이미 디코드된 상태라 깨진 바이트가 존재할 수 없다 —
         *   TextEncoder는 항상 올바른 UTF-8을 낸다. 아래의 broken 분기는
         *   "혹시 들어온 이상한 문자열"에 대한 방어로만 남는다.
         *   깨진 바이트 경로의 실제 검증은 펌웨어 쪽 전수 테스트(test_layout.cpp §7)가 담당한다.
         */
        function layoutLine(text, cap = MAX_PER_LINE) {
            const chars = [];
            const bytes = ENCODER.encode(text);   // UTF-8 바이트열 (Uint8Array)
            // 펌웨어 declaredLenOf()의 미러 — 리딩 바이트가 요구하는 전체 길이
            const declaredLen = b => (b < 0x80) ? 1 : (b < 0xC0) ? 0 : (b < 0xE0) ? 2
                                     : (b < 0xF0) ? 3 : (b < 0xF8) ? 4 : 0;
            const blank = () => chars.push({ c: ' ', x: 0, y: LINE_TOP_Y });
            let pendingBlank = false;

            for (let i = 0; i < bytes.length; ) {
                const b = bytes[i];
                if (b === 0x20) {                       // 공백 — 다음 글자가 올 때만 한 칸
                    if (chars.length > 0) pendingBlank = true;
                    i += 1;
                    continue;
                }
                if (b === 0x00) {                       // NUL — 도중이면 깨진 칸, 끝이면 종료
                    if (bytes.length - i === 1) break;
                    if (pendingBlank) { blank(); pendingBlank = false; }
                    blank(); i += 1;
                    continue;
                }
                const declared = declaredLen(b);
                const remaining = bytes.length - i;
                if (declared === 0) {                  // 컨티뉴레이션 — 1바이트만 소비
                    if (pendingBlank) { blank(); pendingBlank = false; }
                    blank(); i += 1;
                    continue;
                }
                if (declared > remaining) {            // 잘림 — 남은 바이트를 덩어리로 소비
                    if (pendingBlank) { blank(); pendingBlank = false; }
                    blank(); i += remaining;
                    continue;
                }
                // 정상 글자 — 코드포인트로 되돌린 뒤 다시 인코딩하는 경로는
                // 펌웨어와 다른 문자열을 낼 수 있으므로 바이트 구간을 그대로 디코드한다.
                // [주의] 반드시 subarray()를 쓴다 — Array의 slice()는 TypedArray가 아니라
                // TextDecoder.decode()가 "SharedArrayBuffer/ArrayBuffer/ArrayBufferView가
                // 아니다"며 예외를 던진다 (브라우저에서도 동일).
                const s = DECODER.decode(bytes.subarray(i, i + declared));
                if (pendingBlank) { blank(); pendingBlank = false; }
                chars.push({ c: s, x: 0, y: LINE_TOP_Y });
                i += declared;
            }

            if (chars.length > cap) return { chars: [], ok: false };       // 용량 초과
            if (chars.length > MAX_PER_LINE) return { chars: [], ok: false };  // 잘림
            const startX = (SCREEN_W - chars.length * GLYPH_W) / 2;
            for (let k = 0; k < chars.length; k++) chars[k].x = startX + k * GLYPH_W;
            return { chars, ok: true };
        }

        // ==== 시간 문자열 생성 — chinese_time_core.cpp 미러 ====
        // 규칙 근거는 PLAN.md §2.2, 정답 표는 §3이다. 아래 함수는 §3을 그대로 따른다.
        //   이 JS가 펌웨어와 어긋나면 브라우저 미리보기와 OLED가 다른 시간을 보여준다.
        const DIGITS = ["零", "一", "二", "三", "四", "五", "六", "七", "八", "九"];
        // 요일 0=일 ~ 6=토. 간체·번체가 같다.
        const WEEKDAY_CHARS = ["日", "一", "二", "三", "四", "五", "六"];

        /**
         * @brief 간체 글자를 문자판에 맞게 변환
         * @param {string} c  간체 1자
         * @param {number} s  0=간체 1=번체
         * @returns {string} 치환 대상이 아니면 원래 글자
         * @note 펌웨어 chinese_time_core.cpp convertChar()와 **같은 표**를 써야 한다.
         *       test/js/firmware_wiring_test.mjs가 두 표를 정적으로 대조한다.
         */
        function convertChar(c, s) {
            return (s === 1) ? (TRAD_MAP[c] || c) : c;
        }

        /** 펌웨어 buildNumber()의 미러 — 0~59를 한자 수사로 (십의 자리 + 十 + 일의 자리) */
        function buildNumber(n) {
            if (n < 0 || n > 59) return "";
            if (n < 10) return DIGITS[n];
            const tens = Math.floor(n / 10), ones = n % 10;
            // 십의 자리가 1이면 "一"을 생략한다 — 15는 "十五"이지 "一十五"가 아니다.
            return (tens === 1 ? "" : DIGITS[tens]) + "十" + (ones === 0 ? "" : DIGITS[ones]);
        }

        /** chtime::hourToChars — "三点" / "两點" / "二十三点" */
        function hourToChars(hour, is24h, s) {
            if (hour < 0 || hour > 23) return "";
            let h = hour;
            if (!is24h) { h = hour % 12; if (h === 0) h = 12; }
            // 규칙 ②: **단독 2**는 `二`가 아니라 `两`다 (PLAN §3.1·§3.2).
            //   20~23의 십의 자리는 `二`가 유지된다 ("二十三点").
            const numeral = (h === 2) ? convertChar("两", s) : buildNumber(h);
            if (!numeral) return "";
            return numeral + convertChar("点", s);
        }

        /** chtime::msToChars — 분/초 공통 (접미 글자만 다르다) */
        function msToChars(v, suffix) {
            if (v < 0 || v > 59) return "";
            // 규칙 ⑤ 정각은 "整". 규칙 ④ 30분은 정확히 "半"만 쓴다 (31분은 "三十一分").
            if (v === 0) return "整";
            if (v === 30) return "半";
            // 규칙 ③: 10분 미만은 십의 자리에 `零`을 넣는다. "三点五分"은 3:5와 3:35로 오독된다.
            const lead = (v < 10) ? "零" : "";
            return lead + buildNumber(v) + suffix;
        }

        /** chtime::weekdayToChars — "星期三" */
        function weekdayToChars(day) {
            if (day < 0 || day >= WEEKDAY_CHARS.length) return "";
            return "星期" + WEEKDAY_CHARS[day];
        }

        /** chtime::dayPartToChars — "上午"(0~11) / "下午"(12~23). 자정도 上午이다. */
        function dayPartToChars(hour24) {
            if (hour24 < 0 || hour24 > 23) return "";
            return (hour24 < 12) ? "上午" : "下午";
        }

        /** chtime::twoDigit — "%02d" */
        function twoDigit(n) {
            if (n < 0 || n > 59) return "";
            return n.toString().padStart(2, '0');
        }

        /**
         * 숫자 모드의 단위 붙이기 — chinese_time.cpp withUnit()의 미러
         * @param {string} digits 두 자리 숫자
         * @param {string} simp   간체 단위 ("时")
         * @param {string|null} trad 번체 단위 ("時") — 두 문자판이 같으면 null
         * @details 접미 사이의 간격 한 칸이 ENG판 "02 H" 규칙의 승계다.
         *   레이아웃 엔진이 이 공백을 빈 셀 한 칸으로 소비한다 (PLAN §6.4).
         *           2자리 숫자(2셀) + 공백(1셀) + 단위(1셀) = 4셀 = 화면 폭 **정확히**.
         */
        function withUnit(digits, simp, trad, s) {
            if (!digits) return "";
            return digits + " " + ((s === 1 && trad) ? trad : simp);
        }

        /** Chinese_Clock.ino handleClockUpdate() 미러 */
        function getChineseTimeStrings() {
            const now = new Date();
            const h = now.getHours(), m = now.getMinutes(), s = now.getSeconds();
            const d = now.getDay();                       // 0=Sunday — tm_wday와 동일 규약
            const isWord = isWordPresentation(currentPresentation()), is24H = els.hour.value === "1";
            const sc = scriptOfPresentation(currentPresentation());

            // 화면0: 24H는 요일("星期三"), 12H는 오전/오후("上午"/"下午")
            const s0 = is24H ? weekdayToChars(d) : dayPartToChars(h);

            // 숫자 모드는 단위까지 붙은 4셀로 간다 ("13 时").
            //   [PLAN §6.10 정정] 원고는 getNumericHour()에 " 时"를 덧붙였으나,
            //   withUnit()가 이미 붙이므로 "13 时 时"이 되어 6셀 → 화면이 통째로 빈칸이 된다.
            const s1 = isWord ? hourToChars(h, is24H, sc)
                              : withUnit(twoDigit(is24H ? h : (h % 12 === 0 ? 12 : h % 12)),
                                         "时", "時", sc);
            const s2 = isWord ? msToChars(m, "分")
                              : withUnit(twoDigit(m), "分", null, sc);
            let   s3 = isWord ? msToChars(s, "秒")
                              : withUnit(twoDigit(s), "秒", null, sc);

            // 정각 규칙: 분=0 & 초=0 이면 분 화면이 整이므로 Screen 4를 비운다
            if (isWord && m === 0 && s === 0) s3 = "";

            return [s0, s1, s2, s3];
        }

        let lastTimeStrings = ["", "", "", ""], targetTimeStrings = ["", "", "", ""], animStep = 16;

        function drawChimeIcon(ctx) {
            if (els.chime.value !== "1") return;
            const bell = [0x18, 0x3C, 0x3C, 0x3C, 0xFF, 0xDB, 0x18, 0x00];
            ctx.fillStyle = els.invert.value === "1" ? "#000" : "#fff";
            for (let y = 0; y < 8; y++) for (let x = 0; x < 8; x++) if (bell[y] & (1 << (7 - x))) ctx.fillRect(x, y, 1, 1);
        }

        /**
         * 펌웨어 Renderer::drawDitheredChar()의 미러 — Bayer 4×4 행렬로 density만큼 드러낸다.
         * @param {CanvasRenderingContext2D} ctx 대상 캔버스
         * @param {string} ch   글자
         * @param {number} x    셀 왼쪽 좌표
         * @param {number} y    래스터 상단 Y
         * @param {number} density 0(투명) ~ 16(불투명)
         * @details [ENG판 차이] ENG 미리보기는 페이드를 globalAlpha로 흉내 냈다.
         *   펌웨어는 **Bayer 디더**로 픽셀을 골라 칠한다 — OLED에는 알파가 없어서.
         *   두 방식은 중간 프레임이 완전히 다르므로, 미리보기가 페이드를 보여주면
         *   기기와 "같은 애니메이션"이라는 보장이 없다. 여기는 펌웨어 행렬을 그대로 쓴다.
         *   (row_mask 재사용 최적화는 캔버스에서 무의미하므로 생략)
         */
        const BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]];

        function drawDitheredChar(ctx, ch, x, y, density) {
            if (density <= 0) return;
            if (density >= 16) { drawChar(ctx, ch, x, y); return; }
            const img = bitmapCache[ch];
            if (!img) { drawChar(ctx, ch, x, y); return; }   // 캐시 없음 = 원래 그리기 (펌웨어와 동일)

            const src = els.invert.value === "1" ? tintedGlyph(img, "#000") : img;
            const w = img.width, h = img.height;
            const sx = x - X_OFFSET;            // 펌웨어 bx = x + xOffset = x − X_OFFSET
            const px = src.getContext('2d').getImageData(0, 0, w, h).data;
            ctx.fillStyle = els.invert.value === "1" ? "#000" : "#fff";
            for (let r = 0; r < h; r++) {
                for (let c = 0; c < w; c++) {
                    // 잉크가 아니면 건너뛴다 (알파 0)
                    if (px[(r * w + c) * 4 + 3] === 0) continue;
                    // 펌웨어와 같은 위상: x 좌표는 **절대** 화면 좌표로 계산한다.
                    const gx = sx + c;
                    if (BAYER[((r % 4) + 4) % 4][(((gx % 4) + 4) % 4)] < density) {
                        ctx.fillRect(gx, y + r, 1, 1);
                    }
                }
            }
        }

        /**
         * 펌웨어 display_manager.cpp의 1줄 그리기 규칙 — **클립이 필요 없다**.
         *
         * ENG판은 2줄이라 줄 밴드로 잘라야 했고(ctx.clip()), 그 대필로 펌웨어에
         * drawSingleCharClipped()가 필요했다. 중국어판은 1줄이라 밴드 = 화면 전체다.
         * 화면 밖으로 나간 부분은 U8g2가 디스플레이 버퍼에서 자른다.
         *   → 여기선 그냥 순서대로 그리고 끝낸다. (display_manager.cpp bandFor() 제거와 동일)
         *
         * @param {CanvasRenderingContext2D} ctx 대상 캔버스
         * @param {Array} chars layoutLine() 출력 — x, y가 이미 정해져 있다
         * @param {(d:Object)=>void} drawOne 글자 하나를 그리는 동작
         */
        function drawByLine(ctx, chars, drawOne) {
            for (const d of chars) drawOne(d);
        }

        /** 펌웨어 DisplayManager::isTitleScreenOf() 미러 — 뒤집기 여부에 따라 벨 아이콘 위치가 바뀐다 */
        function isTitleScreenOf(idx) {
            return els.flip.value === "1" ? idx === 3 : idx === 0;
        }

        function render() {
            const currentTimeStrings = getChineseTimeStrings();
            if (targetTimeStrings[0] === "") targetTimeStrings = [...currentTimeStrings];
            let changed = currentTimeStrings.some((s, i) => s !== targetTimeStrings[i]);
            if (changed && animStep >= ANIM_STEPS) {
                lastTimeStrings = [...targetTimeStrings]; targetTimeStrings = [...currentTimeStrings];
                animStep = (["1", "2", "3", "4", "5"].includes(els.anim.value)) ? 0 : ANIM_STEPS;
            }
            if (animStep < ANIM_STEPS) animStep++;

            // 한 줄 높이(=48)를 온전히 이동한다 — 펌웨어 drawAnimPair()의 off와 같은 식이다.
            const mode = els.anim.value, off = animStep * (LINE_HEIGHT / ANIM_STEPS);
            for (let s = 0; s < 4; s++) {
                const ctx = pCtx[s], isInverted = els.invert.value === "1";
                ctx.fillStyle = isInverted ? "#fff" : "#000"; ctx.fillRect(0, 0, SCREEN_W, SCREEN_H);

                const curD = layoutLine(targetTimeStrings[s]).chars;
                if (animStep >= ANIM_STEPS || mode === "0") {
                    drawByLine(ctx, curD, d => drawChar(ctx, d.c, d.x, d.y));
                } else {
                    const oldD = layoutLine(lastTimeStrings[s]).chars;
                    // 문자 정체성 = x. 1줄이라 줄 번호는 필요 없다 (ENG판의 line 필드 제거).
                    drawByLine(ctx, curD, nd => {
                        const baseY = nd.y;
                        const od = oldD.find(o => o.x === nd.x);
                        if (od && od.c === nd.c) { drawChar(ctx, nd.c, nd.x, baseY); return; }
                        switch (mode) {
                            case "1": if (od) drawChar(ctx, od.c, nd.x, baseY - off);
                                      drawChar(ctx, nd.c, nd.x, baseY + LINE_HEIGHT - off); break;
                            case "2": if (od) drawChar(ctx, od.c, nd.x, baseY + off);
                                      drawChar(ctx, nd.c, nd.x, baseY - LINE_HEIGHT + off); break;
                            case "3": if (animStep <= 8) { if (od) drawScaledChar(ctx, od.c, nd.x, baseY, ((8 - animStep) * LINE_HEIGHT) / 8); }
                                      else { drawScaledChar(ctx, nd.c, nd.x, baseY, ((animStep - 8) * LINE_HEIGHT) / 8); } break;
                            // 디더 — 펌웨어는 density를 0..16으로 준다 (|ENG globalAlpha 미러 아님)
                            case "4": if (animStep <= 8) { if (od) drawDitheredChar(ctx, od.c, nd.x, baseY, 16 - (animStep * 2)); }
                                      else { drawDitheredChar(ctx, nd.c, nd.x, baseY, (animStep - 8) * 2); } break;
                            case "5": if (animStep <= 8) { if (od) drawZoomedChar(ctx, od.c, nd.x, baseY, (8 - animStep) / 8); }
                                      else { const sc = (animStep <= 12) ? ((animStep - 8) * 150 / 100 / 4) : (1.5 - (animStep - 12) * 50 / 100 / 4);
                                             drawZoomedChar(ctx, nd.c, nd.x, baseY, sc); } break;
                        }
                    });
                    // 펌웨어 drawAnimExit() 미러 — 새 텍스트에 같은 위치가 없어 사라진 셀을 퇴장시킨다
                    drawByLine(ctx, oldD, od => {
                        if (curD.find(nd => nd.x === od.x)) return;
                        const baseY = od.y;
                        switch (mode) {
                            case "1": drawChar(ctx, od.c, od.x, baseY - off); break;
                            case "2": drawChar(ctx, od.c, od.x, baseY + off); break;
                            case "3": if (animStep <= 8) drawScaledChar(ctx, od.c, od.x, baseY, ((8 - animStep) * LINE_HEIGHT) / 8); break;
                            case "4": if (animStep <= 8) drawDitheredChar(ctx, od.c, od.x, baseY, 16 - (animStep * 2)); break;
                            case "5": if (animStep <= 8) drawZoomedChar(ctx, od.c, od.x, baseY, (8 - animStep) / 8); break;
                        }
                    });
                }
                if (isTitleScreenOf(s) && els.chime.value === "1") drawChimeIcon(ctx);
            }
            requestAnimationFrame(render);
        }
        render();
        async function processAll() {
            els.apply.disabled = true; els.pWrap.style.display = "block";
            const chars = ALL_CHARS;
            // [결함 수정 2026-10-05] 업로드 대상 슬롯을 **한 번만 읽어서 고정**한다.
            //   아래 37회의 fetch 사이에 사용자가 슬롯을 바꾸거나 저장 POST가 늦으면
            //   els.slot.value가 이리저리 흔들린다. 글리프는 그때그때의 값으로 보내면
            //   도착점이 달라지고, 이름은 펌웨어가 가진 font_slot(=첫 시점 값)에 기록되어
            //   **글리프는 /f1에, 이름표는 /f0에** 남는다 → "업로드한 이름이 안 보인다".
            const targetSlot = els.slot.value;

            // [2026-10-05] 폰트 이름도 **같은 이유로 한 번만 읽어 고정**하고,
            //   글리프를 보내는 **같은 요청**에 실어 보낸다(?font=).
            //   예전엔 업로드가 끝난 뒤 별도의 /api/config POST로만 이름을 보냈고
            //   그마저 font 파일이 선택된 경우에만 나갔다. 그 요청이 빠지면 슬롯에는
            //   글리프 37개만 남고 이름표(/fN/name.txt)가 없어 드롭다운이
            //   "Empty Slot"으로 보였다 (사용자 보고: 폰트를 넣었는데 비어 보인다).
            //   이름이 슬롯과 함께 도착하므로 펌웨어가 어느 폴더에 쓸지 헷갈릴 수 없다.
            const fontName = els.fIn.files[0] ? els.fIn.files[0].name : '';
            if (!fontName) {
                // 이름 없이 올리면 이 결함이 그대로 재발한다 — 조용히 진행하지 않는다.
                els.status.innerText = "⚠ 1. Font file 을 먼저 선택하세요 — 슬롯 이름표가 그 파일명으로 저장됩니다.";
                els.apply.disabled = false; els.pWrap.style.display = "none";
                return;
            }
            chars.forEach(c => { const b = document.getElementById('b_' + c); if (b) b.classList.remove('active'); });

            // 업로드와 미리보기가 **같은 렌더링 결과**를 써야 한다. 캐시를 먼저 통째로
            // 만들어 두고, 아래 루프는 그 캔버스를 그대로 패킹한다 (두 번 그리면 다를 수 있다).
            refreshPreviewCache();
            const size = currentGlyphSize();
            for (let i = 0; i < chars.length; i++) {
                const ch = chars[i];
                els.status.innerText = `Uploading ${ch} (${i + 1}/${chars.length})`;
                const canvas = bitmapCache[ch] || renderGlyph(ch, size);
                const bm = packGlyph(canvas);
                if (bm.length !== GLYPH_SIZE) {
                    els.status.innerText = `Glyph ${ch} packed ${bm.length}B, expected ${GLYPH_SIZE}B`;
                    els.apply.disabled = false; els.pWrap.style.display = "none";
                    return;   // 크기가 틀리면 펌웨어가 조용히 버린다 — 조용히 실패시키지 않는다
                }

                let hex = "";
                new TextEncoder().encode(ch).forEach(b => hex += b.toString(16).toUpperCase().padStart(2, '0'));
                const fd = new FormData(); fd.append('file', new Blob([bm]), `c_${hex}.bin`);

                const badge = document.getElementById('b_' + ch);
                if (badge) badge.classList.add('active');
                // ⚠ name 은 **encodeURIComponent** — 중문 파일명은 UTF-8 다바이트라
                //   그대로 넣으면 쿼리 문자열이 깨진다. 펌웨어 server.arg()가 디코드한다.
                await fetch(`/upload?slot=${targetSlot}&font=${encodeURIComponent(fontName)}`,
                            { method: 'POST', body: fd });
                els.pFill.style.width = ((i + 1) / chars.length * 100) + "%";
            }
            await fetch('/api/refresh_cache', { method: 'POST' });
            // [2026-10-05] 이름표는 위 업로드가 **이미 그 슬롯에** 썼다. 이 POST는 이제
            //   이름을 위한 것이 아니라 **장치의 현재 슬롯을 업로드한 슬롯으로 옮기기**
            //   위한 것이다(그래야 올린 폰트가 바로 화면에 뜬다). font_name도 함께 보내
            //   두지만, 업로드가 실패했더라도 여기서 살아남는 이중 안전장치일 뿐이다 —
            //   이 POST에 기대면 예전처럼 "조용히 빈 슬롯"이 재발한다.
            //   ⚠ 슬롯을 명시하는 이유: 펌웨어 applyFontName은 configManager.font_slot에
            //     쓰므로, 슬롯이 본문에 없으면 옛 슬롯에 기록될 수 있다.
            //     handleSetConfig가 applyIntSettings를 applyFontName보다 먼저 부르는
            //     순서가 그 계약을 받친다(순서를 바꾸면 재도입된다).
            await fetch('/api/config', {
                method: 'POST',
                body: JSON.stringify({ font_slot: parseInt(targetSlot), font_name: fontName })
            });
            els.status.innerText = `All glyphs uploaded! (${chars.length} glyphs, ${chars.length * GLYPH_SIZE} bytes)`;
            els.apply.disabled = false;
        }
    </script>
</body>
</html>
)rawliteral";

#endif