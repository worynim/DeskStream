// worynim@gmail.com
/**
 * @file web_index.h
 * @brief 스마트 정보 스테이션 웹 대시보드 리소스
 * @details 설정 페이지 HTML, CSS 및 성공 알림 페이지 리소스 정의 (PROGMEM 활용)
 */
#ifndef WEB_INDEX_H
#define WEB_INDEX_H

#include <Arduino.h>

/**
 * --- HTML 헤더 및 CSS 스타일 정의 ---
 * 현대적이고 반응형인 UI를 위해 모바일 최적화 및 CSS Flexbox 레이아웃을 사용합니다.
 */
const char INDEX_HTML_START[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ko">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title data-en="Smart Info Station">스마트 정보 스테이션</title>
    <style>
        body { font-family: 'Apple SD Gothic Neo', 'Malgun Gothic', sans-serif; background-color: #f4f7f6; color: #333; margin: 0; padding: 20px; text-align: center; }
        .container { max-width: 600px; margin: 0 auto; background: #fff; border-radius: 12px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); padding: 30px; }
        h2 { color: #2c3e50; font-size: 24px; margin-bottom: 20px; border-bottom: 2px solid #3498db; padding-bottom: 10px; }
        .section { text-align: left; margin-bottom: 25px; padding: 15px; background: #f9fbfd; border-radius: 8px; border-left: 5px solid #3498db; }
        h3 { color: #2980b9; margin-top: 0; font-size: 18px; }
        p { font-size: 13px; color: #7f8c8d; margin-bottom: 10px; line-height: 1.5; }
        input[type='text'], select { width: calc(100% - 20px); padding: 10px; border: 1px solid #dcdde1; border-radius: 6px; font-size: 15px; outline: none; transition: border 0.3s; }
        input[type='text']:focus, select:focus { border-color: #3498db; }
        .flex-container { display: flex; flex-wrap: wrap; gap: 8px; justify-content: space-between; }
        .flex-item { flex: 0 0 calc(25% - 8px); min-width: 100px; }
        .page-title { background: #ecf0f1; padding: 5px 10px; border-radius: 4px; font-weight: bold; margin-bottom: 15px; color: #7f8c8d; font-size: 14px; }
        .label { display: block; font-weight: bold; margin-bottom: 5px; font-size: 12px; color: #34495e; text-align: center; }
        select { width: 100%; padding: 8px 4px; font-size: 13px; }
        .btn { background-color: #3498db; color: white; border: none; padding: 12px 24px; font-size: 16px; font-weight: bold; border-radius: 8px; cursor: pointer; transition: background 0.3s, transform 0.1s; width: 100%; box-shadow: 0 4px 6px rgba(52, 152, 219, 0.3); }
        .btn:hover { background-color: #2980b9; }
        .btn:active { transform: translateY(2px); box-shadow: 0 2px 4px rgba(52, 152, 219, 0.3); }
        a { color: #3498db; text-decoration: none; font-weight: bold; }
        a:hover { text-decoration: underline; }
        .container { position: relative; }
        .lang { position: absolute; top: 10px; right: 12px; display: flex; border: 1px solid #dcdde1; border-radius: 6px; overflow: hidden; }
        .lang button { border: none; background: #fff; color: #7f8c8d; padding: 4px 10px; font-size: 12px; font-weight: bold; cursor: pointer; }
        .lang button.on { background: #3498db; color: #fff; }
    </style>
</head>
<body>
    <div class="container">
        <div class="lang"><button type="button" id="lang_ko" onclick="setLang('ko')">한국어</button><button type="button" id="lang_en" onclick="setLang('en')">EN</button></div>
        <h2 data-en="🚀 Info Station Settings">🚀 정보 스테이션 환경설정</h2>
        <form action="/set" method="post">
)rawliteral";

/**
 * --- 웹 설정 폼 섹션 (사용자 입력 필드) ---
 * 유튜브 커스터마이징, 미세먼지 관측소, 기상청 동네 코드, 해외 날씨 도시, 시간대 설정을 담당합니다.
 * 해외 날씨와 화면별 도시(세계 시계·일출일몰·주간 날씨·기온 그래프)는 브라우저가 Open-Meteo 도시 검색을 직접 불러 고르고, 이름(영문)·좌표만 기기로 보낸다.
 * 동네 날씨도 브라우저가 기상청 동네 검색을 직접 불러 고르고, 10자리 코드만 weather_code로 보낸다.
 * 관심 종목은 네이버가 브라우저의 직접 요청을 막아 기기의 /stock_search가 대신 검색하고, 이름·시세 경로만 기기로 보낸다.
 * data-widgets 가 있는 칸은 화면 배치(PAGE 1~3)에 그 위젯이 하나라도 있을 때만 보인다 (값은 web_handlers.cpp가 채움).
 */
const char INDEX_HTML_SETTINGS[] PROGMEM = R"rawliteral(
            <div class="section" data-widgets="%WID_YOUTUBE%">
                <h3 data-en="▶️ YouTube Channel">▶️ YouTube 채널 설정</h3>
                <p data-en="Enter the YouTube channel name to show its subscriber count.">구독자 수를 확인할 유튜브 채널명을 입력하세요.</p>
                <input type="text" name="channel" placeholder="현재: %CHANNEL%" data-en-ph="Current: %CHANNEL%">
            </div>
            <div class="section" data-widgets="%WID_DUST%">
                <h3 data-en="😷 Fine Dust Station">😷 미세먼지 측정소 위치</h3>
                <p data-en="Enter an area in Korean as used by Naver fine dust search (e.g. 가산동).">네이버 미세먼지 검색 기준 지역(예: 가산동/읍/면)을 입력하세요.</p>
                <input type="text" name="location" placeholder="현재: %LOCATION%" data-en-ph="Current: %LOCATION%">
            </div>
            <div class="section" data-widgets="%WID_WEATHER%">
                <h3 data-en="☀️ Local Weather (Korean district code)">☀️ 동네 날씨 (법정동코드)</h3>
                <p data-en="Search by district name in Korean (e.g. 역삼, 가산동) and pick one to fill in the code. You can also type the code directly.">동 이름(예: 역삼, 가산동)으로 검색한 뒤 목록에서 고르면 코드가 채워집니다. 코드를 직접 입력해도 됩니다.</p>
                <input type="text" id="dong_q" placeholder="예: 역삼, 가산동" data-en-ph="e.g. 역삼, 가산동">
                <button type="button" onclick="dongSearch()" style="margin-top:8px; padding:8px 16px;" data-en="🔍 Search">🔍 검색</button>
                <select id="dong_list" onchange="dongPick()" style="display:none; margin-top:8px;"></select>
                <p id="dong_msg"></p>
                <input type="text" name="weather_code" id="weather_code" placeholder="현재: %WEATHER%" data-en-ph="Current: %WEATHER%">
                <input type="hidden" name="weather_name" id="weather_name">
            </div>
            <div class="section" data-widgets="%WID_INTL%">
                <h3 data-en="🌍 International Weather City">🌍 해외 날씨 도시</h3>
                <p data-en="Search by city name and pick one from the list. Current: %INTL_EN%">도시 이름(한글·영문)으로 검색한 뒤 목록에서 고르세요. 현재: %INTL%</p>
                <input type="text" id="intl_q" placeholder="예: 도쿄, Paris" data-en-ph="e.g. Tokyo, Paris">
                <button type="button" onclick="citySearch('intl')" style="margin-top:8px; padding:8px 16px;" data-en="🔍 Search">🔍 검색</button>
                <select id="intl_list" onchange="cityPick('intl')" style="display:none; margin-top:8px;"></select>
                <p id="intl_msg"></p>
                <input type="hidden" name="intl_name" id="intl_name">
                <input type="hidden" name="intl_lat" id="intl_lat">
                <input type="hidden" name="intl_lon" id="intl_lon">
            </div>
            <div class="section" data-widgets="%WID_WC%,%WID_SUN%,%WID_WEEKLY%,%WID_TGRAPH%">
                <h3 data-en="🏙️ City per Screen (World Clock · Sunrise/Sunset · Weekly Weather · Temp Graph)">🏙️ 화면별 도시 (세계 시계·일출일몰·주간 날씨·기온 그래프)</h3>
                <p data-en="Pick a city for each screen that has one of these widgets. Time offset and daylight saving are automatic. Defaults: World Clock New York, the others Seoul.">위젯을 놓은 화면마다 도시를 따로 고를 수 있습니다. 시차·서머타임은 자동입니다. 고르지 않으면 세계 시계는 New York, 나머지는 Seoul입니다.</p>
                <div id="city_rows"></div>
            </div>
            <div class="section" data-widgets="%WID_STOCK%">
                <h3 data-en="📈 Stock per Screen">📈 관심 종목</h3>
                <p data-en="Search Korean or overseas stocks by name (e.g. 삼성전자, apple) and pick one for each screen. Updates every minute while its market is open.">화면마다 종목을 이름(예: 삼성전자, 애플, apple)으로 검색해 고릅니다. 국내·해외 종목 모두 되고, 장이 열려 있을 때 1분마다 갱신합니다.</p>
                <div id="stock_rows"></div>
            </div>
            <div class="section" data-widgets="%WID_CLOCKS%">
                <h3 data-en="🕰️ Clock Format">🕰️ 시계 표시 형식</h3>
                <p data-en="Applies to both the Korea Clock and World Clock. In 12-hour mode AM/PM appears at the top left.">한국 시계·세계 시계에 함께 적용됩니다. 12시간제에서는 왼쪽 위에 AM/PM이 보입니다.</p>
                <select name="clock12">
                    <option value="0" %CLOCK24_SEL% data-en="24-hour (14:05:09)">24시간제 (14:05:09)</option>
                    <option value="1" %CLOCK12_SEL% data-en="12-hour (2:05:09 PM)">12시간제 (2:05:09 PM)</option>
                </select>
            </div>
            <div class="section" data-widgets="%WID_DDAY%">
                <h3>📅 D-day</h3>
                <p data-en="Set a name and date for each screen with a D-day widget. Shows days left (D-23) or days since (D+5). Clear the date to unset.">D-day를 놓은 화면마다 이름과 날짜를 따로 정합니다. 남은 날(D-23)이나 지난 날(D+5)이 보입니다. 날짜를 지우면 미설정.</p>
                <div id="dday_rows"></div>
            </div>
            <div class="section">
                <h3 data-en="⏰ Timezone">⏰ 시간대 (Timezone) 설정</h3>
                <p data-en="Enter the UTC offset in hours. (Korea: 9)">UTC 기준 오프셋 시간을 입력하세요. (대한민국: 9)</p>
                <input type="text" name="tz_offset" placeholder="현재: %TZ%">
            </div>
            <input type="submit" class="btn" value="💾 설정 저장 후 즉시 반영" data-en="💾 Save and Apply Now">
        </form>
    </div>
    <script>
        const GEO = 'https://geocoding-api.open-meteo.com/v1/';
        const $ = (id) => document.getElementById(id);

        // --- 언어 (한국어·영어): 저장한 선택 → 없으면 브라우저 언어. 문구는 data-en(글·버튼 값)·data-en-ph(안내 글)에 있다 ---
        let LANG = localStorage.getItem('lang') || (navigator.language.startsWith('ko') ? 'ko' : 'en');
        const L = (ko, en) => (LANG === 'en' ? en : ko);   // 스크립트가 쓰는 문구
        function applyLang() {
            document.documentElement.lang = LANG;
            document.querySelectorAll('[data-en]').forEach((el) => {
                const prop = el.tagName === 'INPUT' ? 'value' : 'textContent';
                if (el.dataset.ko === undefined) el.dataset.ko = el[prop];   // 처음 본 한국어 문구를 기억한다
                el[prop] = L(el.dataset.ko, el.dataset.en);
            });
            document.querySelectorAll('[data-en-ph]').forEach((el) => {
                if (el.dataset.koPh === undefined) el.dataset.koPh = el.placeholder;
                el.placeholder = L(el.dataset.koPh, el.dataset.enPh);
            });
            $('lang_ko').classList.toggle('on', LANG === 'ko');
            $('lang_en').classList.toggle('on', LANG === 'en');
        }
        function setLang(l) {
            LANG = l;
            localStorage.setItem('lang', l);
            applyLang();
            updateCityRows();
            updateStockRows();
        }

        // --- 화면 배치에 놓인 위젯이 쓰는 설정 칸만 보인다 (숨긴 칸의 값은 그대로 저장된다) ---
        const slotSelects = document.querySelectorAll('select[name^="sm"]');
        function updateSections() {
            const used = new Set([...slotSelects].map((s) => s.value));
            document.querySelectorAll('.section[data-widgets]').forEach((sec) => {
                const show = sec.dataset.widgets.split(',').some((w) => used.has(w));
                sec.style.display = show ? '' : 'none';
            });
        }
        slotSelects.forEach((s) => s.addEventListener('change', updateSections));

        // --- 동네 날씨: 기상청 날씨누리의 동네 검색(행정동, src=A1)으로 10자리 코드를 찾는다 ---
        const DONG = 'https://www.weather.go.kr/w/renew2021/rest/main/place-search.do?start=1&src=A1&query=';
        const dongMsg = (t) => { $('dong_msg').textContent = t; };
        const dongFetch = (q) => fetch(DONG + encodeURIComponent(q)).then((r) => r.json());
        function dongSearch() {
            const q = $('dong_q').value.trim();
            if (!q) return;
            dongMsg(L('검색 중...', 'Searching...'));
            dongFetch(q)
                .then((d) => {
                    // "신정동"처럼 1동·2동으로 나뉜 곳은 결과가 없다 → 끝의 동·읍·면을 떼고 한 번 더 찾는다
                    const short = q.replace(/[동읍면]$/, '');
                    return (d.length || short === q || !short) ? d : dongFetch(short);
                })
                .then((d) => {
                    const list = $('dong_list');
                    list.innerHTML = '';
                    if (!d.length) { list.style.display = 'none'; dongMsg(L('검색 결과가 없습니다.', 'No results.')); return; }
                    list.add(new Option(L('동네를 고르세요 (' + d.length + '곳)', 'Pick a district (' + d.length + ')'), ''));
                    d.forEach((x) => list.add(new Option(x.address, x.dongCode)));   // 같은 동 이름도 시·구까지 보여 구분한다
                    list.style.display = 'block';
                    dongMsg(d.length >= 10 ? L('결과가 많습니다 — 찾는 곳이 없으면 시·구 이름을 함께 넣어 보세요.', 'Many results — if yours is missing, add the city or district name.') : '');
                })
                .catch(() => dongMsg(L('검색 실패 — 이 기기(휴대폰·PC)의 인터넷 연결을 확인하세요.', 'Search failed — check this device\'s (phone/PC) internet connection.')));
        }
        function dongPick() {
            const list = $('dong_list');
            if (!list.value) return;
            $('weather_code').value = list.value;
            const addr = list.options[list.selectedIndex].text;
            $('weather_name').value = addr.split(' ').pop();   // OLED 윗줄용 동 이름 (예: 신정1동)
            dongMsg(L('선택: ', 'Selected: ') + addr + ' (' + list.value + ')' + L(' — 아래 저장 버튼을 누르세요.', ' — press Save below.'));
        }
        // 코드를 직접 고치면 고른 동 이름과 맞지 않으므로 이름을 지운다 (OLED에는 코드가 보인다)
        $('weather_code').addEventListener('input', () => { $('weather_name').value = ''; });
        $('dong_q').addEventListener('keydown', (e) => {
            if (e.key !== 'Enter') return;
            e.preventDefault();
            setTimeout(dongSearch, 0);
        });
        // --- 도시 검색: Open-Meteo (p = 'intl' 또는 'cityN', 칸 id 접두어) ---
        const cityMsg = (p, t) => { $(p + '_msg').textContent = t; };
        // 1. 한글·영문 이름으로 도시 후보를 찾는다 (목록은 고른 언어의 이름)
        function citySearch(p) {
            const q = $(p + '_q').value.trim();
            if (!q) return;
            cityMsg(p, L('검색 중...', 'Searching...'));
            fetch(GEO + 'search?count=10&language=' + LANG + '&name=' + encodeURIComponent(q))
                .then((r) => r.json())
                .then((d) => {
                    const list = $(p + '_list');
                    list.innerHTML = '';
                    const rs = d.results || [];
                    if (!rs.length) { list.style.display = 'none'; cityMsg(p, L('검색 결과가 없습니다.', 'No results.')); return; }
                    list.add(new Option(L('도시를 고르세요', 'Pick a city'), ''));
                    rs.forEach((r) => list.add(new Option([r.name, r.admin1, r.country].filter(Boolean).join(', '), r.id)));
                    list.style.display = 'block';
                    cityMsg(p, '');
                })
                .catch(() => cityMsg(p, L('검색 실패 — 이 기기(휴대폰·PC)의 인터넷 연결을 확인하세요.', 'Search failed — check this device\'s (phone/PC) internet connection.')));
        }
        // 2. 고른 도시의 영문 이름(OLED 표시용)과 좌표를 폼에 채운다
        function cityPick(p) {
            const id = $(p + '_list').value;
            if (!id) return;
            fetch(GEO + 'get?language=en&id=' + encodeURIComponent(id))
                .then((r) => r.json())
                .then((r) => {
                    $(p + '_name').value = r.name;
                    $(p + '_lat').value = r.latitude;
                    $(p + '_lon').value = r.longitude;
                    cityMsg(p, L('선택: ', 'Selected: ') + r.name + ', ' + r.country + L(' — 아래 저장 버튼을 누르세요.', ' — press Save below.'));
                })
                .catch(() => cityMsg(p, L('도시 정보를 불러오지 못했습니다.', 'Could not load city details.')));
        }
        // 검색 칸에서 엔터 → 폼 저장 대신 검색 (한글 입력 중 엔터도 조합이 끝난 뒤 검색)
        const enterSearch = (p) => $(p + '_q').addEventListener('keydown', (e) => {
            if (e.key !== 'Enter') return;
            e.preventDefault();
            setTimeout(() => citySearch(p), 0);
        });
        enterSearch('intl');

        // --- 칸별 설정 줄 (화면별 도시·D-day) — 그 위젯이 놓인 칸의 줄만 보인다 ---
        const slotTitle = (i, s) => 'PAGE ' + (Math.floor(i / 4) + 1) + ' · ' + s + ' ' + (i % 4 + 1);
        function addSlotRow(box, p, i, html) {
            const row = document.createElement('div');
            row.id = p + '_row';
            row.style.marginBottom = '14px';
            row.innerHTML = '<b data-en="' + slotTitle(i, 'Screen') + '">' + slotTitle(i, '화면') + '</b> ' + html;
            $(box).appendChild(row);
            return row;
        }
        function showSlotRows(prefix, widgetId) {
            slotSelects.forEach((s, i) => { $(prefix + i + '_row').style.display = s.value === widgetId ? '' : 'none'; });
        }

        // 화면별 도시: 세계 시계·일출일몰·주간 날씨가 함께 쓴다 (폼 이름 cityN_name·lat·lon)
        const CITY_WIDGETS = {   // 위젯 번호 → [한국어 이름, 영어 이름, 기본 도시] (slot_city.h cityFor와 같게)
            '%WID_WC%': ['세계 시계', 'World Clock', 'New York'], '%WID_SUN%': ['일출·일몰', 'Sunrise/Sunset', 'Seoul'],
            '%WID_WEEKLY%': ['주간 날씨', 'Weekly Weather', 'Seoul'], '%WID_TGRAPH%': ['기온 그래프', 'Temp Graph', 'Seoul'],
        };
        const CITIES = %CITIES%;   // 칸별 고른 도시 ("" = 기본 도시, web_handlers.cpp)
        CITIES.forEach((name, i) => {
            const p = 'city' + i;
            const row = addSlotRow('city_rows', p, i, '<span id="' + p + '_cur"></span>' +
                '<input type="text" id="' + p + '_q" placeholder="예: 뉴욕, London" data-en-ph="e.g. New York, London" style="margin-top:6px;">' +
                '<button type="button" style="margin-top:8px; padding:8px 16px;" data-en="🔍 Search">🔍 검색</button>' +
                '<select id="' + p + '_list" style="display:none; margin-top:8px;"></select>' +
                '<p id="' + p + '_msg"></p>' +
                '<input type="hidden" name="' + p + '_name" id="' + p + '_name">' +
                '<input type="hidden" name="' + p + '_lat" id="' + p + '_lat">' +
                '<input type="hidden" name="' + p + '_lon" id="' + p + '_lon">');
            row.querySelector('button').addEventListener('click', () => citySearch(p));
            $(p + '_list').addEventListener('change', () => cityPick(p));
            enterSearch(p);
        });

        // 관심 종목: 기기의 /stock_search가 네이버 종목 검색을 대신 불러 준다 (폼 이름 stockN_name·path)
        const STOCK_ID = '%WID_STOCK%';
        const STOCKS = %STOCKS%;   // 칸별 고른 종목 이름 ("" = 미설정, web_handlers.cpp)
        function stockSearch(p) {
            const q = $(p + '_q').value.trim();
            if (!q) return;
            cityMsg(p, L('검색 중...', 'Searching...'));
            fetch('/stock_search?q=' + encodeURIComponent(q))
                .then((r) => r.json())
                .then((d) => {
                    const list = $(p + '_list');
                    list.innerHTML = '';
                    if (!d.length) { list.style.display = 'none'; cityMsg(p, L('검색 결과가 없습니다.', 'No results.')); return; }
                    list.add(new Option(L('종목을 고르세요', 'Pick a stock'), ''));
                    d.forEach((x) => {   // 이름 (시장 · 코드)
                        const o = new Option(x.n + ' (' + x.m + ' · ' + x.p.split('/').pop() + ')', x.p);
                        o.dataset.name = x.n;
                        list.add(o);
                    });
                    list.style.display = 'block';
                    cityMsg(p, '');
                })
                .catch(() => cityMsg(p, L('검색 실패 — 스테이션의 인터넷 연결을 확인하세요.', 'Search failed — check the station\'s internet connection.')));
        }
        function stockPick(p) {
            const list = $(p + '_list');
            if (!list.value) return;
            $(p + '_path').value = list.value;
            $(p + '_name').value = list.options[list.selectedIndex].dataset.name;
            cityMsg(p, L('선택: ', 'Selected: ') + list.options[list.selectedIndex].text + L(' — 아래 저장 버튼을 누르세요.', ' — press Save below.'));
        }
        STOCKS.forEach((name, i) => {
            const p = 'stock' + i;
            const row = addSlotRow('stock_rows', p, i, '<span id="' + p + '_cur"></span>' +
                '<input type="text" id="' + p + '_q" placeholder="예: 삼성전자, 애플" data-en-ph="e.g. Samsung, apple" style="margin-top:6px;">' +
                '<button type="button" style="margin-top:8px; padding:8px 16px;" data-en="🔍 Search">🔍 검색</button>' +
                '<select id="' + p + '_list" style="display:none; margin-top:8px;"></select>' +
                '<p id="' + p + '_msg"></p>' +
                '<input type="hidden" name="' + p + '_name" id="' + p + '_name">' +
                '<input type="hidden" name="' + p + '_path" id="' + p + '_path">');
            row.querySelector('button').addEventListener('click', () => stockSearch(p));
            $(p + '_list').addEventListener('change', () => stockPick(p));
            $(p + '_q').addEventListener('keydown', (e) => {   // 엔터 → 저장 대신 검색
                if (e.key !== 'Enter') return;
                e.preventDefault();
                setTimeout(() => stockSearch(p), 0);
            });
        });
        function updateStockRows() {
            showSlotRows('stock', STOCK_ID);
            STOCKS.forEach((name, i) => { $('stock' + i + '_cur').textContent = L(' — 현재: ', ' — Current: ') + (name || L('미설정', 'not set')); });
        }

        // D-day: 칸마다 이름·날짜 (폼 이름 ddayN_label·date)
        const DDAY_ID = '%WID_DDAY%';
        const DDAYS = %DDAYS%;   // 칸별 현재 값 (web_handlers.cpp)
        DDAYS.forEach((x, i) => {
            const p = 'dday' + i;
            addSlotRow('dday_rows', p, i,
                '<input type="text" name="' + p + '_label" id="' + p + '_label" placeholder="예: 크리스마스" data-en-ph="e.g. Christmas" style="margin-top:6px;">' +
                '<input type="date" name="' + p + '_date" id="' + p + '_date" style="margin-top:8px; padding:8px; font-size:15px;">');
            $(p + '_label').value = x.l;
            $(p + '_date').value = x.d;
        });

        // 칸 배치가 바뀌면 그 칸의 줄도 보이거나 숨긴다 (도시 줄은 위젯 이름·기본 도시도 바꿔 보인다)
        function updateCityRows() {
            slotSelects.forEach((s, i) => {
                const w = CITY_WIDGETS[s.value];
                $('city' + i + '_row').style.display = w ? '' : 'none';
                if (w) $('city' + i + '_cur').textContent = '(' + L(w[0], w[1]) + ')' + L(' — 현재: ', ' — Current: ') + (CITIES[i] || L('기본 ', 'default ') + w[2]);
            });
        }
        const updateSlotRows = () => { updateCityRows(); updateStockRows(); showSlotRows('dday', DDAY_ID); };
        slotSelects.forEach((s) => s.addEventListener('change', updateSlotRows));
        updateSections();
        updateSlotRows();
        applyLang();
        // 다른 입력 칸에서도 엔터로 저장되지 않게 한다 — 저장은 "설정 저장" 버튼으로만
        document.querySelector('form').addEventListener('keydown', (e) => {
            if (e.key === 'Enter' && e.target.matches('input[type="text"]')) e.preventDefault();
        });
    </script>
</body>
</html>
)rawliteral";

/**
 * --- 설정 저장 성공 결과 페이지 ---
 * 저장이 완료되었음을 시각적으로 알리고 메인으로 돌아가는 버튼을 제공합니다.
 */
const char SUCCESS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body { font-family: 'Apple SD Gothic Neo', sans-serif; background-color: #f4f7f6; display: flex; align-items: center; justify-content: center; height: 100vh; margin: 0; }
        .card { background: #fff; padding: 40px; border-radius: 12px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); text-align: center; }
        h1 { color: #27ae60; margin-bottom: 20px; }
        a { display: inline-block; background: #3498db; color: white; padding: 10px 20px; text-decoration: none; border-radius: 6px; font-weight: bold; }
        a:hover { background: #2980b9; }
    </style>
</head>
<body>
    <div class="card">
        <h1>✅ 저장 완료!</h1>
        <p>스마트 보드에 설정이 성공적으로 반영되었습니다.</p>
        <br>
        <a href="/">⬅️ 환경설정 메인으로 돌아가기</a>
    </div>
    <script>
        // 설정 페이지와 같은 언어 (저장한 선택 → 없으면 브라우저 언어)
        if ((localStorage.getItem('lang') || (navigator.language.startsWith('ko') ? 'ko' : 'en')) === 'en') {
            document.documentElement.lang = 'en';
            document.querySelector('h1').textContent = '✅ Saved!';
            document.querySelector('p').textContent = 'Settings have been applied to the board.';
            document.querySelector('a').textContent = '⬅️ Back to Settings';
        }
    </script>
</body>
</html>
)rawliteral";

#endif
