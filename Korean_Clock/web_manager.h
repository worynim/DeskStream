#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "display_manager.h"
#include "config.h"

extern WebServer server;
extern void startWebServer();

// === 고전/현대 조화 Font Studio HTML ===
const char* font_studio_html = R"rawliteral(
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
        .preview-pane { background: #000; border: 2px solid #333; border-radius: 12px; height: 100px; display: flex; align-items: center; justify-content: center; overflow: hidden; margin: 15px 0; }
        
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
    </style>
</head>
<body>
    <div class="glass">
        <h1>Korean Clock Font Studio</h1>
        <p class="desc">낱자 기반 64px 엔진 (0-Based Unified Index)</p>
        
        <div class="setup-grid">
            <div class="field">
                <label>1. 폰트 선택 (.ttf, .otf)</label>
                <input type="file" id="fIn" accept=".ttf,.otf">
            </div>
            <div class="field">
                <label>2. 낱자 크기: <span id="sVal">48</span>px</label>
                <input type="range" id="sIn" min="20" max="60" value="48">
            </div>
        </div>

        <div class="setup-grid">
            <div class="field">
                <label>3. 애니메이션 (BTN3 Short)</label>
                <select id="animMode">
                    <option value="0">OFF (정적)</option>
                    <option value="1">Scroll Up (상단 스크롤)</option>
                </select>
            </div>
            <div class="field">
                <label>4. 표시 유형 (BTN2 Short)</label>
                <select id="displayMode">
                    <option value="0">한글 (열두시 삼십분)</option>
                    <option value="1">숫자 (12:30:45)</option>
                </select>
            </div>
            <div class="field">
                <label>5. 시간 형식 (BTN2 Long)</label>
                <select id="hourFormat">
                    <option value="0">12시간제 (오전/오후)</option>
                    <option value="1">24시간제 (0~23시)</option>
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
                    <option value="0">NORMAL (정순)</option>
                    <option value="1">FLIP (반전)</option>
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
    </div>

    <script>
        const UNIQ_CHARS = "오전후한시두세네다섯여일곱덟아홉열영이삼사육칠팔구십분초0123456789".split("");
        const els = {
            anim: document.getElementById('animMode'),
            disp: document.getElementById('displayMode'),
            hour: document.getElementById('hourFormat'),
            chime: document.getElementById('chime'),
            flip: document.getElementById('flipMode'),
            status: document.getElementById('status'),
            apply: document.getElementById('apply'),
            pFill: document.getElementById('pFill'),
            pWrap: document.getElementById('pWrap')
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
                console.log("Config Received:", data);
                
                // 0-based 인덱스 매핑 (문자열 변환 필수)
                els.anim.value = (data.anim_mode ?? 1).toString();
                els.disp.value = (data.display_mode ?? 0).toString();
                els.hour.value = (data.hour_format ?? 0).toString();
                els.chime.value = data.chime_enabled ? "1" : "0";
                els.flip.value = data.is_flipped ? "1" : "0";
                
                els.status.innerText = "설정 동기화 완료 (0-Based)";
            } catch(e) { 
                els.status.innerText = "설정 로드 실패";
                console.error(e); 
            }
        }

        async function saveConfig() {
            els.status.innerText = "저장 중...";
            const body = {
                anim_mode: parseInt(els.anim.value),
                display_mode: parseInt(els.disp.value),
                hour_format: parseInt(els.hour.value),
                chime_enabled: els.chime.value === "1",
                is_flipped: els.flip.value === "1"
            };
            try {
                await fetch('/api/config', { method: 'POST', body: JSON.stringify(body) });
                els.status.innerText = "설정 저장됨";
            } catch(e) { els.status.innerText = "저장 실패"; }
        }

        [els.anim, els.disp, els.hour, els.chime, els.flip].forEach(el => el.onchange = saveConfig);
        fetchConfig();
        setInterval(fetchConfig, 5000);

        // --- 시뮬레이션 및 폰트 엔진 ---
        const fIn = document.getElementById('fIn');
        const sIn = document.getElementById('sIn');
        const pCtx = [0,1,2,3].map(i => document.getElementById(`p${i}`).getContext('2d'));
        let fontLoaded = false;

        fIn.onchange = async (e) => {
            const file = e.target.files[0]; if(!file) return;
            const buffer = await file.arrayBuffer();
            const font = new FontFace("ClockFont", buffer);
            await font.load(); document.fonts.add(font);
            fontLoaded = true; els.apply.disabled = false;
            els.status.innerText = "폰트 준비됨. 미리보기를 확인하세요.";
        };
        sIn.oninput = () => { document.getElementById('sVal').innerText = sIn.value; };

        function drawChar(ctx, char, x, yOffset) {
            ctx.fillStyle = "#fff"; ctx.font = `${sIn.value}px ClockFont`;
            ctx.textAlign = "center"; ctx.textBaseline = "middle";
            ctx.fillText(char, x + 16, 32 + yOffset);
        }

        function getKoreanTimeStrings() {
            const now = new Date();
            let h = now.getHours(), m = now.getMinutes(), s = now.getSeconds(), d = now.getDate();
            const isHangul = els.disp.value === "0";
            const is24H = els.hour.value === "1";

            const toHangulNum = (num, unit) => {
                if (num === 0) return "영" + unit;
                const tList = ["", "십", "이십", "삼십", "사십", "오십"], nList = ["", "일", "이", "삼", "사", "오", "육", "칠", "팔", "구"];
                return tList[Math.floor(num / 10)] + nList[num % 10] + unit;
            };
            const toNumericNum = (num, unit) => num.toString().padStart(2, '0') + unit;
            const getHangulHour = (h, is24h) => {
                let hr = is24h ? h : (h % 12 || 12);
                if (is24h && hr === 0) return "영시";
                const h_ones = ["", "한", "두", "세", "네", "다섯", "여섯", "일곱", "여덟", "아홉", "열", "열한", "열두"];
                if (hr <= 12) return h_ones[hr] + "시";
                if (hr < 20) return "열" + h_ones[hr-10] + "시";
                if (hr === 20) return "스무시";
                return "스물" + h_ones[hr-20] + "시";
            };

            let s0 = is24H ? (isHangul ? toHangulNum(d, "일") : d + "일") : (h < 12 ? "오전" : "오후");
            let s1 = isHangul ? getHangulHour(h, is24H) : toNumericNum(is24H ? h : (h % 12 || 12), "시");
            let s2 = isHangul ? toHangulNum(m, "분") : toNumericNum(m, "분");
            let s3 = isHangul ? toHangulNum(s, "초") : toNumericNum(s, "초");
            return [s0, s1, s2, s3];
        }

        function render() {
            const timeStrings = getKoreanTimeStrings();
            const isFlipped = els.flip.value === "1";
            for(let s=0; s<4; s++) {
                const ctx = pCtx[s]; ctx.fillStyle = "#000"; ctx.fillRect(0,0,128,64);
                if (!fontLoaded) continue;
                const text = isFlipped ? timeStrings[3-s] : timeStrings[s];
                const chars = Array.from(text);
                const isCentered = isFlipped ? (s === 3) : (s === 0);
                let startX = (isCentered) ? (128 - chars.length * 32) / 2 : (96 - (chars.length - 1) * 32) / 2;
                chars.forEach((c, i) => {
                    let xPos = (isCentered || i < chars.length - 1) ? (startX + i * 32) : 96;
                    drawChar(ctx, c, xPos, 0);
                });
            }
            requestAnimationFrame(render);
        }
        render();

        async function processAll() {
            els.apply.disabled = true; els.pWrap.style.display = "block";
            const tC = document.createElement('canvas'); tC.width = 64; tC.height = 64;
            const tX = tC.getContext('2d');

            for(let i=0; i<UNIQ_CHARS.length; i++) {
                const char = UNIQ_CHARS[i];
                els.status.innerText = `업로드: ${char} (${i+1}/${UNIQ_CHARS.length})`;
                tX.fillStyle = "#000"; tX.fillRect(0,0,64,64);
                tX.fillStyle = "#fff"; tX.font = `${sIn.value}px ClockFont`;
                tX.textAlign = "center"; tX.textBaseline = "middle"; tX.fillText(char, 32, 32);

                const data = tX.getImageData(0,0,64,64).data, bm = new Uint8Array(512);
                for(let y=0; y<64; y++) {
                    for(let x=0; x<8; x++) {
                        let b = 0;
                        for(let bit=0; bit<8; bit++) {
                            const idx = (y * 64 + (x * 8 + bit)) * 4;
                            if(data[idx] > 128) b |= (1 << (7 - bit));
                        }
                        bm[y * 8 + x] = b;
                    }
                }
                let hex = ""; const bytes = new TextEncoder().encode(char);
                bytes.forEach(b => hex += b.toString(16).toUpperCase().padStart(2, '0'));
                const fd = new FormData(); fd.append('file', new Blob([bm]), `c_${hex}.bin`);
                await fetch('/upload', { method: 'POST', body: fd });
                els.pFill.style.width = ((i+1)/UNIQ_CHARS.length * 100) + "%";
                document.getElementById('b_'+char).classList.add('active');
            }
            await fetch('/api/refresh_cache', { method: 'POST' });
            els.status.innerText = "전체 업로드 완료!"; els.apply.disabled = false;
        }
    </script>
