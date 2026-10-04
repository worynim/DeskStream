// worynim@gmail.com
/**
 * @file web_pages.h
 * @brief 웹 설정 대시보드 및 폰트 스튜디오 리소스 (영어판)
 * @details HTML, CSS, JavaScript 등으로 구성된 임베디드 웹 페이지 리소스 관리 (PROGMEM 활용)
 * @note [SYNC] 원본: Hangeul_Clock/web_pages.h — UI 전량 영문화 + 48×32/192바이트 래스터라이저.
 *
 * @warning JS 로직은 펌웨어를 **행 단위로 미러링**한다. 아래 상수와 함수를 대응시키지 않으면
 *          미리보기와 실제 OLED 출력이 어긋난다.
 *
 *   JS                          펌웨어 (반드시 같이 수정)
 *   -------------------------   ------------------------------------------
 *   RASTER_W/BYTES_PER_ROW/...  renderer_geometry.cpp GEOM_384B (새 업로드 형식, 48×64)
 *   GLYPH_W/GLYPH_H/MAX_PER_LINE  renderer_geometry.cpp GEOM_64B·defaultGeometry()
 *   layoutWrap()                layout_engine.cpp     layoutWrap()
 *   numberToWords() 등          english_time_core.cpp engtime::*
 *   dateString()                english_time_core.cpp engtime::dateString
 *   getEnglishTimeStrings()     ENG_Clock.ino         handleClockUpdate()
 *   MAX_PER_LINE               renderer_geometry.h  maxPerLine
 */
#ifndef WEB_PAGES_H
#define WEB_PAGES_H

#include <pgmspace.h>

