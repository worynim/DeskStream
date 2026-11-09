// worynim@gmail.com
/**
 * @file web_manager.cpp
 * @brief 웹 설정 대시보드 및 API 서버 클래스 구현
 * @details 페이지·폰트 업로드·캐시 갱신 핸들러. 설정 JSON 입출력은 ds::WebConfigApi.
 */
#include "web_manager.h"
#include "config.h"          // FONT_SLOT_COUNT
#include "web_pages.h"

// 전역 객체 정의
WebServer server(WEB_PORT);
WebManager webManager;

WebManager::WebManager() {}

/** GET /api/config 응답에 슬롯 이름 목록을 덧붙인다 (드롭다운용) */
static void appendSlotNames(String& json) {
    json += ",\"slot_names\":[";
    for (int i = 0; i < FONT_SLOT_COUNT; i++) {
        if (i) json += ',';
        json += '"'; json += ds::jsonEscape(display.getSlotName(i)); json += '"';
    }
    json += ']';
}

/** POST /api/config 처리 후 — 다음 루프에서 즉시 재렌더링 */
static void afterConfigPost(const char* body) {
    (void)body;   // 필드 테이블 밖의 키가 없다
    display.setForceUpdate(true);
}

void WebManager::begin() {
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/upload", HTTP_POST, [this]() { server.send(200, "text/plain", "OK"); }, [this]() { handleUploadData(); });
    server.on("/api/refresh_cache", HTTP_POST, [this]() { handleRefreshCache(); });
    // 설정 GET/POST — config_manager의 필드 테이블(키·검증·적용 순서)로 처리한다
    _configApi.begin(server, configManager.store(), appendSlotNames, afterConfigPost);
    
    server.begin();
    Serial.println("[WEB] WebManager started");
}

void WebManager::handleClient() {
    server.handleClient();
}

void WebManager::handleRoot() {
    // [A-1 수정 — 2026-10-06, 중국어판 §12.12 승계] PROGMEM 페이지를 send()로 보내면
    //   ESP32 WebServer 코어가 `const String passStr = (String)content;`로 **전체를 힙에
    //   복사**한다(코어도 "Use send_P for long arrays"라고 경고한다). 이 프로젝트는
    //   I2C 버퍼 4장(u8g2_buffers)을 상시 점유하므로 연속 40~70 KB 할당이 실패하기 쉽고,
    //   그러면 서버는 정상인데 응답만 씻겨 나가 브라우저가 **백지**가 된다 —
    //   크래시도 로그도 없어 진단이 가장 어려운 부류다.
    //   send_P는 strlen_P()로 길이만 재고 스트림으로 내보내므로 복사가 없다.
    server.send_P(200, PSTR("text/html"), font_studio_html);
}

/**
 * @brief (슬롯,이름) 조합을 이번 배치에서 이미 처리했는지 — 로그 폭주 방지
 * @details 글리프 업로드는 파일 40개를 연속으로 보낸다. 이름표 쓰기는 setSlotName()이
 *          중복을 걸러 주지만, 로그는 여기서도 걸러야 한다(40줄이 흐르면 실패가 묻힌다).
 */
static bool nameLogged(uint8_t slot, const String& name) {
    static int32_t lastSlot = -1;
    static String  lastName;
    if (lastSlot == (int32_t)slot && lastName == name) return true;
    lastSlot = (int32_t)slot;
    lastName = name;
    return false;
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
    // 이번 업로드가 **어느 슬롯**으로 가고, 그 슬롯에 **어떤 폰트**가 구워지는지.
    //   파일마다(40회) 이름표를 쓰지 않도록 START에서 한 번만 읽어 두고 END에서 쓴다.
    static uint8_t uploadSlot = 0;
    static String  uploadFontName;

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
            uploadSlot = (uint8_t)slot;

            // [A-2① 수정 — 2026-10-06, 중국어판 §12.13 승계] 폰트 이름을 **업로드와 같은
            //   요청으로** 받는다(?font=<파일명>). 예전엔 이름이 별도의 /api/config POST로만
            //   들어왔고 그마저 font 파일이 선택된 경우에만 나갔다. 그 POST가 빠지거나 늦으면
            //   슬롯에 글리프만 남고 이름표(/fN/name.txt)가 없어 드롭다운이 "Empty Slot"으로
            //   보였다. 이제 이름이 슬롯과 **같은 요청에 실려** 오므로 도착점이 어긋날 수 없다.
            //   빈 값이면 이름표를 건드리지 않는다 — 기존 이름을 지우지 않기 위해서다.
            uploadFontName = server.hasArg("font") ? server.arg("font") : "";

            // 🔴 **진단용 — 지우지 말 것.** 중국어판에서 이 결함을 세 번 고치는 동안
            //   가장 어려웠던 것은 코드가 아니라 **침묵**이었다: 이름표 쓰기 로그가
            //   "이름이 있을 때만" 찍히게 되어 있어 **이름이 아예 안 왔다는 사실**이
            //   보이지 않았다. 그래서 START에서 무조건 찍는다.
            //   fontArg=0 이면 서버가 못 본 것(브라우저가 안 보냈거나 파싱 실패),
            //   fontArg=1 인데 이름표가 안 붙으면 그 뒤 단계(setSlotName)의 문제다.
            Serial.printf("[WEB] upload start slot=%d fontArg=%d font='%s'\n",
                          slot, (int)server.hasArg("font"), uploadFontName.c_str());

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

        case UPLOAD_FILE_END: {
            // 글리프가 실제로 저장된 경우에만 이름표를 쓴다. open 실패·경로 거부로
            //   uploadActive가 서지 않았다면 **이름만 붙은 빈 슬롯**이 되어 버린다 —
            //   그러면 "폰트가 안 뜨는데 이름은 있다"는 더 헷갈리는 상태가 된다.
            const bool wroteFile = (uploadActive && fsUploadFile);
            if (wroteFile) fsUploadFile.close();
            uploadActive = false;

            // [A-2③ 수정] 이름표를 **업로드한 슬롯**에 쓴다. setFontName()을 쓰면 안 된다 —
            //   그쪽은 configManager.font_slot(장치의 **현재** 슬롯)에 쓰므로, 업로드 슬롯과
            //   다르면 엉뚱한 폴더에 이름표가 남는다(이 결함의 원래 형태).
            //   setSlotName()은 같은 값이면 파일을 다시 쓰지 않으므로 40회 호출이 모두
            //   도착해도 실제 쓰기는 슬롯당 한 번이다. 거부되면 이유를 남긴다 —
            //   예전에는 조용히 return해서 "왜 비어 보이는지" 알 수 없었다.
            if (wroteFile && uploadFontName.length() > 0 && !nameLogged(uploadSlot, uploadFontName)) {
                if (display.setSlotName(uploadSlot, uploadFontName)) {
                    Serial.printf("[WEB] Slot %u name = %s\n", (unsigned)uploadSlot, uploadFontName.c_str());
                } else {
                    Serial.printf("[WEB] Slot name rejected: slot=%u name='%s' (len=%u; 1..%d bytes, no \" \\ /)\n",
                                  (unsigned)uploadSlot, uploadFontName.c_str(),
                                  (unsigned)uploadFontName.length(), FONT_NAME_MAX_LEN);
                }
            }
            break;
        }

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
