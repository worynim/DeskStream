#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>

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
        .preview-target { display: flex; gap: 0; border: 1px solid #444; padding: 0; overflow: hidden; border-radius: 4px; }
        .char-preview { width: 32px; height: 64px; background: #000; border: none; image-rendering: pixelated; }

        .progress-wrap { width: 100%; height: 6px; background: rgba(255,255,255,0.1); border-radius: 3px; margin: 20px 0; display: none; overflow: hidden; }
        .progress-fill { height: 100%; width: 0%; background: var(--primary); transition: width 0.2s; }
        .status-msg { text-align: center; font-size: 0.85rem; color: var(--primary); min-height: 1.2rem; }
        
        .btn-row { display: grid; grid-template-columns: 1fr 2fr; gap: 15px; margin-top: 25px; }
        button { padding: 15px; border-radius: 12px; border: none; font-weight: 600; cursor: pointer; transition: 0.2s; }
        .btn-apply { background: var(--primary); color: #000; }
        .btn-apply:hover { box-shadow: 0 0 15px var(--primary); transform: translateY(-2px); }
        .btn-apply:disabled { opacity: 0.3; cursor: not-allowed; transform: none; }
        .btn-reset { background: rgba(255,255,255,0.05); color: #fff; border: 1px solid rgba(255,255,255,0.1); }
        .btn-reset:hover { background: rgba(255,60,60,0.2); border-color: #f55; }

        .inventory { display: flex; flex-wrap: wrap; gap: 4px; margin-top: 25px; justify-content: center; }
        .badge { width: 28px; height: 28px; display: flex; align-items: center; justify-content: center; font-size: 0.75rem; background: rgba(255,255,255,0.03); border-radius: 4px; color: #555; border: 1px solid transparent; }
        .badge.active { color: var(--primary); border-color: rgba(0,242,254,0.3); background: rgba(0,242,254,0.05); }
        select { background: rgba(0,0,0,0.3); border: 1px solid rgba(255,255,255,0.1); border-radius: 10px; padding: 10px; color: #fff; width: 100%; cursor: pointer; outline: none; }
        select:focus { border-color: var(--primary); }
    </style>
</head>
<body>
    <div class="glass">
        <h1>Korean Clock Font Studio</h1>
        <p class="desc">낱자 기반 렌더링 시스템 (32x64 가로형 인코딩)</p>
        
        <div class="setup-grid">
            <div class="field">
                <label>1. 폰트 파일 선택 (.ttf, .otf)</label>
                <input type="file" id="fIn" accept=".ttf,.otf">
            </div>
            <div class="field">
                <label>2. 낱자 크기: <span id="sVal">48</span>px</label>
                <input type="range" id="sIn" min="20" max="60" value="48">
            </div>
            <div class="field">
                <label>3. 애니메이션 모드 <small>(BTN3 short)</small></label>
                <select id="animMode">
                    <option value="0">Mode 1: None (정적)</option>
                    <option value="1" selected>Mode 2: Scroll Up (올라오기)</option>
                </select>
            </div>
        </div>

        <div class="setup-grid">
            <div class="field">
                <label>4. 표시 유형 <small>(BTN2 short)</small></label>
                <select id="displayMode">
                    <option value="0">한글 (열두시 삼십분)</option>
                    <option value="1">숫자 (12시 30분)</option>
                </select>
            </div>
            <div class="field">
                <label>5. 시간 형식 <small>(BTN2 long)</small></label>
                <select id="hourFormat">
                    <option value="0">12시간제 (오전/오후)</option>
                    <option value="1">24시간제 (영시~이십삼시)</option>
                </select>
            </div>
            <div class="field">
                <label>6. 정시 시보 <small>(BTN1 short)</small></label>
                <select id="chime">
                    <option value="0">OFF</option>
                    <option value="1">ON</option>
                </select>
            </div>
            <div class="field">
                <label>7. 화면 반전 (Flip) <small>(BTN1 long)</small></label>
                <select id="flipMode">
                    <option value="0">NORMAL (정순)</option>
                    <option value="1">FLIP (180도 회전)</option>
                </select>
            </div>
        </div>

        <style>
            .preview-list { display: flex; flex-direction: row; flex-wrap: wrap; gap: 10px; margin: 15px 0; justify-content: center; }
            .preview-row { display: flex; flex-direction: column; align-items: center; gap: 8px; background: rgba(0,0,0,0.4); padding: 12px 8px; border-radius: 12px; border: 1px solid #333; width: 145px; }
            .preview-label { width: 100%; font-size: 0.7rem; color: #888; text-align: center; }
        </style>

        <div class="preview-list">
            <div class="preview-row">
                <div class="preview-label">오전/후</div>
                <div class="preview-target">
                    <canvas id="p0" class="screen-preview" width="128" height="64"></canvas>
                </div>
            </div>
            <div class="preview-row">
                <div class="preview-label">시</div>
                <div class="preview-target">
                    <canvas id="p1" class="screen-preview" width="128" height="64"></canvas>
                </div>
            </div>
            <div class="preview-row">
                <div class="preview-label">분</div>
                <div class="preview-target">
                    <canvas id="p2" class="screen-preview" width="128" height="64"></canvas>
                </div>
            </div>
            <div class="preview-row">
                <div class="preview-label">초</div>
                <div class="preview-target">
                    <canvas id="p3" class="screen-preview" width="128" height="64"></canvas>
                </div>
            </div>
        </div>

        <div class="progress-wrap" id="pWrap">
            <div class="progress-fill" id="pFill"></div>
        </div>
        <div id="status" class="status-msg">시작하려면 폰트를 불러오세요.</div>

        <div class="btn-row">
            <button class="btn-apply" id="apply" onclick="processAll()" disabled>시계에 개별 폰트 업로드</button>
        </div>

        <div class="inventory" id="inv"></div>
    </div>

    <script>
        const UNIQ_CHARS = "오전후한시두세네다섯여일곱덟아홉열영이삼사육칠팔구십분초0123456789".split("");
        const inv = document.getElementById('inv');
        const fIn = document.getElementById('fIn');
        const sIn = document.getElementById('sIn');
        const apply = document.getElementById('apply');
        const status = document.getElementById('status');
        const pFill = document.getElementById('pFill');
        const pWrap = document.getElementById('pWrap');

        // 각 화면별 128x64 캔버스 초기화
        const pCtx = [];
        for(let s=0; s<4; s++) {
            pCtx[s] = document.getElementById(`p${s}`).getContext('2d');
        }

        let fontLoaded = false;
        const FONT_NAME = "ClockFontV2";

        UNIQ_CHARS.forEach(c => {
            const d = document.createElement('div');
            d.className = 'badge';
            d.id = 'b_' + c;
            d.innerText = c;
            inv.appendChild(d);
        });

        // 애니메이션 시뮬레이션 상태
        let currentStrings = ["", "", "", ""];
        let targetStrings = ["", "", "", ""];
        let animProgress = [1, 1, 1, 1]; // 0 to 1
        const ANIMATION_DURATION = 8 * 20; // 8 steps * 20ms = 160ms (기기 설정과 동기화)

        const animModeEl = document.getElementById('animMode');
        const dispModeEl = document.getElementById('displayMode');
        const hourFormEl = document.getElementById('hourFormat');
        
        let isInteraction = false; // 사용자 조작 중 폴링 중단용

        // 기기 설정 불러오기
        async function fetchConfig() {
            if (isInteraction) return; // 조작 중이면 업데이트 건너뜀
            try {
                const res = await fetch('/api/config');
                const data = await res.json();
                animModeEl.value = data.anim_mode;
                dispModeEl.value = data.display_mode;
                hourFormEl.value = data.hour_format;
                document.getElementById('chime').value = data.chime_enabled ? "1" : "0";
                document.getElementById('flipMode').value = data.is_flipped ? "1" : "0";
            } catch(e) { console.error("Config fetch failed", e); }
        }
        
        // 3초마다 기기 상태를 확인하여 웹 UI 동기화 (실시간성 확보)
        setInterval(fetchConfig, 3000);
        fetchConfig();

        async function saveConfig() {
            isInteraction = true; // 조작 시작
            try {
                await fetch('/api/config', {
                    method: 'POST',
                    body: JSON.stringify({ 
                        anim_mode: parseInt(animModeEl.value),
                        display_mode: parseInt(dispModeEl.value),
                        hour_format: parseInt(hourFormEl.value),
                        chime_enabled: document.getElementById('chime').value == "1",
                        is_flipped: document.getElementById('flipMode').value == "1"
                    })
                });
            } finally {
                // 조작 완료 후 1초 뒤에 다시 폴링 허용
                setTimeout(() => { isInteraction = false; }, 1000);
            }
        }
        animModeEl.onchange = saveConfig;
        dispModeEl.onchange = saveConfig;
        hourFormEl.onchange = saveConfig;
        document.getElementById('chime').onchange = saveConfig;
        document.getElementById('flipMode').onchange = saveConfig;
        function getKoreanTimeStrings() {
            const now = new Date();
            let h = now.getHours();
            let m = now.getMinutes();
            let s = now.getSeconds();
            let d = now.getDate();

            const isHangul = parseInt(dispModeEl.value) === 1;
            const is24H = parseInt(hourFormEl.value) === 2;

            function toHangulNum(num, unit) {
                if (num === 0) return "영" + unit;
                const tList = ["", "십", "이십", "삼십", "사십", "오십"];
                const nList = ["", "일", "이", "삼", "사", "오", "육", "칠", "팔", "구"];
                return tList[Math.floor(num / 10)] + nList[num % 10] + unit;
            }

            function toNumericNum(num, unit) {
                return num.toString().padStart(2, '0') + unit;
            }

            function getHangulHour(h, is24h) {
                let hr = is24h ? h : (h % 12);
                if (!is24h && hr === 0) hr = 12;
                if (is24h && hr === 0) return "영시";
                const h_ones = ["", "한", "두", "세", "네", "다섯", "여섯", "일곱", "여덟", "아홉", "열", "열한", "열두"];
                if (hr <= 12) return h_ones[hr] + "시";
                if (hr < 20) return "열" + h_ones[hr-10] + "시";
                if (hr === 20) return "스무시";
                return "스물" + h_ones[hr-20] + "시";
            }

            // Screen 0: AM/PM or Date
            let s0 = "";
            if (is24H) {
                s0 = isHangul ? toHangulNum(d, "일") : d + "일";
            } else {
                s0 = h < 12 ? "오전" : "오후";
            }

            // Screen 1: Hour
            let s1 = isHangul ? getHangulHour(h, is24H) : (is24H ? h : (h % 12 || 12)) + "시";

            // Screen 2, 3: Min, Sec
            let s2 = isHangul ? toHangulNum(m, "분") : toNumericNum(m, "분");
            let s3 = isHangul ? toHangulNum(s, "초") : toNumericNum(s, "초");

            return [s0, s1, s2, s3];
        }

        fIn.onchange = async (e) => {
            const file = e.target.files[0];
            if(!file) return;
            status.innerText = "폰트 로딩 중...";
            const buffer = await file.arrayBuffer();
            const font = new FontFace(FONT_NAME, buffer);
            try {
                await font.load();
                document.fonts.add(font);
                fontLoaded = true;
                apply.disabled = false;
                status.innerText = "폰트 준비됨. 아래 버튼으로 각 화면을 미리보세요.";
                // updatePreview() 대신 render 루프가 자동 반영함
            } catch(err) {
                status.innerText = "폰트 로드 실패: " + err;
                console.error(err);
            }
        };

        sIn.oninput = () => {
            document.getElementById('sVal').innerText = sIn.value;
            // updatePreview() 대신 render 루프가 자동 반영함
        };

        function updateDisplays() {
            const timeStrings = getKoreanTimeStrings();
            const mode = parseInt(animModeEl.value);

            for(let s=0; s<4; s++) {
                if(timeStrings[s] !== targetStrings[s]) {
                    if (mode === 1) { // None
                        currentStrings[s] = timeStrings[s];
                        targetStrings[s] = timeStrings[s];
                        animProgress[s] = 1;
                    } else { // Scroll Up
                        currentStrings[s] = targetStrings[s];
                        targetStrings[s] = timeStrings[s];
                        animProgress[s] = 0;
                    }
                }
            }
        }

        function getCharPositions(screenIdx, text) {
            const chars = Array.from(text);
            const positions = [];
            if (screenIdx === 0) {
                const totalW = chars.length * 32;
                const startX = (128 - totalW) / 2;
                chars.forEach((c, i) => positions.push({ c, x: startX + (i * 32) }));
            } else {
                const numChars = chars.length - 1;
                if (chars.length > 0) {
                    const unitX = 96;
                    const startX = numChars > 0 ? (96 - (numChars * 32)) / 2 : 0;
                    chars.forEach((c, i) => {
                        if (i === chars.length - 1) positions.push({ c, x: unitX });
                        else positions.push({ c, x: startX + (i * 32) });
                    });
                }
            }
            return positions;
        }

        function drawChar(ctx, char, x, yOffset) {
            ctx.fillStyle = "#fff";
            ctx.font = `${sIn.value}px ${FONT_NAME}`;
            ctx.textAlign = "center";
            ctx.textBaseline = "middle";
            ctx.fillText(char, x + 16, 32 + yOffset);
        }

        let lastTime = 0;
        function render(time) {
            const dt = time - lastTime;
            lastTime = time;

            updateDisplays();

            const mode = parseInt(animModeEl.value);

            for(let s=0; s<4; s++) {
                const ctx = pCtx[s];
                ctx.fillStyle = "#000";
                ctx.fillRect(0,0,128,64);
                if (!fontLoaded) continue;

                const oldData = getCharPositions(s, currentStrings[s]);
                const newData = getCharPositions(s, targetStrings[s]);

                if (animProgress[s] < 1) {
                    animProgress[s] = Math.min(1, animProgress[s] + dt / ANIMATION_DURATION);
                    const offset = animProgress[s] * 64;

                    newData.forEach(nc => {
                        const oc = oldData.find(o => o.x === nc.x);
                        if (oc && oc.c === nc.c) {
                            drawChar(ctx, nc.c, nc.x, 0);
                        } else {
                            if (oc && animProgress[s] < 1) drawChar(ctx, oc.c, nc.x, -offset);
                            drawChar(ctx, nc.c, nc.x, 64 - offset);
                        }
                    });
                    
                    // 사라지는 글자 처리
                    oldData.forEach(oc => {
                        if (!newData.find(n => n.x === oc.x) && animProgress[s] < 1) {
                            drawChar(ctx, oc.c, oc.x, -offset);
                        }
                    });
                } else {
                    newData.forEach(nc => drawChar(ctx, nc.c, nc.x, 0));
                }
            }
            requestAnimationFrame(render);
        }
        requestAnimationFrame(render);

        async function processAll() {
            if(!fontLoaded) return;
            apply.disabled = true;
            status.innerText = "업로드 준비 중...";
            
            pWrap.style.display = "block";
            
            // 낱자 렌더링용 임시 캔버스
            const tC = document.createElement('canvas');
            tC.width = 32; tC.height = 64;
            const tX = tC.getContext('2d');

            for(let i=0; i<UNIQ_CHARS.length; i++) {
                const char = UNIQ_CHARS[i];
                status.innerText = `업로드 중: ${char} (${i+1}/${UNIQ_CHARS.length})`;
                
                tX.fillStyle = "#000";
                tX.fillRect(0,0,32,64);
                tX.fillStyle = "#fff";
                tX.font = `${sIn.value}px ${FONT_NAME}`;
                tX.textAlign = "center";
                tX.textBaseline = "middle";
                tX.fillText(char, 16, 32);

                const data = tX.getImageData(0,0,32,64).data;
                const bm = new Uint8Array(256); // (32/8) * 64 = 4 * 64 = 256 bytes

                // Horizontal (MSB) Encoding
                for(let y=0; y<64; y++) {
                    for(let x=0; x<4; x++) { // 4 bytes per row
                        let b = 0;
                        for(let bit=0; bit<8; bit++) {
                            const idx = (y * 32 + (x * 8 + bit)) * 4;
                            if(data[idx] > 128) b |= (1 << (7 - bit));
                        }
                        bm[y * 4 + x] = b;
                    }
                }

                // UTF-8 Hex Filename (Match C++ sprintf %02X)
                let hex = "";
                const encoder = new TextEncoder();
                const bytes = encoder.encode(char);
                bytes.forEach(b => hex += b.toString(16).toUpperCase().padStart(2, '0'));
                
                const fd = new FormData();
                fd.append('file', new Blob([bm]), `c_${hex}.bin`);
                await fetch('/upload', { method: 'POST', body: fd });
                
                pFill.style.width = ((i+1)/UNIQ_CHARS.length * 100) + "%";
                document.getElementById('b_'+char).classList.add('active');
            }
            // 업로드 완료 후 기기에 캐시 갱신 요청
            await fetch('/api/refresh_cache', { method: 'POST' });
            
            status.innerText = "낱자 폰트 세트 적용 및 메모리 캐시 갱신 완료!";
            apply.disabled = false;
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
        json += "\"anim_mode\":" + String(display.anim_mode) + ",";
        json += "\"display_mode\":" + String(display.display_mode) + ",";
        json += "\"hour_format\":" + String(display.hour_format) + ",";
        json += "\"chime_enabled\":" + String(display.chime_enabled ? "true" : "false") + ",";
        json += "\"is_flipped\":" + String(display.is_flipped ? "true" : "false");
        json += "}";
        server.send(200, "application/json", json);
    });

    server.on("/api/config", HTTP_POST, []() {
        if (server.hasArg("plain")) {
            String body = server.arg("plain");
            extern unsigned long forceUpdateTrigger;
            
            // [설정 동기화] 브라우저(0/1) 값을 기기 상수와 맞춤 (v1.3.48)
            // 1. 애니메이션 모드
            if (body.indexOf("\"anim_mode\":0") != -1) display.setAnimMode(ANIMATION_TYPE_NONE);
            else if (body.indexOf("\"anim_mode\":1") != -1) display.setAnimMode(ANIMATION_TYPE_SCROLL_UP);
            
            // 2. 표시 유형
            if (body.indexOf("\"display_mode\":0") != -1) display.setDisplayMode(CLOCK_MODE_HANGUL);
            else if (body.indexOf("\"display_mode\":1") != -1) display.setDisplayMode(CLOCK_MODE_NUMERIC);
            
            // 3. 시간 형식
            if (body.indexOf("\"hour_format\":0") != -1) display.setHourFormat(HOUR_FORMAT_12H);
            else if (body.indexOf("\"hour_format\":1") != -1) display.setHourFormat(HOUR_FORMAT_24H);

            // 4. 시보 기능
            if (body.indexOf("\"chime_enabled\":true") != -1) display.setChime(true);
            else if (body.indexOf("\"chime_enabled\":false") != -1) display.setChime(false);
            
            // 5. 플립 모드
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
        forceUpdateTrigger = 1; // 폰트 변경 즉시 화면 반영
        server.send(200, "text/plain", "OK");
    });

    server.begin();
    Serial.println("[WEB] Font Studio started");
}

#endif