</body>
</html>
)rawliteral";

void startWebServer() {
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", font_studio_html);
    });

    server.on("/upload", HTTP_POST, []() {
        server.send(200, "text/plain", "OK");
    }, []() {
        HTTPUpload& upload = server.upload();
        static File fsUploadFile;
        if (upload.status == UPLOAD_FILE_START) {
            String filename = "/" + upload.filename;
            fsUploadFile = LittleFS.open(filename, "w");
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
        } else if (upload.status == UPLOAD_FILE_END) {
            if (fsUploadFile) fsUploadFile.close();
        }
    });

    server.on("/api/config", HTTP_GET, []() {
        String json = "{";
        json += "\"anim_mode\":" + String((int)display.anim_mode) + ",";
        json += "\"display_mode\":" + String((int)display.display_mode) + ",";
        json += "\"hour_format\":" + String((int)display.hour_format) + ",";
        json += "\"chime_enabled\":" + String(display.chime_enabled ? "true" : "false") + ",";
        json += "\"is_flipped\":" + String(display.is_flipped ? "true" : "false");
        json += "}";
        server.send(200, "application/json", json);
    });

    server.on("/api/config", HTTP_POST, []() {
        if (server.hasArg("plain")) {
            String body = server.arg("plain");
            extern unsigned long forceUpdateTrigger;
            
            auto parseNum = [&](String key) {
                int pos = body.indexOf("\"" + key + "\":");
                if (pos == -1) return -1;
                int start = pos + key.length() + 3;
                while (start < body.length() && (body[start] == ' ' || body[start] == ':' || body[start] == '\"')) start++;
                if (start < body.length() && isDigit(body[start])) return (int)(body[start] - '0');
                return -1;
            };

            int am = parseNum("anim_mode");
            if (am != -1) display.setAnimMode((uint8_t)am);

            int dm = parseNum("display_mode");
            if (dm != -1) display.setDisplayMode((uint8_t)dm);

            int hf = parseNum("hour_format");
            if (hf != -1) display.setHourFormat((uint8_t)hf);

            if (body.indexOf("\"chime_enabled\":true") != -1) display.setChime(true);
            else if (body.indexOf("\"chime_enabled\":false") != -1) display.setChime(false);

            if (body.indexOf("\"is_flipped\":true") != -1) display.setFlipDisplay(true);
            else if (body.indexOf("\"is_flipped\":false") != -1) display.setFlipDisplay(false);

            forceUpdateTrigger = 1;
            server.send(200, "text/plain", "OK");
        } else {
            server.send(400, "text/plain", "Bad Request");
        }
    });

    server.on("/api/refresh_cache", HTTP_POST, []() {
        display.loadBitmapCache();
        extern unsigned long forceUpdateTrigger;
        forceUpdateTrigger = 1;
        server.send(200, "text/plain", "OK");
    });

    server.begin();
    Serial.println("[WEB] Font Studio started (0-Based Path)");
}

#endif
