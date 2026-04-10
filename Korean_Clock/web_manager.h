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
                <label>2. 낱자 크기: <span id="sVal">48</span>px (최대 32px 폭 권장)</label>
                <input type="range" id="sIn" min="20" max="60" value="48">
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
            <button class="btn-reset" onclick="resetAll()">초기화</button>
            <button class="btn-apply" id="apply" onclick="processAll()" disabled>시계에 개별 낱자 업로드</button>
        </div>

        <div class="inventory" id="inv"></div>
    </div>

    <script>
        const UNIQ_CHARS = "오전후한시두세네다섯여일곱덟아홉열영이삼사육칠팔구십분초".split("");
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

        // 한글 시간 변환 로직 (JS 버전)
        function getKoreanTimeStrings() {
            const now = new Date();
            let h = now.getHours();
            let m = now.getMinutes();
            let s = now.getSeconds();

            const ampm = h < 12 ? "오전" : "오후";
            if (h > 12) h -= 12;
            if (h == 0) h = 12;

            const hList = ["", "한시", "두시", "세시", "네시", "다섯시", "여섯시", "일곱시", "여덟시", "아홉시", "열시", "열한시", "열두시"];
            const tList = ["", "십", "이십", "삼십", "사십", "오십"];
            const nList = ["", "일", "이", "삼", "사", "오", "육", "칠", "팔", "구"];

            function convert(num, unit) {
                if (num === 0) return "영" + unit;
                let res = tList[Math.floor(num / 10)] + nList[num % 10] + unit;
                return res;
            }

            return [ampm, hList[h], convert(m, "분"), convert(s, "초")];
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
                updatePreview();
            } catch(err) {
                status.innerText = "폰트 로드 실패: " + err;
            }
        };

        sIn.oninput = () => {
            document.getElementById('sVal').innerText = sIn.value;
            updatePreview();
        };

        function updatePreview() {
            if(!fontLoaded) return;
            const timeStrings = getKoreanTimeStrings(); // [ampm, hStr, mStr, sStr]
            
            for(let s=0; s<4; s++) { // 4개 화면 순회
                const ctx = pCtx[s];
                const text = timeStrings[s] || "";
                const chars = Array.from(text);
                
                ctx.fillStyle = "#000";
                ctx.fillRect(0,0,128,64);
                ctx.fillStyle = "#fff";
                ctx.font = `${sIn.value}px ${FONT_NAME}`;
                ctx.textAlign = "center";
                ctx.textBaseline = "middle";

                if (s === 0) { // [오전/오후] 중앙 정렬
                    const totalW = chars.length * 32;
                    const startX = (128 - totalW) / 2;
                    for (let i = 0; i < chars.length; i++) {
                        ctx.fillText(chars[i], startX + (i * 32) + 16, 32);
                    }
                } else { // [시/분/초] 단위 우측 고정 + 숫자 남은 공간 중앙 정렬
                    const unitX = 96;
                    const numChars = chars.length - 1;
                    
                    // 단위 글자 그리기
                    if (chars.length > 0) {
                        ctx.fillText(chars[chars.length - 1], unitX + 16, 32);
                    }
                    
                    // 숫자 글자들 중앙 정렬 그리기
                    if (numChars > 0) {
                        const startX = (96 - (numChars * 32)) / 2;
                        for (let i = 0; i < numChars; i++) {
                            ctx.fillText(chars[i], startX + (i * 32) + 16, 32);
                        }
                    }
                }
            }
        }

        // 1초마다 미리보기 자동 갱신
        setInterval(() => {
            if(fontLoaded && !apply.disabled) updatePreview();
        }, 1000);

        async function processAll() {
            if(!fontLoaded) return;
            apply.disabled = true;
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
            
            status.innerText = "낱자 폰트 세트 적용 완료!";
            apply.disabled = false;
        }

        async function resetAll() {
            if(!confirm("모든 커스텀 낱자를 삭제하시겠습니까?")) return;
            await fetch('/delete_all', { method: 'POST' });
            location.reload();
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

    server.on("/delete_all", HTTP_POST, []() {
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while (file) {
            String fileName = file.name();
            file.close();
            if (fileName.startsWith("c_")) {
                LittleFS.remove("/" + fileName);
            }
            file = root.openNextFile();
        }
        server.send(200, "text/plain", "OK");
    });

    server.begin();
    Serial.println("[WEB] Font Studio started");
}

#endif
