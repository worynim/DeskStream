#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include <Arduino.h>
#include <WebServer.h>
#include "LittleFS.h"
#include <ArduinoJson.h>

#define BUZZER_PIN 7

extern WebServer server;
extern void drawDefaultScreen(int idx);

#ifndef KEY_CONFIG_STRUCT
#define KEY_CONFIG_STRUCT
#define MODE_STRING 0
#define MODE_COMBO  1
#define MODE_MEDIA  2
struct KeyConfig {
    uint8_t mode; 
    char stringVal[256]; 
    char korVal[256];
    uint8_t modifiers[3];
    uint8_t key;
    char label[16];
};
#endif
extern KeyConfig deckConfigs[4];

// === 프리미엄 UI HTML ===
const char* index_html = R"rawliteral(
<!DOCTYPE html>
<html lang="ko">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>DS DECK MANAGER</title>
<style>
    @import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;500;700&display=swap');
    body { background: radial-gradient(circle at top left, #1e1e2f, #0f0f16); color: #fff; font-family: 'Inter', sans-serif; display: flex; flex-direction: column; align-items: center; margin: 0; padding: 20px; }
    h1 { color: #f82; font-weight: 700; background: linear-gradient(90deg, #ff8a00, #e52e71); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
    .container { display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; max-width: 1400px; }
    .card { background: rgba(255, 255, 255, 0.05); border: 1px solid rgba(255,255,255,0.1); backdrop-filter: blur(15px); border-radius: 16px; padding: 25px; width: 320px; box-shadow: 0 10px 40px rgba(0,0,0,0.4); box-sizing: border-box; }
    .card h3 { margin-top: 0; color: #ccc; text-align: center; }
    .previews { display: flex; gap: 10px; margin-bottom: 20px; justify-content: space-around; }
    .preview-box { text-align: center; }
    .preview-box small { color: #888; font-size: 0.75rem; }
    .preview-box canvas { background: #000; border: 1px solid #444; width: 128px; height: 64px; border-radius: 4px; image-rendering: pixelated; margin-top: 5px; }
    .field { margin-bottom: 12px; }
    .field label { display: flex; justify-content: space-between; font-size: 0.8rem; color: #aaa; margin-bottom: 5px; }
    input[type="text"], select, input[type="file"] { width: 100%; border: 1px solid rgba(255,255,255,0.1); background: rgba(0,0,0,0.3); color: #fff; padding: 10px; border-radius: 6px; box-sizing: border-box; font-size: 0.9rem; }
    input[type="range"] { width: 100%; margin-top: 5px; }
    input[type="text"]:focus, select:focus { outline: none; border-color: #ff8a00; }
    select option { background: #222; }
    .combo-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 5px; }
    .combo-grid select { grid-column: span 1; }
    .combo-grid select:last-of-type { grid-column: span 2; }
    .combo-grid input { grid-column: span 2; }
    button.save { width: 100%; padding: 14px; background: linear-gradient(90deg, #ff8a00, #e52e71); border: none; color: #fff; border-radius: 8px; cursor: pointer; font-weight: bold; margin-top: 10px; transition: 0.3s; }
    button.save:hover { transform: translateY(-2px); box-shadow: 0 6px 20px rgba(229, 46, 113, 0.4); }
</style>
</head>
<body>
    <h1>DS DECK CONTROL PANEL</h1>
    <div class="container" id="main"></div>
    <script>
        const KOR_LAYOUT = {
            'ㄱ':'r','ㄲ':'R','ㄴ':'s','ㄷ':'e','ㄸ':'E','ㄹ':'f','ㅁ':'a','ㅂ':'q','ㅃ':'Q','ㅅ':'t','ㅆ':'T','ㅇ':'d','ㅈ':'w','ㅉ':'W','ㅊ':'c','ㅋ':'z','ㅌ':'x','ㅍ':'v','ㅎ':'g',
            'ㅏ':'k','ㅐ':'o','ㅒ':'O','ㅑ':'i','ㅓ':'j','ㅔ':'p','ㅖ':'P','ㅕ':'u','ㅗ':'h','ㅛ':'y','ㅜ':'n','ㅠ':'b',
            'ㅡ':'m','ㅣ':'l',
            'ㄳ':'rt','ㄵ':'sw','ㄶ':'sg','ㄺ':'fr','ㄻ':'fa','ㄼ':'fq','ㄽ':'ft','ㄾ':'fx','ㄿ':'fv','ㅀ':'fg','ㅄ':'qt',
            'ㅘ':'hk','ㅙ':'ho','ㅚ':'hl','ㅝ':'nj','ㅞ':'np','ㅟ':'nl','ㅢ':'ml'
        };
        const ENG_LAYOUT = Object.fromEntries(Object.entries(KOR_LAYOUT).map(([k,v])=>[v,k]));

        function korToEng(text) {
            let res = "";
            let mode = "KOR"; // 기기는 기본적으로 Mac이 '한글 모드'라고 가정함
            
            for(let i=0; i<text.length; i++) {
                let char = text[i];
                let c = char.charCodeAt(0);
                let isKor = (c >= 0xAC00 && c <= 0xD7A3) || KOR_LAYOUT[char] !== undefined;
                let isEng = /[a-zA-Z]/.test(char);
                
                // 첫 글자부터 영어라면 시작하자마자 언어 전환 트리거
                if (i === 0 && isEng) {
                    res += "[#CAPS#]";
                    mode = "ENG";
                } 
                // 중간에 모드가 바뀌는 경우
                else if (isKor && mode === "ENG") {
                    res += "[#CAPS#]";
                    mode = "KOR";
                } else if (isEng && mode === "KOR") {
                    res += "[#CAPS#]";
                    mode = "ENG";
                }
                
                if(c >= 0xAC00 && c <= 0xD7A3) { // 완성형 한글 완전 분해
                    let code = c - 0xAC00;
                    let t = code % 28; // 종성
                    let m = ((code - t) / 28) % 21; // 중성
                    let cho = Math.floor(((code - t) / 28) / 21); // 초성
                    
                    const choList = ["r","R","s","e","E","f","a","q","Q","t","T","d","w","W","c","z","x","v","g"];
                    const jungList = ["k","o","i","O","j","p","u","P","h","hk","ho","hl","y","n","nj","np","nl","b","m","ml","l"];
                    const jongList = ["","r","R","rt","s","sw","sg","e","f","fr","fa","fq","ft","fx","fv","fg","a","q","qt","t","T","d","w","c","z","x","v","g"];
                    
                    res += choList[cho] + jungList[m] + jongList[t];
                } else if(KOR_LAYOUT[char]) {
                    res += KOR_LAYOUT[char]; // 한글 자모 단일 글자
                } else {
                    res += char; // 영어, 숫자, 고유 기호는 있는 그대로
                }
            }
            
            // 매크로 입력 후 사용자의 편의를 위해 원상태(한글)로 복구
            if (mode === "ENG") {
                res += "[#CAPS#]";
            }
            
            return res;
        }

        window.toggleMode = (i) => {
            const mode = document.getElementById('mode'+i).value;
            document.getElementById('strDiv'+i).style.display = mode == '0' ? 'block' : 'none';
            document.getElementById('cmbDiv'+i).style.display = mode == '1' ? 'block' : 'none';
            document.getElementById('mediaDiv'+i).style.display = mode == '2' ? 'block' : 'none';
        };

        async function loadUI() {
            const res = await fetch('/get_config');
            const data = await res.json();
            const main = document.getElementById('main');
            main.innerHTML = ''; // 요소 중복 생성 방지를 위한 초기화
            data.keys.forEach((k, i) => {
                let mod0 = k.mod0 || 0, mod1 = k.mod1 || 0, mod2 = k.mod2 || 0;
                let isSpecial = [178, 179, 177, 176, 10, 27, 8, 9, 128, 206, 210, 213, 212, 211, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226].includes(k.key);
                let keyCh = (k.key && !isSpecial) ? String.fromCharCode(k.key) : '';
                main.innerHTML += `
                    <div class="card">
                        <h3>BUTTON ${i+1}</h3>
                        <div class="previews">
                           <div class="preview-box"><small>ORIGINAL</small><br><canvas id="cO${i}" width="128" height="64"></canvas></div>
                           <div class="preview-box"><small>OLED RESULT</small><br><canvas id="cB${i}" width="128" height="64"></canvas></div>
                        </div>
                        <div class="field">
                            <label>OLED 아이콘 변경 (.png, .jpg) <button type="button" style="background:none; border:none; color:#f55; cursor:pointer; font-weight:bold; font-size:0.8rem;" onclick="deleteIcon(${i})">[아이콘 삭제]</button></label>
                            <input type="file" id="f${i}" accept="image/*" onchange="previewImg(${i})">
                        </div>
                        <div class="field">
                            <label><span>흑백 명암비 (Threshold)</span><span id="thVal${i}">128</span></label>
                            <input type="range" id="th${i}" min="0" max="255" value="128" oninput="previewImg(${i})">
                        </div>
                        <div class="field">
                            <label>이미지 렌더링 필터</label>
                            <select id="dither${i}" onchange="previewImg(${i})">
                                <option value="0">단순 다크/라이트 (Threshold)</option>
                                <option value="1">Floyd-Steinberg 디더링 (Halftone)</option>
                            </select>
                        </div>
                        <div class="field">
                            <label>텍스트 라벨 (이미지가 없을 때만 표시)</label>
                            <input type="text" id="l${i}" value="${k.label}" maxlength="15">
                        </div>
                        <div class="field">
                            <label>동작 모드</label>
                            <select id="mode${i}" onchange="toggleMode(${i})">
                                <option value="0" ${k.mode==0?'selected':''}>텍스트 매크로 (자동 타이핑)</option>
                                <option value="1" ${k.mode==1?'selected':''}>단축키 (조합키 모드)</option>
                                <option value="2" ${k.mode==2?'selected':''}>멀티미디어 (볼륨/재생)</option>
                            </select>
                        </div>
                        <div id="strDiv${i}" style="display:${k.mode==0?'block':'none'}" class="field">
                            <label>전송할 문자열 (한글 입력 시 Mac의 한/영 상태가 '한'이어야 함)</label>
                            <input type="text" id="str${i}" value="${k.mode==0 && k.korVal ? k.korVal : k.stringVal}">
                        </div>
                        <div id="cmbDiv${i}" style="display:${k.mode==1?'block':'none'}" class="field">
                            <label>단축키 조합 설정</label>
                            <div class="combo-grid">
                                <select id="m0_${i}"><option value="0">None</option><option value="224" ${mod0==224?'selected':''}>CTRL</option><option value="225" ${mod0==225?'selected':''}>SHIFT</option><option value="226" ${mod0==226?'selected':''}>ALT</option><option value="227" ${mod0==227?'selected':''}>CMD (Mac)</option><option value="231" ${mod0==231?'selected':''}>WIN</option></select>
                                <select id="m1_${i}"><option value="0">None</option><option value="224" ${mod1==224?'selected':''}>CTRL</option><option value="225" ${mod1==225?'selected':''}>SHIFT</option><option value="226" ${mod1==226?'selected':''}>ALT</option><option value="227" ${mod1==227?'selected':''}>CMD (Mac)</option><option value="231" ${mod1==231?'selected':''}>WIN</option></select>
                                <select id="m2_${i}"><option value="0">None</option><option value="224" ${mod2==224?'selected':''}>CTRL</option><option value="225" ${mod2==225?'selected':''}>SHIFT</option><option value="226" ${mod2==226?'selected':''}>ALT</option><option value="227" ${mod2==227?'selected':''}>CMD (Mac)</option><option value="231" ${mod2==231?'selected':''}>WIN</option></select>
                                <select id="special${i}">
                                    <option value="0">일반 키보드 입력 사용</option>
                                    <option value="178" ${k.key==178?'selected':''}>← (Left Arrow)</option>
                                    <option value="179" ${k.key==179?'selected':''}>→ (Right Arrow)</option>
                                    <option value="177" ${k.key==177?'selected':''}>↑ (Up Arrow)</option>
                                    <option value="176" ${k.key==176?'selected':''}>↓ (Down Arrow)</option>
                                    <option value="10" ${k.key==10?'selected':''}>ENTER</option>
                                    <option value="27" ${k.key==27?'selected':''}>ESC</option>
                                    <option value="8" ${k.key==8?'selected':''}>BACKSPACE</option>
                                    <option value="9" ${k.key==9?'selected':''}>TAB</option>
                                    <option value="128" ${k.key==128?'selected':''}>CAPS LOCK</option>
                                    <option value="206" ${k.key==206?'selected':''}>PRTSC</option>
                                    <option value="210" ${k.key==210?'selected':''}>HOME</option>
                                    <option value="213" ${k.key==213?'selected':''}>END</option>
                                    <option value="212" ${k.key==212?'selected':''}>DELETE</option>
                                    <option value="211" ${k.key==211?'selected':''}>PG UP</option>
                                    <option value="214" ${k.key==214?'selected':''}>PG DN</option>
                                    <option value="215" ${k.key==215?'selected':''}>F1</option><option value="216" ${k.key==216?'selected':''}>F2</option><option value="217" ${k.key==217?'selected':''}>F3</option><option value="218" ${k.key==218?'selected':''}>F4</option><option value="219" ${k.key==219?'selected':''}>F5</option><option value="220" ${k.key==220?'selected':''}>F6</option><option value="221" ${k.key==221?'selected':''}>F7</option><option value="222" ${k.key==222?'selected':''}>F8</option><option value="223" ${k.key==223?'selected':''}>F9</option><option value="224" ${k.key==224?'selected':''}>F10</option><option value="225" ${k.key==225?'selected':''}>F11</option><option value="226" ${k.key==226?'selected':''}>F12</option>
                                </select>
                                <input type="text" id="key${i}" value="${keyCh}" placeholder="기준 키" maxlength="1">
                            </div>
                        </div>
                        <div id="mediaDiv${i}" style="display:${k.mode==2?'block':'none'}" class="field">
                            <label>미디어 컨트롤</label>
                            <select id="med${i}">
                                <option value="1" ${k.key==1?'selected':''}>볼륨 크게 (Volume Up)</option>
                                <option value="2" ${k.key==2?'selected':''}>볼륨 작게 (Volume Down)</option>
                                <option value="3" ${k.key==3?'selected':''}>음소거 (Mute)</option>
                                <option value="4" ${k.key==4?'selected':''}>재생 / 일시정지 (Play/Pause)</option>
                                <option value="5" ${k.key==5?'selected':''}>다음 곡 (Next Track)</option>
                                <option value="6" ${k.key==6?'selected':''}>이전 곡 (Prev Track)</option>
                            </select>
                        </div>
                        <button class="save" onclick="save(${i})">기기에 적용</button>
                        <canvas id="origData${i}" width="128" height="64" style="display:none;"></canvas>
                    </div>
                `;
            });
            
            // 기존 기기 아이콘 불러오기 (U8G2_R2 180도 회전 복조)
            for(let i=0; i<4; i++) {
                fetch(`/icon${i+1}.bin?t=${new Date().getTime()}`).then(r => {
                    if(r.ok) return r.arrayBuffer(); else throw new Error();
                }).then(buf => {
                    const bw = new Uint8Array(buf);
                    const ctxB = document.getElementById('cB'+i).getContext('2d');
                    const ctxO = document.getElementById('cO'+i).getContext('2d');
                    const id = ctxB.createImageData(128, 64);
                    // 180도 렌더링에 맞춰 하드웨어에 저장된 이미지를 인간의 눈에 맞게 역회전 (x, y 인버트)
                    for(let p=0; p<8; p++) {
                        for(let x=0; x<128; x++) {
                            let b = bw[p*128+x];
                            let actualX = 127 - x;
                            for(let y=0; y<8; y++) {
                                let actualY = 63 - (p*8 + y);
                                let idx = (actualY * 128 + actualX) * 4;
                                let val = (b & (1<<y)) ? 255 : 0;
                                id.data[idx]=val; id.data[idx+1]=val; id.data[idx+2]=val; id.data[idx+3]=255;
                            }
                        }
                    }
                    ctxB.putImageData(id, 0, 0);
                    ctxO.putImageData(id, 0, 0); 
                }).catch(e => { console.log('No icon for ' + (i+1)); });
            }
        }

        window.previewImg = (i) => {
            const file = document.getElementById('f'+i).files[0];
            const thresh = parseInt(document.getElementById('th'+i).value);
            document.getElementById('thVal'+i).innerText = thresh;
            
            const origCanvas = document.getElementById('origData'+i);
            const ctxOrig = origCanvas.getContext('2d');

            const applyThreshold = () => {
               const useDither = document.getElementById('dither'+i).value === "1";
               const cO = document.getElementById('cO'+i);
               const cB = document.getElementById('cB'+i);
               cO.getContext('2d').putImageData(ctxOrig.getImageData(0,0,128,64), 0, 0);
               const ctxB = cB.getContext('2d');
               
               let idData = ctxOrig.getImageData(0,0,128,64);
               let data = idData.data;
               let w = 128, h = 64;
               
               // 먼저 모든 픽셀을 명도(Luma)로 변환
               let lumaArray = new Float32Array(w * h);
               for(let j=0; j<data.length; j+=4) {
                   lumaArray[j/4] = data[j]*0.299 + data[j+1]*0.587 + data[j+2]*0.114;
               }

               if (!useDither) {
                   // 일반 Threshold 모드
                   for(let j=0; j<lumaArray.length; j++) {
                       let v = lumaArray[j] >= thresh ? 255 : 0;
                       data[j*4] = data[j*4+1] = data[j*4+2] = v;
                       data[j*4+3] = 255;
                   }
               } else {
                   // Floyd-Steinberg Dithering 모드
                   // 디더링은 오차를 분산하기 때문에 단순 피벗을 바꾸면 밝기가 변하지 않음.
                   // 따라서 Thresh 슬라이더를 '밝기 오프셋(Brightness Shift)'으로 활용.
                   let brightnessOffset = 128 - thresh;
                   
                   for(let j=0; j<lumaArray.length; j++) {
                       let val = lumaArray[j] + brightnessOffset;
                       if (val < 0) val = 0; else if (val > 255) val = 255;
                       lumaArray[j] = val;
                   }

                   for (let y = 0; y < h; y++) {
                       for (let x = 0; x < w; x++) {
                           let idx = y * w + x;
                           let oldPixel = lumaArray[idx];
                           // 디더링 판별 기준점은 정중앙(128)으로 고정
                           let newPixel = oldPixel >= 128 ? 255 : 0;
                           data[idx*4] = data[idx*4+1] = data[idx*4+2] = newPixel;
                           data[idx*4+3] = 255;
                           
                           let err = oldPixel - newPixel;
                           
                           // Error diffusion
                           if (x + 1 < w) lumaArray[idx + 1] += err * 7 / 16;
                           if (y + 1 < h) {
                               if (x - 1 >= 0) lumaArray[(y + 1) * w + x - 1] += err * 3 / 16;
                               lumaArray[(y + 1) * w + x] += err * 5 / 16;
                               if (x + 1 < w) lumaArray[(y + 1) * w + x + 1] += err * 1 / 16;
                           }
                       }
                   }
               }
               ctxB.putImageData(idData, 0, 0);
            };

            if(file && !origCanvas.dataset.loadedFile || origCanvas.dataset.loadedFile !== file.name) {
                const reader = new FileReader();
                reader.onload = (e) => {
                    const img = new Image();
                    img.onload = () => {
                       ctxOrig.fillStyle="#000"; ctxOrig.fillRect(0,0,128,64);
                       let s = Math.min(128/img.width, 64/img.height);
                       let x = (128-img.width*s)/2, y = (64-img.height*s)/2;
                       ctxOrig.drawImage(img, x, y, img.width*s, img.height*s);
                       origCanvas.dataset.loadedFile = file.name;
                       applyThreshold();
                    };
                    img.src = e.target.result;
                };
                reader.readAsDataURL(file);
            } else {
                applyThreshold(); // Threshold 슬라이더 변화 시 즉시 적용
                window['iconChanged'+i] = true;
            }
        }
        
        window.deleteIcon = async (i) => {
            await fetch(`/delete_icon`, {method:'POST', headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: `idx=${i}`});
            const cO = document.getElementById('cO'+i).getContext('2d');
            const cB = document.getElementById('cB'+i).getContext('2d');
            const hiddenCanvas = document.getElementById('origData'+i);
            cO.clearRect(0,0,128,64); cB.clearRect(0,0,128,64);
            if(hiddenCanvas) {
                hiddenCanvas.getContext('2d').clearRect(0,0,128,64);
                delete hiddenCanvas.dataset.loadedFile;
            }
        }

        window.save = async (i) => {
            const l = document.getElementById('l'+i).value;
            const mode = document.getElementById('mode'+i).value;
            let strVal = document.getElementById('str'+i).value;
            let engStr = strVal;
            
            // --- 한글 매크로 저장 시 영문 자판 위치로 변환 ---
            if (mode === '0') {
                engStr = korToEng(strVal);
            }
            
            const m0 = document.getElementById('m0_'+i).value;
            const m1 = document.getElementById('m1_'+i).value;
            const m2 = document.getElementById('m2_'+i).value;
            
            let keyCode = 0;
            if (mode === '1') {
                const specVal = parseInt(document.getElementById('special'+i).value);
                if (specVal !== 0) {
                    // 드롭다운 value가 곧 실제 키코드 (10=ENTER, 178=Left 등)
                    keyCode = specVal;
                } else {
                    const keyChar = document.getElementById('key'+i).value.toLowerCase();
                    keyCode = keyChar.length > 0 ? keyChar.charCodeAt(0) : 0;
                }
            } else if (mode === '2') {
                keyCode = parseInt(document.getElementById('med'+i).value);
            }
            
            let bodyParams = new URLSearchParams({
                idx: i, label: l, mode: mode, str: engStr, kor: strVal, m0: m0, m1: m1, m2: m2, key: keyCode
            });

            await fetch('/save_config', { method: 'POST', headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: bodyParams.toString() });

            // 캔버스에 이미지가 그려져 있다면 (Threshold 적용 혹은 새로 업로드) 무조건 갱신
            // U8G2_R2 환경에 맞춰서 거꾸로(180도 회전) 인코딩 하여 바이너리 파일 저장
            const cB = document.getElementById('cB'+i);
            const id = cB.getContext('2d').getImageData(0,0,128,64).data;
            const bm = new Uint8Array(1024);
            
            for(let p=0; p<8; p++) {
                for(let x=0; x<128; x++) {
                    let b = 0; 
                    let actualX = 127 - x;
                    for(let y=0; y<8; y++) {
                        let actualY = 63 - (p*8 + y);
                        if(id[(actualY*128 + actualX)*4] > 128) b |= (1<<y);
                    }
                    bm[p*128+x] = b;
                }
            }
            
            // 이미지가 등록되었거나 Threshold가 변경된 경우에만 이미지 전송
            const file = document.getElementById('f'+i).files[0];
            if(file || window['iconChanged'+i]) {
                let fd = new FormData();
                fd.append('icon', new Blob([bm]), `icon${i+1}.bin`);
                await fetch(`/upload?idx=${i}`, {method:'POST', body:fd});
                window['iconChanged'+i] = false;
            }
            loadUI();
        }
        loadUI();
    </script>
</body>
</html>
)rawliteral";

File fUp;

void initWebHandlers() {
    server.on("/", []() { server.send(200, "text/html", index_html); });
    
    server.on("/get_config", []() {
        DynamicJsonDocument doc(4096); 
        JsonArray keys = doc.createNestedArray("keys");
        for(int i=0; i<4; i++){ 
            JsonObject o = keys.createNestedObject(); 
            o["label"] = deckConfigs[i].label; 
            o["mode"] = deckConfigs[i].mode;
            o["stringVal"] = deckConfigs[i].stringVal;
            o["korVal"] = deckConfigs[i].korVal;
            o["mod0"] = deckConfigs[i].modifiers[0];
            o["mod1"] = deckConfigs[i].modifiers[1];
            o["mod2"] = deckConfigs[i].modifiers[2];
            o["key"] = deckConfigs[i].key; 
        }
        String out; serializeJson(doc, out); server.send(200, "application/json", out);
    });

    server.on("/save_config", []() {
        if (!server.hasArg("idx")) { server.send(400); return; }
        int idx = server.arg("idx").toInt();
        strlcpy(deckConfigs[idx].label, server.arg("label").c_str(), 16);
        deckConfigs[idx].mode = (uint8_t)server.arg("mode").toInt();
        strlcpy(deckConfigs[idx].stringVal, server.arg("str").c_str(), 256);
        if(server.hasArg("kor")) strlcpy(deckConfigs[idx].korVal, server.arg("kor").c_str(), 256);
        deckConfigs[idx].modifiers[0] = (uint8_t)server.arg("m0").toInt();
        deckConfigs[idx].modifiers[1] = (uint8_t)server.arg("m1").toInt();
        deckConfigs[idx].modifiers[2] = (uint8_t)server.arg("m2").toInt();
        deckConfigs[idx].key = (uint8_t)server.arg("key").toInt();
        
        DynamicJsonDocument doc(4096); JsonArray keys = doc.createNestedArray("keys");
        for(int i=0; i<4; i++){ 
            JsonObject o = keys.createNestedObject(); 
            o["label"] = deckConfigs[i].label; 
            o["mode"] = deckConfigs[i].mode;
            o["stringVal"] = deckConfigs[i].stringVal;
            o["korVal"] = deckConfigs[i].korVal;
            o["mod0"] = deckConfigs[i].modifiers[0];
            o["mod1"] = deckConfigs[i].modifiers[1];
            o["mod2"] = deckConfigs[i].modifiers[2];
            o["key"] = deckConfigs[i].key; 
        }
        File f = LittleFS.open("/config.json", "w"); 
        if(f) { serializeJson(doc, f); f.close(); }
        // 라벨이나 모드가 즉시 반영될 수 있도록 명시적 드로우 실행
        drawDefaultScreen(idx); 
        tone(BUZZER_PIN, 2000, 100); // 기기 적용 성공 부저음
        server.send(200, "text/plain", "OK");
    });
    
    server.on("/delete_icon", HTTP_POST, []() {
        if(server.hasArg("idx")) {
            String path = "/icon" + String(server.arg("idx").toInt() + 1) + ".bin";
            if(LittleFS.exists(path)) LittleFS.remove(path);
            drawDefaultScreen(server.arg("idx").toInt());
            tone(BUZZER_PIN, 1500, 100); // 아이콘 삭제 성공 부저음
        }
        server.send(200, "text/plain", "OK");
    });

    server.on("/upload", HTTP_POST, [](){ server.send(200); }, [](){
        HTTPUpload& up = server.upload();
        if(up.status == UPLOAD_FILE_START) { fUp = LittleFS.open("/" + up.filename, "w"); }
        else if(up.status == UPLOAD_FILE_WRITE && fUp) { fUp.write(up.buf, up.currentSize); }
        else if(up.status == UPLOAD_FILE_END && fUp) { 
            fUp.close(); 
            if(server.hasArg("idx")) drawDefaultScreen(server.arg("idx").toInt()); 
        }
    });

    server.onNotFound([]() {
        String path = server.uri();
        if(LittleFS.exists(path)){
            File f = LittleFS.open(path, "r");
            server.streamFile(f, "application/octet-stream");
            f.close();
            return;
        }
        server.send(404, "text/plain", "Not Found");
    });

    server.begin();
}
#endif
