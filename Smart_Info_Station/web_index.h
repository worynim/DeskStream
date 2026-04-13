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
    <title>스마트 정보 스테이션</title>
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
    </style>
</head>
<body>
    <div class="container">
        <h2>🚀 정보 스테이션 환경설정</h2>
        <form action="/set" method="post">
)rawliteral";

/**
 * --- 웹 설정 폼 섹션 (사용자 입력 필드) ---
 * 유튜브 커스터마이징, 미세먼지 관측소, 기상청 동네 코드, 시간대 설정을 담당합니다.
 */
const char INDEX_HTML_SETTINGS[] PROGMEM = R"rawliteral(
            <div class="section">
                <h3>▶️ YouTube 채널 설정</h3>
                <p>구독자 수를 확인할 유튜브 채널명을 입력하세요.</p>
                <input type="text" name="channel" placeholder="현재: %CHANNEL%">
            </div>
            <div class="section">
                <h3>😷 미세먼지 측정소 위치</h3>
                <p>네이버 미세먼지 검색 기준 지역(예: 가산동/읍/면)을 입력하세요.</p>
                <input type="text" name="location" placeholder="현재: %LOCATION%">
            </div>
            <div class="section">
                <h3>☀️ 날씨 동네 (법정동코드)</h3>
                <p>기상청 예보 기준 10자리 법정동코드를 입력하세요.</p>
                <input type="text" name="weather_code" placeholder="현재: %WEATHER%">
            </div>
            <div class="section">
                <h3>⏰ 시간대 (Timezone) 설정</h3>
                <p>UTC 기준 오프셋 시간을 입력하세요. (대한민국: 9)</p>
                <input type="text" name="tz_offset" placeholder="현재: +%TZ%">
            </div>
            <input type="submit" class="btn" value="💾 설정 저장 후 즉시 반영">
        </form>
    </div>
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
</body>
</html>
)rawliteral";

#endif