// === 영어판 Font Studio HTML ===
const char font_studio_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Font Studio v2 — English Clock</title>
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
        <h1>English Clock Font Studio</h1>
        <p class="desc">DeskStream Project &middot; Current font: <span id="curFont" style="color:var(--primary)">-</span></p>

        <div class="setup-grid">
            <div class="field">
                <label>1. Font file (.ttf, .otf)</label>
                <input type="file" id="fIn" accept=".ttf,.otf">
            </div>
            <div class="field">
                <label>2. Font size: <span id="sVal">26</span>px <span style="color:#666">(used as-is — may overlap)</span></label>
                <input type="range" id="sIn" min="12" max="48" value="26">
            </div>
            <div class="field">
                <label>3. Storage slot (0&ndash;4)</label>
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
                <label>4. Animation (BTN3 short)</label>
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
                <label>5. Display type (BTN2 short)</label>
                <select id="displayMode">
                    <option value="0">Words (TWELVE / THIRTY)</option>
                    <option value="1">Numeric (12 / 30)</option>
                </select>
            </div>
            <div class="field">
                <label>6. Hour format (BTN2 long)</label>
                <select id="hourFormat">
                    <option value="0">12-hour (AM / PM)</option>
                    <option value="1">24-hour (0&ndash;23)</option>
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
            <div class="field">
                <label>11. Date order (24-hour screen 1)</label>
                <select id="dateOrder">
                    <option value="0">Day / Month &mdash; 2/10</option>
                    <option value="1">Month / Day &mdash; 10/2</option>
                </select>
            </div>
        </div>

        <div class="setup-grid">
            <div class="field">
                <label>12. Timezone (DST is applied automatically)</label>
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

        <button class="btn-apply" id="apply" onclick="processAll()" disabled>Upload Font Set (38 glyphs)</button>

        <div class="inventory" id="inv"></div>
        <div class="footer">
            <a href="https://fonts.google.com/" target="_blank">Free fonts (Google Fonts)</a>
        </div>
    </div>

    <script>
        // ==== 펌웨어 기하 상수와 1:1 대응 (renderer_geometry.cpp GEOM_384B) ====
        // 하나당 한 줄: 테스트 하네스가 이 상수들을 심볼 단위로 뽑아 대조하기 때문이다.
        const GLYPH_W = 14;   // 피치(셀 간격) — 레이아웃은 이 값만 쓴다 (GEOM_384B glyphW)
        const GLYPH_H = 64;   // 래스터 높이 — 잉크 64px까지 수용 (GEOM_384B glyphH)
        const RASTER_W = 48;  // 래스터 폭 — 잉크가 여기 중앙에 놓인다 (GEOM_384B drawW)
        const BYTES_PER_ROW = 6;   // 48px → 6B/행 × 64행 = 384B/글자 (GEOM_384B bytesPerRow)
        const MAX_PER_LINE = 9;
        const LINE_HEIGHT = 32;
        const SCREEN_W = 128;
        const SCREEN_H = 64;
        // [사용자 지정 사다리] n 글자 줄의 총 폭을 (n+1) 글자분으로 맞춘다.
        //   layout_engine.h linePitch()의 미러 — 규칙은 저기에만 적혀 있다.
        const PITCH_SUM_OFFSET = 1;
        // 래스터가 피치보다 넓은 만큼의 보정 — 그릴 때 x − X_OFFSET 이고,
        // 펌웨어 xOffset(−17) = −X_OFFSET 이다. 넘친 잉크는 옆 글자 위로 겹쳐 그려진다 (사용자 요구).
        const X_OFFSET = (RASTER_W - GLYPH_W) / 2;

        // A-Z + 0-9 + 아포스트로피 + 슬래시. 38글자 × 384B = 14,592 B
        // ('/'는 [수정할 사항 3]의 날짜 "2/10"에 필요하다)
        // 공백은 넣지 않는다 — layoutWrap()이 어절 사이 공백을 그리지 않는다.
        const UNIQ_CHARS = ("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'/").split("");

        let bitmapCache = {};
        // [여백 수정 v4] 대문자(A–Z) 잉크 합집합을 래스터 세로 중앙에 놓기 위한 fillText y
        // (정렬점 절대 y). 기준 잉크를 38자 전체에서 대문자로 좁힌 것은 v6 (PLAN §6.15) —
        // 숫자 디센더가 끼면 대문자가 위로 치우친다. refreshPreviewCache()가 폰트·크기마다
        // 다시 계산한다 (폰트 미로드 시 em 중앙).
        // 주의: "이동량(shift)"이 아니라 **그릴 y 값 자체**다 — v4 초판에서 이 둘을
        // 혼동해 래스터 중앙을 다시 더해 잉크가 밀리는 회귀가 있었다 (PLAN §6.13e).
        let glyphBaselineY = GLYPH_H / 2;
        // [간격 확장 §6.16] 현재 폰트의 최대 잉크 폭. layoutWrap()가 이를 읽어
        // 글자가 적은 줄의 피치를 넓힌다 (펌웨어 Renderer::maxInkWidth 미러).
        // refreshPreviewCache()가 폰트·크기마다 다시 잰다. 폰트 미로드 시 0 = 확장 없음.
        let previewInkWidth = 0;
        const els = {
            anim: document.getElementById('animMode'),
            disp: document.getElementById('displayMode'),
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
            dateOrder: document.getElementById('dateOrder'),
            tzSel: document.getElementById('tzSel'),
            tzCustom: document.getElementById('tzCustom'),
            tzHint: document.getElementById('tzHint')
        };

        UNIQ_CHARS.forEach(c => {
            const d = document.createElement('div');
            d.className = 'badge'; d.id = 'b_' + c; d.innerText = c;
            document.getElementById('inv').appendChild(d);
        });

        /**
         * [버그 2 수정] 설정 필드 한 장의 정의.
         * 폴링(fetchConfig)과 저장(saveConfig)이 **같은 표**를 보게 만들어
         * 한쪽만 고쳐지는 사고(설정을 바꿨는데 반영이 안 됨)를 구조적으로 막는다.
         * key = 펌웨어 web_manager.cpp 가 파싱하는 JSON 키, el = els 의 키.
         */
        const CONFIG_FIELDS = [
            { key: 'anim_mode',     el: 'anim',      def: 1 },
            { key: 'display_mode',  el: 'disp',      def: 0 },
            { key: 'hour_format',   el: 'hour',      def: 0 },
            { key: 'chime_enabled', el: 'chime',     def: false, isBool: true },
            { key: 'is_flipped',    el: 'flip',      def: false, isBool: true },
            { key: 'is_inverted',   el: 'invert',    def: false, isBool: true },
            { key: 'font_slot',     el: 'slot',      def: 0 },
            { key: 'brightness',    el: 'brIn',      def: 1 },
            { key: 'date_order',    el: 'dateOrder', def: 0 },
        ];

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
        CONFIG_FIELDS.forEach(f => { els[f.el].onchange = saveConfig; });
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
         * @param {(ch: string) => HTMLCanvasElement} renderFn 글자 1개를 캔버스로 그리는 함수
         * @returns {Object<string, HTMLCanvasElement>} 새 캐시 (이전 캐시는 건드리지 않는다)
         */
        function buildGlyphCache(chars, renderFn) {
            const cache = {};
            for (const ch of chars) cache[ch] = renderFn(ch);
            return cache;
        }

        /**
         * [간격 확장 §6.16] 글리프 캔버스 1장의 실제 잉크 폭을 잰다.
         * 펌웨어 renderer_geometry.h `inkWidthOf()`의 미러다.
         * 잉크 판정은 알파 > 128 — packPixels()의 업로드 임계값과 같은 규약이라
         * "화면에서 보이는 폭"과 "디바이스로 올라가는 폭"이 어긋나지 않는다.
         *
         * [웹-기기 간격 불일치] 아래 구현은 **캔버스가 아니라 업로드할 바이트**를 잰다.
         * @param {HTMLCanvasElement} canvas RASTER_W × GLYPH_H 래스터
         * @returns {number} 잉크 열 수. 빈 글리프라면 0
         */
        function glyphInkWidth(canvas) {
            // [웹-기기 간격 불일치 수정] **캔버스가 아니라 업로드할 바이트를** 잰다.
            // 펌웨어는 LittleFS에 올라간 384바이트에서 잉크를 잰다(inkWidthOf).
            // 캔버스 알파를 다시 읽는 경로에는 premultiplied alpha나 GPU 반올림이
            // 끼어 1~2px 어긋나고, 그게 간격으로 드러난다("웹은 괜찮은데 기기만 더 벌어진다").
            // 펌웨어와 같은 바이트를 재야 두 값이 정의상 같아진다.
            const bm = packGlyph(canvas);
            let first = -1, last = -1;
            for (let y = 0; y < GLYPH_H; y++) {
                for (let b = 0; b < BYTES_PER_ROW; b++) {
                    const byte = bm[y * BYTES_PER_ROW + b];
                    if (byte === 0) continue;
                    for (let p = 0; p < 8; p++) {
                        if (!(byte & (1 << (7 - p)))) continue;   // MSB 우선 — inkWidthOf 규약
                        const col = b * 8 + p;
                        if (first < 0 || col < first) first = col;
                        if (col > last) last = col;
                    }
                }
            }
            return first < 0 ? 0 : last - first + 1;
        }

        /**
         * [간격 확장 §6.16] 캐시된 폰트 전체의 최대 잉크 폭.
         * RASTER_W는 어떤 크기의 폰트든 48로 같으므로, 파일 크기만으로는
         * 폰트 크기를 알 수 없다 — 실제로 그려지는 폭을 재야 피치를 정할 수 있다.
         * @param {Object<string, HTMLCanvasElement>} cache buildGlyphCache() 결과
         * @returns {number} 최대 잉크 폭 (픽셀)
         */
        function maxInkWidthOf(cache) {
            let max = 0;
            for (const ch of Object.keys(cache)) {
                const w = glyphInkWidth(cache[ch]);
                if (w > max) max = w;
            }
            return max;
        }

        /**
         * [§6.16b] layout_engine.h `InkWidthFn`의 JS 미러 — 글자 하나 → 잉크 폭.
         * 펌웨어가 캐시 표(Renderer::inkOf)를 조회하는 것과 같은 자리다.
         * previewInkByChar가 채워지면 measureLineInk()가 이 함수를 쓴다.
         */
        let previewInkByChar = {};
        function inkOfChar(ch) {
            const w = previewInkByChar[ch];
            return (typeof w === 'number') ? w : previewInkWidth;
        }

        /**
         * [bug 1 수정] 렌더링에 쓸 글자 크기 = 슬라이더 값 그대로.
         * 자동 축소(commonSize)가 있던 시절에는 이 값이 슬라이더와 무관하게 고정됐다.
         * 메모이제이션도 필요 없다 — 잴 것이 없기 때문이다.
         */
        function currentGlyphSize() {
            return parseInt(els.sIn.value);
        }

        /** 폰트가 로드된 상태에서만 캐시를 갱신한다 (미로드면 라이브 폰트로 그린다)
         *  [여백 수정 v4] glyphBaselineY도 여기서만 계산한다 — 업로드(processAll)와
         *  미리보기가 반드시 같은 잉크 중앙 정렬 결과를 쓰게 하기 위해서다. */
        function refreshPreviewCache() {
            if (!fontLoaded) return;
            const size = currentGlyphSize();
            glyphBaselineY = computeGlyphBaselineFor(size);
            bitmapCache = buildGlyphCache(UNIQ_CHARS, ch => renderGlyph(ch, size));
            previewInkWidth = maxInkWidthOf(bitmapCache);   // §6.16 — 간격 확장의 상한
            // [§6.16b] 글자별 잉크 — 줄마다 다른 상한을 걸기 위해 필요하다.
            //   펌웨어의 Renderer::inkByChar와 같은 역할이다.
            previewInkByChar = {};
            for (const ch of UNIQ_CHARS) previewInkByChar[ch] = glyphInkWidth(bitmapCache[ch]);
        }

        /**
         * [여백 수정 v6 — PLAN §6.15] 대문자(A–Z) 잉크 합집합을 래스터 세로 중앙에
         * 놓는 기준을 측정한다 (브라우저 전용).
         *   시계 화면의 지배 잉크는 대문자다. 기준을 38자 전체 합집합으로 삼으면
         *   숫자(3·5·7·9·J·Q)의 아래 잉크나 슬래시의 디센더가 합집합에 끼어
         *   베이스라인이 아래로 내려가고, 대문자가 래스터 중앙보다 위에 놓인다
         *   ("글자가 약간 위에 표시된다"). 이 치우침은 폰트 메트릭에 좌우되므로
         *   어떤 폰트든 대문자만 중앙에 오도록 기준을 좁혔다.
         *   숫자 디센더·아포스트로피·슬래시는 같은 베이스라인을 유지한 채 넘친다 —
         *   1줄 화면(64px 전체)에서는 온전히 보이고, 2줄 화면(32px 밴드)에서는
         *   극단적인 부분만 잘린다.
         */
        function computeGlyphBaselineFor(size) {
            const c = document.createElement('canvas'); c.width = RASTER_W; c.height = GLYPH_H;
            const x = c.getContext('2d');
            x.font = `${size}px ClockFont, sans-serif`;
            x.textAlign = "center"; x.textBaseline = "middle";
            const caps = UNIQ_CHARS.filter(ch => ch >= "A" && ch <= "Z");
            return fontBaselineY(caps.map(ch => x.measureText(ch)));
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

        /**
         * [간격 확장 §6.16] 한 줄의 가로 피치 — layout_engine.cpp `linePitch()`의 미러.
         * @param {number} charCount 이 줄의 셀 수 (공백 셀 포함)
         * @param {number} inkWidth 폰트 최대 잉크 폭 (0이면 확장 없음)
         * @returns {number} 셀 간격. 항상 GLYPH_W 이상
         *
         * 9자면 126/9 = GLYPH_W가 되어 **기존과 완전히 같아진다.**
         * 글자가 적을수록 넓어지되, `inkWidth`를 넘겨 더 벌리지는 않는다 —
         * 그 이상은 겹침이 이미 풀렸기 때문이다.
         */
        function linePitch(charCount, lineInk, fontInk, lineFloor) {
            if (charCount <= 1) return GLYPH_W;
            // 작은 폰트는 늘리지 않는다 — 이미 셀 안에 들어 있다.
            if (fontInk <= GLYPH_W) return GLYPH_W;
            // [사용자 지정 사다리] n 글자 줄의 총 폭을 (n+1) 글자분으로 맞춘다.
            //   9자 126(변화 없음) · 8자 126 · 7자 112 · 6자 98 · 5자 84 …
            const total = (charCount + PITCH_SUM_OFFSET > MAX_PER_LINE)
                        ? MAX_PER_LINE * GLYPH_W : (charCount + PITCH_SUM_OFFSET) * GLYPH_W;
            let spread = Math.floor(total / charCount);
            // 잉크는 목표가 아니라 제한 두 개로만 쓴다.
            const inkCap = (lineInk > GLYPH_W) ? lineInk : fontInk;
            // 1) 겹침 방지 — 하단은 최소 필요한 값이라 **올린다**.
            //    잉크를 모르면 하단을 만들지 않는다(폰트 최댓값을 하한으로 쓰면 모든
            //    짧은 줄이 W 폭까지 밀린다). layout_engine.cpp와 동일한 규칙.
            const floor = (lineFloor > GLYPH_W) ? lineFloor
                        : ((lineInk > GLYPH_W) ? lineInk : 0);
            if (floor > spread) spread = floor;
            // 2) 화면 밖 잘림 방지 — 잘림은 겹침보다 나쁘다.
            const fit = Math.floor((SCREEN_W - inkCap) / (charCount - 1));
            if (spread > fit) spread = fit;
            return Math.max(GLYPH_W, spread);
        }

        /**
         * [§6.16c] 이 줄의 겹침 없는 최소 피치 — layout_engine.cpp `measureLineFloor()`의 미러.
         * 잉크는 래스터 안에서 **중앙 정렬**되므로 두 글자의 빈틈은
         * `피치 − (inkA + inkB) / 2`다. 겹치지 않으려면 그 이상이어야 하고,
         * 줄의 하한은 인접 쌍의 최댓값이다. 공백은 그려지지 않으므로 쌍에서 제외한다.
         * @param {string} lineText 이 줄에 놓일 문자열
         * @param {number} fallback 조회 실패 시 대체할 값 (보통 줄 최대)
         * @returns {number} 필요한 최소 피치
         */
        function measureLineFloor(lineText, fallback) {
            let floorInk = 0, prevInk = -1, seen = false;
            for (const ch of Array.from(lineText)) {
                if (ch === ' ') continue;
                const w = inkOfChar(ch);
                if (seen) {
                    const pair = Math.floor((prevInk + w + 1) / 2);   // 올림
                    if (pair > floorInk) floorInk = pair;
                }
                prevInk = w; seen = true;
            }
            return floorInk ? floorInk : fallback;
        }

        /**
         * [§6.16b] 한 줄의 최대 잉크 폭 — layout_engine.cpp `measureLineInk()`의 미러.
         * @param {string} lineText 이 줄에 놓일 문자열
         * @param {number} fallback 조회 실패 시 대체할 값 (보통 폰트 최대)
         * @returns {number} 이 줄의 최대 잉크 폭
         *
         * 공백도 한 **셀이므로** 문자를 하나씩 소비하되 잉크 상한에는 넣지 않는다
         * (그려지지 않으므로). 펌웨어는 캐시 표를, 웹은 캔버스를 본다.
         */
        function measureLineInk(lineText, fallback) {
            if (!inkOfChar) return fallback;
            let maxInk = 0;
            for (const ch of Array.from(lineText)) {
                if (ch === ' ') continue;
                const w = inkOfChar(ch);
                if (w > maxInk) maxInk = w;
            }
            return maxInk ? maxInk : fallback;
        }

        /**
         * [간격 확장 §6.16] 한 줄의 첫 셀 x — layout_engine.cpp `lineStartX()`의 미러.
         * @param {number} charCount 이 줄의 셀 수
         * @param {number} pitch linePitch()가 준 간격
         * @param {number} inkWidth 폰트 최대 잉크 폭
         * @returns {number} 첫 셀의 x
         *
         * 피치가 GLYPH_W와 같으면(9자·작은 폰트) 기존 셀 중앙 정렬을 그대로 쓴다.
         * 넓어졌을 때만 잉크 블록((n−1)×피치 + 잉크폭)을 화면 중앙에 둔다 —
         * 잉크는 셀의 중앙에 있으므로 셀 폭으로 중앙 정렬하면 왼쪽으로 쏠린다.
         */
        function lineStartX(charCount, pitch, lineInk) {
            if (pitch <= GLYPH_W) return Math.floor((SCREEN_W - charCount * pitch) / 2);
            const inkSpan = (charCount - 1) * pitch + lineInk;
            return Math.floor((SCREEN_W - inkSpan) / 2) + Math.floor((lineInk - GLYPH_W) / 2);
        }

        /**
         * 펌웨어 layout_engine.cpp layoutWrap()의 행 단위 미러.
         * - 어절이 2개 이상이면 첫 어절만 줄 0, 나머지는 줄 1부터
         * - 단어는 절대 쪼개지 않는다
         * - 가로 중앙 + 세로 중앙 (1줄이면 y=16, 2줄이면 y=0)
         * @param {boolean} [singleLine] [수정할 사항 1] true면 "어절 2개 이상 → 2줄" 규칙을
         *        건너뛴다 (숫자 모드 "02 H"). 펌웨어 layout_engine.cpp의 4번째 규칙과 1:1 대응.
         * @param {number} [inkWidth] [간격 확장 §6.16] 폰트 최대 잉크 폭.
         *        기본값은 previewInkWidth(미리보기에서는 refreshPreviewCache가 채운다).
         *        0이면 간격을 늘리지 않는다 — 펌웨어의 inkWidth 기본값과 같다.
         * @returns {{chars: Array<{c,x,y,line}>, dropped: number}}
         */
        function layoutWrap(text, singleLine = false, inkWidth = previewInkWidth) {
            const words = text.split(' ').filter(w => w.length > 0);
            if (words.length === 0) return { chars: [], dropped: 0 };

            // 1단계: 어절을 줄에 배정
            // [수정할 사항 1] 같은 줄에 이어지는 어절 사이의 공백은 빈 셀 한 칸으로 센다
            // (펌웨어 layout_engine.cpp의 pendingSpace 규칙과 1:1 대응)
            const lineCount = [0, 0];
            let lines = 1, dropped = 0, seenWord = false, pendingSpace = false;
            for (const w of words) {
                const n = Array.from(w).length;
                let gap = (pendingSpace && seenWord) ? 1 : 0;   // 어절 사이 공백 한 칸
                pendingSpace = false;
                // 어절 경계가 줄을 가른다 — 길이가 아님 ("FORTY ONE"도 2줄)
                // [수정할 사항 1] singleLine=true(숫자 모드 "02 H")면 이 규칙을 건너뛴다
                if (seenWord && !singleLine && lines < 2) {
                    lines = 2; lineCount[1] = 0;
                    gap = 0;   // 줄 경계 → 어절 사이 공백은 버려진다 (펌웨어와 동일)
                }
                seenWord = true;
                if (lineCount[lines - 1] + gap + n <= MAX_PER_LINE) {
                    lineCount[lines - 1] += gap + n;
                    pendingSpace = true;   // 다음 어절과의 사이 공백 (줄이 바뀌면 버려진다)
                    continue;
                }
                // singleLine 모드에서는 현재 줄에 못 들면 빈 다음 줄로 내려간다
                if (singleLine && lines < 2) {
                    lines = 2; lineCount[1] = 0;
                    gap = 0;   // 줄 경계 → 어절 사이 공백은 버려진다
                    if (lineCount[1] + n <= MAX_PER_LINE) { lineCount[1] += n; pendingSpace = true; continue; }
                }
                dropped += n; break;              // 단어는 전부 또는 아무것도
            }

            // 2단계: 줄 문자열 구성 → 줄 잉크 측정 → 문자 단위 x/y 배정
            // 세로 중앙: (64 - 줄수×32)/2 → 1줄 16, 2줄 0
            const baseY = Math.floor((SCREEN_H - lines * LINE_HEIGHT) / 2);
            const out = [];
            let wi = 0;
            for (let ln = 0; ln < lines; ln++) {
                const need = lineCount[ln];
                // 이 줄의 문자들을 먼저 모은다 — [§6.16b] 줄 잉크를 재려면
                // 좌표보다 글자가 먼저 필요하다. 어절 사이 공백은 한 셀로 넣어 둔다.
                const cells = [];
                let placed = 0;
                while (wi < words.length && placed < need) {
                    if (placed > 0) { cells.push(' '); placed++; }   // 같은 줄 어절 사이 공백 셀
                    for (const c of Array.from(words[wi])) { cells.push(c); placed++; }
                    wi++;
                }
                // [사용자 지정 사다리] 글자가 적은 줄일수록 피치가 넓어진다.
                // 9자 · 작은 폰트에서는 pitch가 GLYPH_W 그대로라 좌표가 바뀌지 않는다.
                // [§6.16b] 상한은 이 줄의 실제 잉크로 건다 (폰트 최대로 묶지 않는다).
                const lineText = cells.join('');
                const lineInk = measureLineInk(lineText, inkWidth);
                // [하한] 이 줄의 겹침 없는 최소 피치 — 사다리보다 좁으면 이것까지 올린다
                const lineFloor = measureLineFloor(lineText, lineInk);
                const pitch = linePitch(cells.length, lineInk, inkWidth, lineFloor);
                const startX = lineStartX(cells.length, pitch, lineInk);
                for (let i = 0; i < cells.length; i++) {
                    out.push({ c: cells[i], x: startX + i * pitch,
                               y: baseY + ln * LINE_HEIGHT, line: ln });
                }
            }
            return { chars: out, dropped };
        }

        // ==== 시간 문자열 생성 — english_time_core.cpp 미러 ====
        const SMALL = ["ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE",
                       "TEN", "ELEVEN", "TWELVE", "THIRTEEN", "FOURTEEN", "FIFTEEN",
                       "SIXTEEN", "SEVENTEEN", "EIGHTEEN", "NINETEEN"];   // numberToWords 0~19
        const TENS = ["", "", "TWENTY", "THIRTY", "FORTY", "FIFTY"];      // numberToWords 20~59
        const DAYS = ["SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"];

        function numberToWords(n) {
            if (n < 0 || n > 59) return "";
            if (n < SMALL.length) return SMALL[n];
            const tens = Math.floor(n / 10), ones = n % 10;
            if (ones === 0) return TENS[tens];
            return TENS[tens] + " " + SMALL[ones];
        }
        function hourToWords(h, is24h) {
            if (h < 0 || h > 23) return "";
            if (is24h) return numberToWords(h);
            const x = h % 12 === 0 ? 12 : h % 12;
            return numberToWords(x);
        }
        function minuteToWords(m) { return m === 0 ? "O'CLOCK" : numberToWords(m); }
        function secondToWords(s) { return s === 0 ? "O'CLOCK" : numberToWords(s); }
        function amPm(h) { return h < 12 ? "AM" : "PM"; }
        function twoDigit(n) { return n.toString().padStart(2, '0'); }
        function dayName(d) { return (d >= 0 && d < DAYS.length) ? DAYS[d] : ""; }
        function numericHour(h, is24h) { const x = is24h ? h : (h % 12 === 0 ? 12 : h % 12); return twoDigit(x); }
        /** engtime::dateString — "2/10"(일/월) 또는 "10/2"(월/일). 선행 0 없음. */
        function dateString(mon, mday, monthFirst) {
            if (mon < 1 || mon > 12 || mday < 1 || mday > 31) return "";
            return monthFirst ? `${mon}/${mday}` : `${mday}/${mon}`;
        }

        /** ENG_Clock.ino handleClockUpdate() 미러 */
        function getEnglishTimeStrings() {
            const now = new Date();
            const h = now.getHours(), m = now.getMinutes(), s = now.getSeconds();
            const d = now.getDay();                       // 0=Sunday — tm_wday와 동일 규약
            const isWord = els.disp.value === "0", is24H = els.hour.value === "1";

            let s0;
            if (is24H) {
                // [수정할 사항 3] "날짜 요일" 2줄. layoutWrap()의 공백 규칙이 줄을 나눈다.
                // 숫자 모드에서도 요일은 영문으로 표시한다 (예: MONDAY).
                const date = dateString(now.getMonth() + 1, now.getDate(),
                                        els.dateOrder.value === "1");
                const dayStr = dayName(d);
                s0 = (date && dayStr) ? `${date} ${dayStr}` : `${date}${dayStr}`;
            }
            else s0 = amPm(h);

            // [수정할 사항 2] 숫자 모드는 숫자만 두지 않고 시/분/초 단위를 오른쪽에 붙인다.
            // [수정할 사항 1] 숫자와 단위 문자 사이에 공백 한 칸을 둔다 ("02 H").
            //   펌웨어도 ENG_Clock.ino handleClockUpdate()에서 같은 규칙을 쓴다.
            const s1 = isWord ? hourToWords(h, is24H) : (numericHour(h, is24H) + " H");
            const s2 = isWord ? minuteToWords(m)      : (twoDigit(m) + " M");
            let   s3 = isWord ? secondToWords(s)      : (twoDigit(s) + " S");

            // 정각 규칙: 분=0 & 초=0 이면 Screen 4를 비운다 (O'CLOCK은 Screen 3이 담당)
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
         * [버그 3 수정] 한 줄의 세로 영역( baseY ~ baseY+LINE_HEIGHT )으로만 그리도록 잘라낸다.
         *
         * 원인: 스크롤 애니메이션이 SCREEN_H(64px)를 이동해서, 2줄일 때 아래줄 글자가
         *   위로 올라가며 위줄 글자(TWENTY)와 겹쳤다.
         * 해결: 이동량을 LINE_HEIGHT(32)로 줄이고, 여기에 더해 그리기 자체를 줄 밴드로
         *   클립한다. 두 가지를 함께 해야 슬라이드·스케일·페이드·줌 전부에서 새어나가지 않는다.
         *
         * [여백 수정 v5] **1줄 레이아웃**(모든 글자 y = 세로 중앙 16)은 클립하지 않는다 —
         *   화면 전체가 그 줄의 영역이므로 큰 폰트(잉크 64px)도 위아래로 잘리지 않고,
         *   스크롤 글자가 화면 가장자리에서 드나난다. 펌웨어 drawAnimPair/drawCenterText의
         *   1줄 규칙과 1:1 대응한다.
         *
         * 펌웨어는 U8g2 의 클립 API를 쓸 수 없다(이 빌드에서 그리기엔 무효 — §6.9 v2 참조).
         *   그래서 display_manager.cpp 의 스크롤 드로잉 6곳이
         *   renderer.cpp 의 Renderer::drawSingleCharClipped() 를 부르고,
         *   그쪽이 비트맵을 행 단위로 잘라 그린다. 결과는 여기 ctx.clip() 와 같다.
         *
         * @param {CanvasRenderingContext2D} ctx 대상 캔버스
         * @param {Array} chars layoutWrap() 출력 — 같은 줄의 문자가 연속으로 나온다
         * @param {(d:Object)=>void} drawOne 글자 하나를 그리는 동작
         */
        function drawByLine(ctx, chars, drawOne) {
            const singleLine = chars.length === 0 ||
                chars.every(d => d.y === (SCREEN_H - LINE_HEIGHT) / 2);
            if (singleLine) {
                for (const d of chars) drawOne(d);
                return;
            }
            let openY = null;
            for (const d of chars) {
                if (d.y !== openY) {
                    if (openY !== null) ctx.restore();
                    ctx.save(); ctx.beginPath();
                    ctx.rect(0, d.y, SCREEN_W, LINE_HEIGHT);
                    ctx.clip();
                    openY = d.y;
                }
                drawOne(d);
            }
            if (openY !== null) ctx.restore();
        }

        function render() {
            const currentTimeStrings = getEnglishTimeStrings();
            if (targetTimeStrings[0] === "") targetTimeStrings = [...currentTimeStrings];
            let changed = currentTimeStrings.some((s, i) => s !== targetTimeStrings[i]);
            if (changed && animStep >= 16) {
                lastTimeStrings = [...targetTimeStrings]; targetTimeStrings = [...currentTimeStrings];
                animStep = (["1", "2", "3", "4", "5"].includes(els.anim.value)) ? 0 : 16;
            }
            if (animStep < 16) animStep++;

            // 한 줄 높이를 온전히 이동한다 (이전엔 화면 높이 64px를 이동했다)
            const mode = els.anim.value, off = animStep * (LINE_HEIGHT / 16);
            // [수정할 사항 1] 숫자 모드에서는 H/M/S 화면(s≠0)을 한 줄로 배치한다 —
            // 펌웨어 DisplayManager::updateAll()의 singleLine 규칙과 1:1 대응한다.
            const isWordPreview = els.disp.value === "0";
            for (let s = 0; s < 4; s++) {
                const ctx = pCtx[s], isInverted = els.invert.value === "1";
                ctx.fillStyle = isInverted ? "#fff" : "#000"; ctx.fillRect(0, 0, SCREEN_W, SCREEN_H);

                const curD = layoutWrap(targetTimeStrings[s], !isWordPreview && s !== 0).chars;
                if (animStep >= 16 || mode === "0") {
                    drawByLine(ctx, curD, d => drawChar(ctx, d.c, d.x, d.y));
                } else {
                    const oldD = layoutWrap(lastTimeStrings[s], !isWordPreview && s !== 0).chars;
                    drawByLine(ctx, curD, nd => {
                        const baseY = nd.y;      // layoutWrap()이 세로 중앙까지 계산해 준다
                        // 문자 정체성 = (줄, x). 2줄에서 x만으로는 구분되지 않는다.
                        const od = oldD.find(o => o.line === nd.line && o.x === nd.x);
                        if (od && od.c === nd.c) { drawChar(ctx, nd.c, nd.x, baseY); return; }
                        switch (mode) {
                            case "1": if (od) drawChar(ctx, od.c, nd.x, baseY - off);
                                      drawChar(ctx, nd.c, nd.x, baseY + LINE_HEIGHT - off); break;
                            case "2": if (od) drawChar(ctx, od.c, nd.x, baseY + off);
                                      drawChar(ctx, nd.c, nd.x, baseY - LINE_HEIGHT + off); break;
                            case "3": if (animStep <= 8) { if (od) drawScaledChar(ctx, od.c, nd.x, baseY, ((8 - animStep) * LINE_HEIGHT) / 8); }
                                      else { drawScaledChar(ctx, nd.c, nd.x, baseY, ((animStep - 8) * LINE_HEIGHT) / 8); } break;
                            case "4": if (animStep <= 8) { ctx.save(); ctx.globalAlpha = (8 - animStep) / 8;
                                          if (od) drawChar(ctx, od.c, nd.x, baseY); ctx.restore(); }
                                      else { ctx.save(); ctx.globalAlpha = (animStep - 8) / 8;
                                          drawChar(ctx, nd.c, nd.x, baseY); ctx.restore(); } break;
                            case "5": if (animStep <= 8) { if (od) drawZoomedChar(ctx, od.c, nd.x, baseY, (8 - animStep) / 8); }
                                      else { const sc = (animStep <= 12) ? ((animStep - 8) * 150 / 100 / 4) : (1.5 - (animStep - 12) * 50 / 100 / 4);
                                             drawZoomedChar(ctx, nd.c, nd.x, baseY, sc); } break;
                        }
                    });
                    drawByLine(ctx, oldD, od => {
                        if (curD.find(nd => nd.line === od.line && nd.x === od.x)) return;
                        const baseY = od.y;
                        switch (mode) {
                            case "1": drawChar(ctx, od.c, od.x, baseY - off); break;
                            case "2": drawChar(ctx, od.c, od.x, baseY + off); break;
                            case "3": if (animStep <= 8) drawScaledChar(ctx, od.c, od.x, baseY, ((8 - animStep) * LINE_HEIGHT) / 8); break;
                            case "4": if (animStep <= 8) { ctx.save(); ctx.globalAlpha = (8 - animStep) / 8;
                                          drawChar(ctx, od.c, od.x, baseY); ctx.restore(); } break;
                            case "5": if (animStep <= 8) drawZoomedChar(ctx, od.c, od.x, baseY, (8 - animStep) / 8); break;
                        }
                    });
                }
                if (s === 0 && els.chime.value === "1") drawChimeIcon(ctx);
            }
            requestAnimationFrame(render);
        }
        render();

        async function processAll() {
            els.apply.disabled = true; els.pWrap.style.display = "block";
            UNIQ_CHARS.forEach(c => document.getElementById('b_' + c).classList.remove('active'));

            // 업로드와 미리보기가 **같은 렌더링 결과**를 써야 한다. 캐시를 먼저 통째로
            // 만들어 두고, 아래 루프는 그 캔버스를 그대로 패킹한다 (두 번 그리면 다를 수 있다).
            refreshPreviewCache();
            const size = currentGlyphSize();
            for (let i = 0; i < UNIQ_CHARS.length; i++) {
                const ch = UNIQ_CHARS[i];
                els.status.innerText = `Uploading ${ch} (${i + 1}/${UNIQ_CHARS.length})`;
                const canvas = bitmapCache[ch] || renderGlyph(ch, size);
                const bm = packGlyph(canvas);

                let hex = "";
                new TextEncoder().encode(ch).forEach(b => hex += b.toString(16).toUpperCase().padStart(2, '0'));
                const fd = new FormData(); fd.append('file', new Blob([bm]), `c_${hex}.bin`);

                document.getElementById('b_' + ch).classList.add('active');
                await fetch(`/upload?slot=${els.slot.value}`, { method: 'POST', body: fd });
                els.pFill.style.width = ((i + 1) / UNIQ_CHARS.length * 100) + "%";
            }
            await fetch('/api/refresh_cache', { method: 'POST' });
            if (els.fIn.files[0]) await fetch('/api/config', { method: 'POST', body: JSON.stringify({ font_name: els.fIn.files[0].name }) });
            els.status.innerText = "All glyphs uploaded!"; els.apply.disabled = false;
        }
    </script>
</body>
</html>
)rawliteral";

#endif