/**
 * @file web_pages.h
 * @brief 웹 설정 대시보드 및 폰트 스튜디오 리소스
 * @details HTML, CSS, JavaScript 등으로 구성된 임베디드 웹 페이지 리소스 관리 (PROGMEM 활용)
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
            fIn: document.getElementById('fIn')
        };

        UNIQ_CHARS.forEach(c => {
            const d = document.createElement('div');
            d.className = 'badge'; d.id = 'b_' + c; d.innerText = c;
            document.getElementById('inv').appendChild(d);
        });

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
                font_slot: parseInt(els.slot.value)
            };
            try {
                await fetch('/api/config', { method: 'POST', body: JSON.stringify(body) });
                els.status.innerText = "설정 저장됨";
            } catch(e) { els.status.innerText = "저장 실패"; }
        }

        [els.anim, els.disp, els.hour, els.chime, els.flip, els.invert, els.slot].forEach(el => el.onchange = saveConfig);
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

        let lastTimeStrings = ["", "", "", ""], targetTimeStrings = ["", "", "", ""], animStep = 16; 
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

        function render() {
            const currentTimeStrings = getHangeulTimeStrings();
            if (targetTimeStrings[0] === "") targetTimeStrings = [...currentTimeStrings];
            let changed = currentTimeStrings.some((s, i) => s !== targetTimeStrings[i]);
            if (changed && animStep >= 16) {
                lastTimeStrings = [...targetTimeStrings]; targetTimeStrings = [...currentTimeStrings];
                animStep = (["1", "2", "3", "4", "5"].includes(els.anim.value)) ? 0 : 16;
            }
            if (animStep < 16) animStep++; 
            for(let s=0; s<4; s++) {
                const ctx = pCtx[s]; const isInverted = els.invert.value === "1";
                ctx.fillStyle = isInverted ? "#fff" : "#000"; ctx.fillRect(0,0,128,64);
                const isC = (s === 0) || (targetTimeStrings[s] === "정각");
                const curD = getCharPositions(targetTimeStrings[s], isC);
                if (animStep >= 16 || els.anim.value === "0") curD.forEach(d => drawChar(ctx, d.c, d.x, 0));
                else {
                    const isOC = (s === 0) || (lastTimeStrings[s] === "정각");
                    const off = animStep * 4, oldD = getCharPositions(lastTimeStrings[s], isOC), mode = els.anim.value;
                    curD.forEach(nd => {
                        let od = oldD.find(o => o.x === nd.x);
                        if (od && od.c === nd.c) drawChar(ctx, nd.c, nd.x, 0);
                        else {
                            switch(mode) {
                                case "1": if(od) drawChar(ctx, od.c, nd.x, -off); drawChar(ctx, nd.c, nd.x, 64-off); break;
                                case "2": if(od) drawChar(ctx, od.c, nd.x, off); drawChar(ctx, nd.c, nd.x, -64+off); break;
                                case "3": if(animStep<=8) { if(od) drawScaledChar(ctx, od.c, nd.x, ((8-animStep)/8)*64); } else drawScaledChar(ctx, nd.c, nd.x, ((animStep-8)/8)*64); break;
                                case "4": ctx.save(); if(animStep<=8) { ctx.globalAlpha=(8-animStep)/8; if(od) drawChar(ctx, od.c, nd.x, 0); } else { ctx.globalAlpha=(animStep-8)/8; drawChar(ctx, nd.c, nd.x, 0); } ctx.restore(); break;
                                case "5": if(animStep<=8) { if(od) drawZoomedChar(ctx, od.c, nd.x, (8-animStep)/8); } else { let sc = (animStep<=12)?((animStep-8)*1.5/4):(1.5-(animStep-12)*0.5/4); drawZoomedChar(ctx, nd.c, nd.x, sc); } break;
                            }
                        }
                    });
                    oldD.forEach(od => {
                        if (!curD.find(nd => nd.x === od.x)) {
                            switch(mode) {
                                case "1": drawChar(ctx, od.c, od.x, -off); break;
                                case "2": drawChar(ctx, od.c, od.x, off); break;
                                case "3": if(animStep<=8) drawScaledChar(ctx, od.c, od.x, ((8-animStep)/8)*64); break;
                                case "4": if(animStep<=8) { ctx.save(); ctx.globalAlpha=(8-animStep)/8; drawChar(ctx, od.c, od.x, 0); ctx.restore(); } break;
                                case "5": if(animStep<=8) drawZoomedChar(ctx, od.c, od.x, (8-animStep)/8); break;
                            }
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
                await fetch(`/upload?slot=${els.slot.value}`, { method: 'POST', body: fd });
                els.pFill.style.width = ((i+1)/UNIQ_CHARS.length * 100) + "%";
                document.getElementById('b_'+char).classList.add('active');
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
