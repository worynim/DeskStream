// worynim@gmail.com
/**
 * @file web_manager.cpp
 * @brief 웹 설정 대시보드 및 API 서버 클래스 구현
 * @details 비동기 HTTP 핸들러 및 JSON 설정 데이터 입출력 로직 구현
 * @note [SYNC] 원본: Hangeul_Clock/web_manager.cpp — 현재 바이트 단위로 동일. 한글판 수정 시 함께 반영할 것.
 */
#include "web_manager.h"
#include "config.h"          // FONT_SLOT_COUNT
#include "web_pages.h"
#include "tz_util.h"
#include <string.h>   // strlen — JSON 키 파싱 오프셋 계산용

// 전역 객체 정의
WebServer server(WEB_PORT);
WebManager webManager;

/**
 * @brief 문자열을 JSON 문자열 리터럴 본문으로 이스케이프한다 (따옴표·역슬래시 없는 형태)
 * @details [리뷰 §1.3] /api/config 응답을 문자열 이어붙이기로 만들기 때문에,
 *          사용자 입력(font_name·slot_names)에 " 또는 \ 가 들어가면 JSON이 깨지고
 *          웹 UI의 res.json()이 예외를 내 "Failed to load settings"로 뭉개진다.
 *          **입력 검증으로는 막을 수 없다** — 슬롯 이름은 폰트 파일 업로드 경로로도
 *          들어오기 때문이다. 출력 지점에서 확실히 이스케이프한다.
 *
 * @note 제어문자(< 0x20)는 \u00XX로, 유니코드(>= 0x80)는 UTF-8 바이트를 그대로 둔다.
 *       JSON은 UTF-8을 원본 그대로 허용하므로 한글 폰트 이름도 보존된다.
 */
static String jsonEscape(const String& s) {
    String out;
    out.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
        const char c = s[i];
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c < 0x20) {
            char u[7];
            snprintf(u, sizeof(u), "\\u%04x", (unsigned char)c);
            out += u;
        } else {
            out += c;   // UTF-8 연속 바이트는 그대로 통과
        }
    }
    return out;
}

WebManager::WebManager() {}

void WebManager::begin() {
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/upload", HTTP_POST, [this]() { server.send(200, "text/plain", "OK"); }, [this]() { handleUploadData(); });
    server.on("/api/refresh_cache", HTTP_POST, [this]() { handleRefreshCache(); });
    server.on("/api/config", HTTP_GET, [this]() { handleGetConfig(); });
    server.on("/api/config", HTTP_POST, [this]() { handleSetConfig(); });
    
    server.begin();
    Serial.println("[WEB] WebManager started");
}

void WebManager::handleClient() {
    server.handleClient();
}

void WebManager::handleRoot() {
    server.send(200, "text/html", font_studio_html);
}

