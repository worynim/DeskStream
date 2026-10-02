// worynim@gmail.com
/**
 * @file web_pages.h
 * @brief 웹 설정 대시보드 및 폰트 스튜디오 리소스
 * @details HTML, CSS, JavaScript 등으로 구성된 임베디드 웹 페이지 리소스 관리 (PROGMEM 활용)
 * @note [SYNC] ENG_Clock/web_pages.h — 시간대 셀렉트 + POSIX TZ 입력칸 추가.
 *       옵션 value가 곧 POSIX TZ 문자열이라 펌웨어와 매핑 테이블이 없다.
 *       tzSel/tzCustom/tzPending 는 ENG판과 동일 계약이다.
 */
#ifndef WEB_PAGES_H
#define WEB_PAGES_H

#include <pgmspace.h>

// === 고전/현대 조화 Font Studio HTML ===
const char font_studio_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ko">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Font Studio v2</title>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Outfit:wght@400;600&family=Noto+Sans+KR:wght@500;700&display=swap');
        :root { --primary: #00f2fe; --secondary: #4facfe; --bg: #0b0e14; --card: rgba(255, 255, 255, 0.05); }
        body { background: var(--bg); color: #fff; font-family: 'Outfit', 'Noto Sans KR', sans-serif; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
        .glass { background: var(--card); backdrop-filter: blur(15px); border: 1px solid rgba(255,255,255,0.1); border-radius: 24px; padding: 30px; width: 100%; max-width: 800px; box-shadow: 0 20px 50px rgba(0,0,0,0.5); }
        h1 { font-weight: 600; font-size: 2.2rem; background: linear-gradient(135deg, #00f2fe 0%, #4facfe 100%); -webkit-background-clip: text; -webkit-text-fill-color: transparent; text-align: center; margin-top:0; }
        .desc { text-align: center; color: #888; font-size: 0.9rem; margin-bottom: 30px; }
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
        .preview-item { display: flex; flex-direction: column; align-items: center; gap: 8px; background: rgba(0,0,0,0.4); padding: 12px 8px; border-radius: 12px; border: 1px solid #333; width: 145px; }
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
        <h1>한글 시계 Font Studio</h1>
        <p class="desc">DeskStream Project | 현재 적용 폰트: <span id="curFont" style="color:var(--primary)">-</span></p>
        
        <div class="setup-grid">
            <div class="field">
                <label>1. 폰트 선택 (.ttf, .otf)</label>
                <input type="file" id="fIn" accept=".ttf,.otf">
            </div>
            <div class="field">
                <label>2. 폰트 크기: <span id="sVal">48</span>px</label>
                <input type="range" id="sIn" min="20" max="60" value="48">
            </div>
            <div class="field">
                <label>3. 저장 슬롯 (0~4)</label>
                <select id="fontSlot">
                    <option value="0">Slot 0 (기존 폰트)</option>
                    <option value="1">Slot 1</option>
                    <option value="2">Slot 2</option>
                    <option value="3">Slot 3</option>
                    <option value="4">Slot 4</option>
                </select>
            </div>
        </div>

        <div class="setup-grid">
            <div class="field">
                <label>4. 애니메이션 (BTN3 Short)</label>
                <select id="animMode">
                    <option value="0">0. OFF</option>
                    <option value="1">1. Scroll Up</option>
                    <option value="2">2. Scroll Down</option>
                    <option value="3">3. Vertical Flip</option>
                    <option value="4">4. Dithered Fade</option>
                    <option value="5">5. Zoom In/Out</option>
                    <option value="6">6. Snow Assemble</option>
                </select>
            </div>
            <div class="field">
                <label>4. 표시 유형 (BTN2 Short)</label>
                <select id="displayMode">
                    <option value="0">한글 (열두시 삼십분 사십오초)</option>
                    <option value="1">숫자 (12시 30분 45초)</option>
                </select>
            </div>
            <div class="field">
                <label>5. 시간 형식 (BTN2 Long)</label>
                <select id="hourFormat">
                    <option value="0">12시간제 (오전/오후)</option>
                    <option value="1">24시간제 (0시~23시)</option>
                </select>
            </div>
            <div class="field">
                <label>6. 정시 시보 (BTN1 Short)</label>
                <select id="chime">
                    <option value="0">OFF</option>
                    <option value="1">ON</option>
                </select>
            </div>
            <div class="field">
                <label>7. 화면 반전 (BTN1 Long)</label>
                <select id="flipMode">
                    <option value="0">NORMAL</option>
                    <option value="1">FLIP</option>
                </select>
            </div>
            <div class="field">
                <label>8. 색상 반전 (BTN4 Long)</label>
                <select id="invertMode">
                    <option value="0">NORMAL (Black BG)</option>
                    <option value="1">INVERT (White BG)</option>
                </select>
            </div>
            <div class="field">
                <label>9. OLED 밝기: <span id="brVal">1</span></label>
                <input type="range" id="brIn" min="1" max="255" value="1">
            </div>
        </div>

        <div class="setup-grid">
            <div class="field">
                <label>10. 시간대 (DST 자동 적용)</label>
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
                    <option value="CUSTOM">직접 입력&hellip; (아래 POSIX TZ)</option>
                </select>
            </div>
            <div class="field">
                <label>POSIX TZ 문자열 <span id="tzHint" style="color:#666">(읽기 전용)</span></label>
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
        <div id="status" class="status-msg">설정을 로드하는 중...</div>

        <button class="btn-apply" id="apply" onclick="processAll()" disabled>폰트 세트 일괄 업로드</button>

        <div class="inventory" id="inv"></div>
        <div class="footer">
            <a href="https://gongu.copyright.or.kr/gongu/bbs/B0000018/list.do?menuNo=200195" target="_blank">무료폰트 다운로드</a>
        </div>
    </div>

    <script>
        const UNIQ_CHARS = "오전후한시두세네다섯여일곱덟아홉열영이삼사육칠팔구십분초정각0123456789".split("");
        const bitmapCache = {};
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
            tzSel: document.getElementById('tzSel'),
            tzCustom: document.getElementById('tzCustom'),
            tzHint: document.getElementById('tzHint')
        };

        UNIQ_CHARS.forEach(c => {
            const d = document.createElement('div');
            d.className = 'badge'; d.id = 'b_' + c; d.innerText = c;
            document.getElementById('inv').appendChild(d);
        });

        /** Custom 모드 여부에 따라 TZ 입력칸을 편집 가능/읽기전용으로 전환한다 */
        function setTzEditable(editable, value) {
            els.tzCustom.readOnly = !editable;
            els.tzHint.innerText = editable ? "(입력 가능 — 전송 시 적용)" : "(읽기 전용)";
            if (value !== undefined) els.tzCustom.value = value;
        }

        /**
         * 서버 반영을 아직 확인하지 못한 시간대 값 — 5초 폴링이 이를 옛 값으로 되돌린다.
         * 사용자가 시간대를 바꾸고 POST가 오기 전에 폴링 응답이 도착하면 선택이 튀었다가
         * 되감기는 문제다. 저장이 확인되면 해제하고, 실패하면 그대로 둔다(재시도 가능).
         */
        let tzPending = false;

        async function fetchConfig() {
            try {
                const res = await fetch('/api/config');
                const data = await res.json();
                els.anim.value = (data.anim_mode ?? 1).toString();
                els.disp.value = (data.display_mode ?? 0).toString();
                els.hour.value = (data.hour_format ?? 0).toString();
                els.chime.value = data.chime_enabled ? "1" : "0";
                els.flip.value = data.is_flipped ? "1" : "0";
                els.invert.value = data.is_inverted ? "1" : "0";
                els.slot.value = (data.font_slot ?? 0).toString();
                els.brIn.value = (data.brightness ?? 1).toString();
                els.brVal.innerText = els.brIn.value;

                // 시간대: 저장된 POSIX 문자열과 일치하는 옵션을 선택하고,
                // 목록에 없는 문자열이면 Custom 모드로 입력칸을 연다.
                if (typeof data.timezone === 'string' && data.timezone.length
                    && !tzPending && els.tzSel !== document.activeElement) {
                    let matched = false;
                    for (const opt of els.tzSel.options) {
                        if (opt.value !== 'CUSTOM' && opt.value === data.timezone) {
                            els.tzSel.value = opt.value; matched = true; break;
                        }
                    }
                    if (!matched) els.tzSel.value = 'CUSTOM';
                    setTzEditable(els.tzSel.value === 'CUSTOM', data.timezone);
                }

                // 슬롯 이름 업데이트
                if (data.slot_names) {
                    for(let i=0; i<5; i++) {
                        const opt = els.slot.options[i];
                        if (opt) opt.innerText = `Slot ${i} (${data.slot_names[i]})`;
                    }
                }

                els.curFont.innerText = `Slot ${data.font_slot}: ${data.font_name || 'System Default'}`;
                els.status.innerText = "설정 동기화 완료";
            } catch(e) { els.status.innerText = "설정 로드 실패"; }
        }

        async function saveConfig() {
            els.status.innerText = "저장 중...";
            const body = {
                anim_mode: parseInt(els.anim.value),
                display_mode: parseInt(els.disp.value),
                hour_format: parseInt(els.hour.value),
                chime_enabled: els.chime.value === "1",
                is_flipped: els.flip.value === "1",
                is_inverted: els.invert.value === "1",
                font_slot: parseInt(els.slot.value),
                brightness: parseInt(els.brIn.value),
                timezone: els.tzCustom.value
            };
            tzPending = true;
            try {
                const res = await fetch('/api/config', { method: 'POST', body: JSON.stringify(body) });
                // 응답을 확인하지 않으면 거절된 시간대도 "설정 저장됨"으로 보인다.
                // fetch는 네트워크 단절에서만 reject 된다.
                if (!res.ok) throw new Error(`HTTP ${res.status}`);
                tzPending = false;
                els.status.innerText = "설정 저장됨";
            } catch(e) {
                // 실패하면 tzPending을 지우지 않는다. 그래야 다음 폴링이 사용자 값을
                // 지워버리지 않고, 사용자가 재시도할 수 있다.
                els.status.innerText = `저장 실패 (${e.message}) — 값을 확인하고 다시 시도하세요`;
            }
        }

        [els.anim, els.disp, els.hour, els.chime, els.flip, els.invert, els.slot].forEach(el => el.onchange = saveConfig);
        els.brIn.oninput = () => { els.brVal.innerText = els.brIn.value; };
        els.brIn.onchange = saveConfig;
        // 시간대 선택: 옵션의 value가 곧 POSIX TZ 문자열이므로 별도 매핑 테이블이 없다.
        els.tzSel.onchange = () => {
            if (els.tzSel.value === 'CUSTOM') setTzEditable(true);   // 값은 사용자가 입력
            else { setTzEditable(false, els.tzSel.value); saveConfig(); }
        };
        els.tzCustom.onchange = saveConfig;
        fetchConfig();
        setInterval(fetchConfig, 5000);

        const pCtx = [0,1,2,3].map(i => document.getElementById(`p${i}`).getContext('2d'));
        let fontLoaded = false;

        els.fIn.onchange = async (e) => {
            const file = e.target.files[0]; if(!file) return;
            const buffer = await file.arrayBuffer();
            const font = new FontFace("ClockFont", buffer);
            await font.load(); document.fonts.add(font);
            fontLoaded = true; els.apply.disabled = false;
            els.status.innerText = "폰트 준비됨. 미리보기를 확인하세요.";
        };
        els.sIn.oninput = () => { document.getElementById('sVal').innerText = els.sIn.value; };

        function drawChar(ctx, char, x, yOffset) {
            const charData = bitmapCache[char];
            const isInverted = els.invert.value === "1";
            if (charData) {
                if (isInverted) {
                    // 비트맵 반전 처리 (임시 캔버스 활용)
                    const tempCanvas = document.createElement('canvas'); tempCanvas.width=64; tempCanvas.height=64;
                    const tCtx = tempCanvas.getContext('2d');
                    tCtx.fillStyle = "#fff"; tCtx.fillRect(0,0,64,64);
                    tCtx.globalCompositeOperation = 'destination-out';
                    tCtx.drawImage(charData.canvas, 0, 0);
                    ctx.drawImage(tempCanvas, x, yOffset);
                } else {
                    ctx.drawImage(charData.canvas, x, yOffset);
                }
            }
            else {
                ctx.fillStyle = isInverted ? "#000" : "#fff";
                ctx.font = `${els.sIn.value}px ClockFont, sans-serif`;
                ctx.textAlign = "center"; ctx.textBaseline = "middle";
                ctx.fillText(char, x + 16, 32 + yOffset);
            }
        }

        function drawScaledChar(ctx, charStr, x, h) {
            if (h <= 0) return;
            const charData = bitmapCache[charStr];
            const isInverted = els.invert.value === "1";
            if (charData) {
                if (isInverted) {
                    const tempCanvas = document.createElement('canvas'); tempCanvas.width=64; tempCanvas.height=64;
                    const tCtx = tempCanvas.getContext('2d');
                    tCtx.fillStyle = "#fff"; tCtx.fillRect(0,0,64,64);
                    tCtx.globalCompositeOperation = 'destination-out';
                    tCtx.drawImage(charData.canvas, 0, 0);
                    ctx.drawImage(tempCanvas, x, (64 - h) / 2, 64, h);
                } else {
                    ctx.drawImage(charData.canvas, x, (64 - h) / 2, 64, h);
                }
            }
            else {
                ctx.save(); ctx.translate(x + 16, 32); ctx.scale(1, h / 64);
                ctx.fillStyle = isInverted ? "#000" : "#fff";
                ctx.font = `${els.sIn.value}px ClockFont, sans-serif`;
                ctx.textAlign = "center"; ctx.textBaseline = "middle";
                ctx.fillText(charStr, 0, 0); ctx.restore();
            }
        }

        function drawZoomedChar(ctx, charStr, x, scale) {
            if (scale <= 0) return;
            const charData = bitmapCache[charStr];
            const isInverted = els.invert.value === "1";
            if (charData) {
                const w = charData.size <= 256 ? 32 : 64;
                const bx = charData.size <= 256 ? x : x - 16;
                const tw = w * scale, th = 64 * scale;
                if (isInverted) {
                    const tempCanvas = document.createElement('canvas'); tempCanvas.width=64; tempCanvas.height=64;
                    const tCtx = tempCanvas.getContext('2d');
                    tCtx.fillStyle = "#fff"; tCtx.fillRect(0,0,64,64);
                    tCtx.globalCompositeOperation = 'destination-out';
                    tCtx.drawImage(charData.canvas, 0, 0);
                    ctx.drawImage(tempCanvas, bx + (w - tw) / 2, (64 - th) / 2, tw, th);
                } else {
                    ctx.drawImage(charData.canvas, bx + (w - tw) / 2, (64 - th) / 2, tw, th);
                }
            } else {
                ctx.save(); ctx.translate(x + 16, 32); ctx.scale(scale, scale);
                ctx.fillStyle = isInverted ? "#000" : "#fff";
                ctx.font = `${els.sIn.value}px ClockFont, sans-serif`;
                ctx.textAlign = "center"; ctx.textBaseline = "middle";
                ctx.fillText(charStr, 0, 0); ctx.restore();
            }
        }

        function getHangeulTimeStrings() {
            const now = new Date();
            let h = now.getHours(), m = now.getMinutes(), s = now.getSeconds(), d = now.getDate();
            const isHangul = els.disp.value === "0", is24H = els.hour.value === "1";
            
            const toHangulNum = (num, unit) => {
                if (num === 0 && (unit === "분" || unit === "초")) return "정각";
                const tList = ["", "십", "이십", "삼십", "사십", "오십"], nList = ["", "일", "이", "삼", "사", "오", "육", "칠", "팔", "구"];
                if (num === 0) return "영" + unit;
                return tList[Math.floor(num / 10)] + nList[num % 10] + unit;
            };
            
            const toNumericNum = (num, unit) => num.toString().padStart(2, '0') + unit;
            
            const getHangulHour = (h, is24h) => {
                let hr = is24h ? h : (h % 12 || 12);
                if (is24h && hr === 0) return "영시";
                if (is24h && hr >= 13) return toHangulNum(hr, "시");
                const h_ones = ["", "한", "두", "세", "네", "다섯", "여섯", "일곱", "여덟", "아홉", "열", "열한", "열두"];
                if (hr <= 12) return h_ones[hr] + "시";
                return hr + "시";
            };

            let s0 = is24H ? (isHangul ? toHangulNum(d, "일") : d + "일") : (h < 12 ? "오전" : "오후");
            let s1 = isHangul ? getHangulHour(h, is24H) : toNumericNum(is24H ? h : (h % 12 || 12), "시");
            let s2 = isHangul ? toHangulNum(m, "분") : toNumericNum(m, "분");
            let s3 = isHangul ? toHangulNum(s, "초") : toNumericNum(s, "초");

            if (isHangul && m === 0 && s === 0) s3 = "";

            return [s0, s1, s2, s3];
        }

        let lastTimeStrings = ["", "", "", ""], targetTimeStrings = ["", "", "", ""], animStep = 16, animTransition = 0;
        function getCharPositions(text, isCentered) {
            const chars = Array.from(text), count = chars.length;
            if (count === 0) return [];
            let startX = (isCentered) ? (128 - count * 32) / 2 : (96 - (count - 1) * 32) / 2;
            return chars.map((c, i) => ({ c, x: (isCentered || i < count - 1) ? (startX + i * 32) : 96 }));
        }

        function drawChimeIcon(ctx) {
            if (els.chime.value !== "1") return;
            const bell = [0x18, 0x3C, 0x3C, 0x3C, 0xFF, 0xDB, 0x18, 0x00];
            ctx.fillStyle = els.invert.value === "1" ? "#000" : "#fff";
            for(let y=0; y<8; y++) for(let x=0; x<8; x++) if(bell[y] & (1 << (7-x))) ctx.fillRect(x, y, 1, 1);
        }

        // ── 눈 조립 모드 (펌웨어 renderer.cpp / display_manager.cpp 미러) ──
        // 모드별 총 프레임 수. 눈 조립만 48프레임(기존 모드는 16)이다. config.h와 맞춰야 한다.
        const ANIM_MAX_STEP = {1:16, 2:16, 3:16, 4:16, 5:16, 6:48};
        const SNOW_W = 128, SNOW_H = 64, SNOW_SPAWN_Y = -2, SNOW_PROGRESS_FULL = 255, SNOW_ARRIVAL_MAX = 240;

        /** C++ 정수 나눗셈(0 방향 절삭)을 그대로 흉내낸다 */
        const idiv = (a, b) => Math.trunc(a / b);

        /** 좌표 기반 결정적 해시 — 매 프레임 같은 눈이 나오도록 위치가 깜빡이지 않는다 */
        function animHash(seed, x, y) {
            let h = Math.imul(seed, 2654435761) >>> 0;
            h = (h ^ ((Math.imul(x, 40503) & 0xFFFF) + 0x9E3779B9)) >>> 0;
            h = (h ^ ((Math.imul(y, 42137) & 0xFFFF) + 0x85EBCA6B)) >>> 0;
            h = (h ^ (h >>> 15)) >>> 0;
            h = Math.imul(h, 2246822519) >>> 0;
            h = (h ^ (h >>> 13)) >>> 0;
            return (h >>> 8) & 0xFFFF;
        }

        /** 아래쪽 픽셀일수록 늦게 도착하고, 같은 높이는 해시로 흩뿌린다 */
        function animArrival(seed, px, py) {
            const base = idiv(py * 200, SNOW_H);
            const jitter = (animHash(seed, px, py) % 81) - 40;
            return Math.max(0, Math.min(SNOW_ARRIVAL_MAX, base + jitter));
        }

        /** 위에서 떨어져 e = t² 가속으로 조립되는 픽셀 위치 */
        function animAssembling(seed, px, py, progress) {
            const arrive = animArrival(seed, px, py);
            if (progress >= arrive) return {x: px, y: py, on: 1};
            const den = arrive, eNum = progress * progress, eDen = den * den;
            const spawn = animHash((seed ^ 0x5A5A) & 0xFFFF, px, py) % SNOW_W;
            const sway = (animHash((seed ^ 0xA5A5) & 0xFFFF, px, py) % 9) - 4;
            const x = spawn + idiv((px - spawn) * progress, den) + idiv(sway * progress * (den - progress), den * 48);
            const y = SNOW_SPAWN_Y + idiv((py - SNOW_SPAWN_Y) * eNum, eDen);
            return {x: x, y: y, on: (y >= 0 && y < SNOW_H) ? 1 : 0};
        }

        /** 아래로 가라앉으며 흩어지는 픽셀 위치 */
        function animDispersing(seed, px, py, progress) {
            const h = animHash(seed, px, py);
            const delay = h % 48;
            if (progress <= delay) return {x: px, y: py, on: 1};
            const num = progress - delay, den = SNOW_PROGRESS_FULL - delay;
            const drift = ((h >>> 8) % 7) - 3;
            const y = py + idiv((SNOW_H - py) * num * num, den * den);
            const x = px + idiv(drift * num, 128);
            return {x: x, y: y, on: (y < SNOW_H) ? 1 : 0};
        }

        function snowSeed(transitionId, screenIdx, slot, x) {
            return (transitionId * 7919 + screenIdx * 131 + slot * 17 + x * 3) & 0xFFFF;
        }

        /**
         * 글자의 켜진 픽셀 목록 [x0,y0,x1,y1,...]을 캐시한다.
         * 오프스크린 캔버스에 글자를 렌더링한 뒤 getImageData로 뽑는다. 폰트·크기·반전 여부가
         * 바뀌면 캐시 키도 바뀌어 자동으로 무효화된다.
         */
        const snowPixelCache = {};
        function getSnowPixels(charStr, x) {
            const key = `${charStr}|${x}|${els.sIn.value}|${els.invert.value}|${fontLoaded}`;
            if (snowPixelCache[key]) return snowPixelCache[key];
            const off = document.createElement('canvas'); off.width = 32; off.height = SNOW_H;
            const octx = off.getContext('2d', {willReadFrequently: true});
            octx.fillStyle = "#fff";
            octx.font = `${els.sIn.value}px ClockFont, sans-serif`;
            octx.textAlign = "center"; octx.textBaseline = "middle";
            octx.fillText(charStr, 16, SNOW_H / 2);
            const img = octx.getImageData(0, 0, 32, SNOW_H).data;
            const pts = [];
            for (let y = 0; y < SNOW_H; y++)
                for (let px = 0; px < 32; px++)
                    if (img[(y * 32 + px) * 4 + 3] > 128) pts.push(px, y);
            snowPixelCache[key] = pts;
            return pts;
        }

        function drawSnowChar(ctx, charStr, x, progress, seed) {
            const pts = getSnowPixels(charStr, x);
            ctx.fillStyle = els.invert.value === "1" ? "#000" : "#fff";
            for (let i = 0; i < pts.length; i += 2) {
                const p = animAssembling(seed, x + pts[i], pts[i + 1], progress);
                if (p.on) ctx.fillRect(p.x, p.y, 1, 1);
            }
        }

        function drawDispersingSnowChar(ctx, charStr, x, progress, seed) {
            const pts = getSnowPixels(charStr, x);
            ctx.fillStyle = els.invert.value === "1" ? "#000" : "#fff";
            for (let i = 0; i < pts.length; i += 2) {
                const p = animDispersing(seed, x + pts[i], pts[i + 1], progress);
                if (p.on) ctx.fillRect(p.x, p.y, 1, 1);
            }
        }

        function render() {
            const currentTimeStrings = getHangeulTimeStrings();
            if (targetTimeStrings[0] === "") targetTimeStrings = [...currentTimeStrings];
            const mode = els.anim.value, maxStep = ANIM_MAX_STEP[mode] ?? 16;
            const snowProgress = Math.trunc(animStep * SNOW_PROGRESS_FULL / maxStep);
            let changed = currentTimeStrings.some((s, i) => s !== targetTimeStrings[i]);
            if (changed && animStep >= maxStep) {
                lastTimeStrings = [...targetTimeStrings]; targetTimeStrings = [...currentTimeStrings];
                animTransition++; // 펌웨어의 transitionId와 같은 역할: 전환마다 다른 눈
                animStep = (mode !== "0") ? 0 : maxStep;
            }
            if (animStep < maxStep) animStep++;
            for(let s=0; s<4; s++) {
                const ctx = pCtx[s]; const isInverted = els.invert.value === "1";
                ctx.fillStyle = isInverted ? "#fff" : "#000"; ctx.fillRect(0,0,128,64);
                const isC = (s === 0) || (targetTimeStrings[s] === "정각");
                const curD = getCharPositions(targetTimeStrings[s], isC);
                if (animStep >= maxStep || mode === "0") curD.forEach(d => drawChar(ctx, d.c, d.x, 0));
                else {
                    const isOC = (s === 0) || (lastTimeStrings[s] === "정각");
                    const off = animStep * 4, oldD = getCharPositions(lastTimeStrings[s], isOC);
                    curD.forEach((nd, slot) => {
                        let od = oldD.find(o => o.x === nd.x);
                        if (od && od.c === nd.c) drawChar(ctx, nd.c, nd.x, 0);
                        else {
                            switch(mode) {
                                case "1": if(od) drawChar(ctx, od.c, nd.x, -off); drawChar(ctx, nd.c, nd.x, 64-off); break;
                                case "2": if(od) drawChar(ctx, od.c, nd.x, off); drawChar(ctx, nd.c, nd.x, -64+off); break;
                                case "3": if(animStep<=8) { if(od) drawScaledChar(ctx, od.c, nd.x, ((8-animStep)/8)*64); } else drawScaledChar(ctx, nd.c, nd.x, ((animStep-8)/8)*64); break;
                                case "4": ctx.save(); if(animStep<=8) { ctx.globalAlpha=(8-animStep)/8; if(od) drawChar(ctx, od.c, nd.x, 0); } else { ctx.globalAlpha=(animStep-8)/8; drawChar(ctx, nd.c, nd.x, 0); } ctx.restore(); break;
                                case "5": if(animStep<=8) { if(od) drawZoomedChar(ctx, od.c, nd.x, (8-animStep)/8); } else { let sc = (animStep<=12)?((animStep-8)*1.5/4):(1.5-(animStep-12)*0.5/4); drawZoomedChar(ctx, nd.c, nd.x, sc); } break;
                                case "6": drawSnowChar(ctx, nd.c, nd.x, snowProgress, snowSeed(animTransition, s, slot, nd.x)); break;
                            }
                        }
                    });
                    oldD.forEach((od, slot) => {
                        const replaced = curD.find(nd => nd.x === od.x);
                        // 같은 글자는 첫 번째 반복문에서 이미 정적으로 그렸다
                        if (replaced && replaced.c === od.c) return;
                        // 기존 모드는 같은 자리를 새 글자가 넘겨받으면 옛 글자를 그냥 지운다.
                        // 눈 조립만 새 글자와 함께 가라앉힌다 (펌웨어 renderSnowFrame와 동일)
                        if (replaced && mode !== "6") return;
                        switch(mode) {
                            case "1": drawChar(ctx, od.c, od.x, -off); break;
                            case "2": drawChar(ctx, od.c, od.x, off); break;
                            case "3": if(animStep<=8) drawScaledChar(ctx, od.c, od.x, ((8-animStep)/8)*64); break;
                            case "4": if(animStep<=8) { ctx.save(); ctx.globalAlpha=(8-animStep)/8; drawChar(ctx, od.c, od.x, 0); ctx.restore(); } break;
                            case "5": if(animStep<=8) drawZoomedChar(ctx, od.c, od.x, (8-animStep)/8); break;
                            case "6": drawDispersingSnowChar(ctx, od.c, od.x, snowProgress, snowSeed(animTransition, s, slot + 8, od.x)); break;
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
            // 업로드 시작 전 모든 배지 초기화 (어둡게)
            UNIQ_CHARS.forEach(c => document.getElementById('b_'+c).classList.remove('active'));
            
            const tC = document.createElement('canvas'); tC.width = 64; tC.height = 64;
            const tX = tC.getContext('2d');
            for(let i=0; i<UNIQ_CHARS.length; i++) {
                const char = UNIQ_CHARS[i]; els.status.innerText = `업로드: ${char} (${i+1}/${UNIQ_CHARS.length})`;
                tX.fillStyle = "#000"; tX.fillRect(0,0,64,64); tX.fillStyle = "#fff"; tX.font = `${els.sIn.value}px ClockFont`;
                tX.textAlign = "center"; tX.textBaseline = "middle"; tX.fillText(char, 32, 32);
                const data = tX.getImageData(0,0,64,64).data, bm = new Uint8Array(512);
                for(let y=0; y<64; y++) for(let x=0; x<8; x++) {
                    let b = 0; for(let bit=0; bit<8; bit++) if(data[(y*64+(x*8+bit))*4]>128) b|=(1<<(7-bit));
                    bm[y*8+x]=b;
                }
                let hex = ""; new TextEncoder().encode(char).forEach(b => hex += b.toString(16).toUpperCase().padStart(2, '0'));
                const fd = new FormData(); fd.append('file', new Blob([bm]), `c_${hex}.bin`);
                
                // 해당 글자 배지 강조 (업로드 시도 시점)
                document.getElementById('b_'+char).classList.add('active');
                
                await fetch(`/upload?slot=${els.slot.value}`, { method: 'POST', body: fd });
                els.pFill.style.width = ((i+1)/UNIQ_CHARS.length * 100) + "%";
            }
            await fetch('/api/refresh_cache', { method: 'POST' });
            if (els.fIn.files[0]) await fetch('/api/config', { method: 'POST', body: JSON.stringify({ font_name: els.fIn.files[0].name }) });
            els.status.innerText = "전체 업로드 완료!"; els.apply.disabled = false;
        }
    </script>
</body>
</html>
)rawliteral";

#endif