void WebManager::handleUploadData() {
    HTTPUpload& upload = server.upload();
    // ESP32 WebServer는 업로드 콜백에 상태를 넘겨주지 않으므로 파일 핸들을 static으로 유지한다.
    //   단 그 상태를 그대로 신뢰하면 안 된다 — 다음 세 경우를 모두 막아야 한다:
    //     1) 경로 검증 실패로 START가 return하거나, START 없이 WRITE가 먼저 오는 경우
    //        → 이전 업로드의 열린 파일에 이번 내용이 계속 쓰인다
    //     2) 클라이언트 연결 중단(UPLOAD_FILE_ABORTED) → 파일이 열린 채 남는다
    //     3) LittleFS.open 실패 → falsy 핸들이 WRITE마다 검사돼야 한다
    //   uploadActive는 "열린 핸들이 이번 업로드 소유"라는 불변식을 나타낸다.
    static File fsUploadFile;
    static bool uploadActive = false;

    switch (upload.status) {
        case UPLOAD_FILE_START: {
            // 이전 업로드가 미완료로 방치된 경우(경로 거부·연결 중단) 핸들을 정리한다.
            if (uploadActive && fsUploadFile) fsUploadFile.close();
            uploadActive = false;

            String filename = upload.filename;
            if (!filename.startsWith("/")) filename = "/" + filename;

            // 슬롯 파라미터 확인 (예: /upload?slot=1)
            int slot = 0;
            if (server.hasArg("slot")) {
                slot = server.arg("slot").toInt();
                if (slot < 0 || slot >= FONT_SLOT_COUNT) slot = 0;
            }

            String path = "/f" + String(slot);
            if (!LittleFS.exists(path)) LittleFS.mkdir(path);

            String fullPath = path + filename;
            if (fullPath.indexOf("..") != -1) {
                Serial.println("[WEB] Invalid path detected: " + fullPath);
                return;   // uploadActive는 이미 false — 이후 WRITE는 아무 데도 쓰지 않는다
            }
            fsUploadFile = LittleFS.open(fullPath, "w");
            if (!fsUploadFile) {
                Serial.println("[WEB] Failed to open for write: " + fullPath);
                return;   // 열지 못했으므로 uploadActive를 세우지 않는다
            }
            uploadActive = true;
            break;
        }

        case UPLOAD_FILE_WRITE:
            // uploadActive가 false면 이번 업로드에 유효한 파일이 없다 (경로 거부·open 실패).
            //   이전 업로드의 핸들이 남아 있어도 **쓰지 않는다.**
            if (uploadActive && fsUploadFile) {
                fsUploadFile.write(upload.buf, upload.currentSize);
            }
            break;

        case UPLOAD_FILE_END:
            if (uploadActive && fsUploadFile) fsUploadFile.close();
            uploadActive = false;
            break;

        case UPLOAD_FILE_ABORTED:
            // 클라이언트가 전송 도중 끊음 — END 없이 여기서 끝나면 핸들이 새어 나간다.
            if (uploadActive && fsUploadFile) {
                fsUploadFile.close();
                Serial.println("[WEB] Upload aborted by client, handle closed");
            }
            uploadActive = false;
            break;
    }
}

void WebManager::handleRefreshCache() {
    display.loadBitmapCache();
    display.setForceUpdate(true);
    server.send(200, "text/plain", "OK");
}

void WebManager::handleGetConfig() {
    SystemSettings& s = configManager.get();
    String slotNames = "[";
    for (int i = 0; i < FONT_SLOT_COUNT; i++) {
        slotNames += "\"" + jsonEscape(display.getSlotName(i)) + "\"" + (i < FONT_SLOT_COUNT - 1 ? "," : "");
    }
    slotNames += "]";

    // [리뷰 §1.3] font_name은 이스케이프 없이 붙이면 JSON이 깨진다.
    String json = "{\"anim_mode\":" + String((int)s.anim_mode) +
                 ",\"display_mode\":" + String((int)s.display_mode) +
                 ",\"hour_format\":" + String((int)s.hour_format) +
                 ",\"chime_enabled\":" + String(s.chime_enabled ? "true":"false") +
                 ",\"font_name\":\"" + jsonEscape(s.font_name) +
                 "\",\"font_slot\":" + String((int)s.font_slot) +
                 ",\"slot_names\":" + slotNames +
                 ",\"is_inverted\":" + String(s.is_inverted ? "true":"false") +
                 ",\"brightness\":" + String((int)s.brightness) +
                 ",\"is_flipped\":" + String(s.is_flipped ? "true":"false") +
                 ",\"date_order\":" + String((int)s.date_order) +
                 ",\"timezone\":\"" + s.timezone + "\"}";
    server.send(200, "application/json", json);
}

void WebManager::handleSetConfig() {
    if (server.hasArg("plain")) {
        SystemSettings& s = configManager.get();
        String body = server.arg("plain");
        
        int am = parseVal(body, "anim_mode"); 
        if(am >= 0 && am < ANIMATION_TYPE_COUNT && am != s.anim_mode) display.setAnimMode(am);
        
        int dm = parseVal(body, "display_mode"); 
        if(dm >= 0 && dm <= CLOCK_MODE_NUMERIC && dm != s.display_mode) display.setDisplayMode(dm);
        
        int hf = parseVal(body, "hour_format"); 
        if(hf >= 0 && hf <= 1 && hf != s.hour_format) display.setHourFormat(hf);
        
        if (body.indexOf("\"chime_enabled\"") != -1) {
            bool val = parseBool(body, "chime_enabled");
            if(val != s.chime_enabled) display.setChime(val);
        }
        if (body.indexOf("\"is_flipped\"") != -1) {
            bool val = parseBool(body, "is_flipped");
            if(val != s.is_flipped) display.setFlipDisplay(val);
        }
        if (body.indexOf("\"is_inverted\"") != -1) {
            bool val = parseBool(body, "is_inverted");
            if(val != s.is_inverted) display.setInversion(val);
        }
        
        int fs = parseVal(body, "font_slot");
        if (fs >= 0 && fs < FONT_SLOT_COUNT && fs != s.font_slot) display.setFontSlot(fs);

        int br = parseVal(body, "brightness");
        if (br >= 1 && br <= 255 && br != s.brightness) display.setBrightness(br);

        // [수정할 사항 3] 24H 첫 화면의 날짜 순서. 값이 2개뿐이라 enum 범위만 보면 된다.
        int dor = parseVal(body, "date_order");
        if (dor >= DATE_ORDER_DAY_MONTH && dor <= DATE_ORDER_MONTH_DAY && dor != s.date_order) {
            display.setDateOrder((uint8_t)dor);
        }

        // [개선] POSIX TZ 문자열. setenv("TZ", ...)로 직접 들어가므로 화이트리스트 검증을 통과해야 한다.
        // font_name과 동일한 문자열 파싱 패턴을 쓰되, 검증은 tz_util::isValidTimezone()에 위임한다
        // (PLAN §6.6(f) — NVS 로드와 웹 입력이 같은 검증기를 써야 한다).
        //
        // [bug fix] 파싱 오프셋은 리터럴 실제 길이(strlen)로 계산한다.
        //   이전 코드는 font_name의 오프셋(13)을 복사해 timezone에 썼는데
        //   "\"timezone\":\""는 12자라서 첫 글자가 유실되었다 ("KST-9" → "ST-9").
        //   유실된 값은 어떤 프리셋과도 일치하지 않아 웹 UI가 Custom으로 표시되고,
        //   디바이스 시간도 서울 시간대로 바뀌지 않았다 (bug #2의 두 증상 모두).
        static const char TZ_JSON_KEY[] = "\"timezone\":\"";
        int tzS = body.indexOf(TZ_JSON_KEY);
        if (tzS != -1) {
            tzS += strlen(TZ_JSON_KEY);
            int tzE = body.indexOf('"', tzS);
            if (tzE > tzS) {
                String newTz = body.substring(tzS, tzE);
                if (!isValidTimezone(newTz.c_str())) {
                    Serial.println("[WEB] Rejected invalid timezone: " + newTz);
                } else if (newTz != s.timezone) {
                    strncpy(s.timezone, newTz.c_str(), sizeof(s.timezone) - 1);
                    s.timezone[sizeof(s.timezone) - 1] = '\0';   // NUL 종료 보장
                    configManager.setDirty();
                    display.applyTimezone();
                }
            }
        }

        // timezone과 같은 오프셋 버그가 재발하지 않도록 리터럴 길이를 직접 계산한다.
        static const char FONT_JSON_KEY[] = "\"font_name\":\"";
        int fnS = body.indexOf(FONT_JSON_KEY);
        if (fnS != -1) {
            fnS += strlen(FONT_JSON_KEY);
            int fnE = body.indexOf("\"", fnS);
            if (fnE != -1) {
                String fontName = body.substring(fnS, fnE);
                // [리뷰 §1.3] 길이·문자 검증은 DisplayManager::setFontName() 한 곳에서 한다.
                //   그곳이 JSON 출력과 파일명 쓰기 **둘 다**의 입구다. 여기서 한 번만 거르면
                //   나중에 다른 경로로 들어온 값은 검증 없이 통과한다.
                display.setFontName(fontName);
            }
        }
        
        display.setForceUpdate(true);
        server.send(200, "text/plain", "OK");
    }
}

int WebManager::parseVal(const String& body, const String& key) {
    int pos = body.indexOf("\"" + key + "\":");
    if (pos == -1) return -1;
    int start = pos + key.length() + 2;
    while (start < body.length() && !isDigit(body[start])) start++;
    String v = "";
    while (start < body.length() && isDigit(body[start])) v += body[start++];
    return v.length() > 0 ? v.toInt() : -1;
}

bool WebManager::parseBool(const String& body, const String& key) {
    return body.indexOf("\"" + key + "\":true") != -1;
}
