// worynim@gmail.com
/**
 * @file Screen_To_OLEDs.ino
 * @brief PC 화면 스트리밍 수신 및 4분할 OLED 출력용 펌웨어
 * @details PC로부터 전송된 비트맵 데이터를 수신하여 4개의 OLED 디스플레이에 분할 및 실시간 렌더링 처리
 */

#include <Arduino.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <Ticker.h>
#include <Preferences.h>
#include "config.h"
#include "DisplayEngine.h"

// ==========================================
// [전역 상태 변수]
// ==========================================
Preferences prefs;

// 화면 플립 상태 (DisplayEngine.cpp에서 extern으로 참조)
bool is_flipped = true;  // DeskStream 설치 방향 기본값: 플립 ON

// 밝기 (0~255)
uint8_t brightness_val = 1;

// FPS 카운터 및 타이머
Ticker fps_ticker;
bool is_streaming = false;

// ==========================================
// [UDP 수신 변수 - 더블 버퍼링]
// ==========================================
//
// [F2] 더블 버퍼: recv_buf(조립중) ⇔ ready_buf(렌더링 대기)
// pushCanvas() 32ms 블로킹 중에도 recv_buf는 새 프레임을 받을 수 있음
uint8_t  frameBufferA[CANVAS_BYTES];   // 수신 조립 버퍼 A
uint8_t  frameBufferB[CANVAS_BYTES];   // 수신 조립 버퍼 B
uint8_t* recv_buf     = frameBufferA;  // 현재 조립 중인 버퍼 (스왜 전환)
uint8_t* ready_buf    = frameBufferB;  // 렌더링 대기 중인 버퍼
volatile bool frame_ready = false;     // 컨버스 갱신 요청 플래그

WiFiUDP udp;
WiFiUDP discoveryUDP;

uint8_t  packet_buffer[MAX_PACKET_SIZE];

uint8_t  current_frame_id      = 0;
uint8_t  total_chunks_for_frame = 0;
bool     is_assembling          = false;
uint8_t  received_chunk_mask    = 0;   // 4 청크만 사용하므로 1 byte bitmask로 충분
uint16_t current_data_len       = 0;

unsigned long last_packet_time   = 0;
unsigned long last_discovery_time = 0;
unsigned long last_wifi_check_time = 0;

// ==========================================
// [버튼 상태 변수]
// ==========================================
// BTN1 (FLIP)
bool btn1_last_state       = HIGH;
unsigned long btn1_fall_time = 0;
bool btn1_long_press_fired = false;

// BTN2 (밝기DOWN / 도움말)
bool btn2_last_state       = HIGH;
unsigned long btn2_fall_time = 0;
bool btn2_long_press_fired = false;

// BTN3 (밝기UP)
bool btn3_last_state       = HIGH;
unsigned long btn3_fall_time = 0;

// BTN4 (예비)
bool btn4_last_state       = HIGH;
unsigned long btn4_fall_time = 0;

// 도움말 모드
bool is_help_mode = false;

Ticker buzzer_ticker;

void stopBuzzer() {
    noTone(BUZZER_PIN);
}

void beepAsync(int duration = 30, int freq = 2000) {
    tone(BUZZER_PIN, freq);
    buzzer_ticker.once_ms(duration, stopBuzzer);
}

// 기존 updateTone 및 melody 관련 변수들은 단순화를 위해 제거하거나 무시 가능
// 여기서는 가장 요청이 많은 비프음을 Ticker로 정확히 제어하게 함
void updateTone() {
    // Ticker가 자동으로 끄므로 루프에서의 업데이트는 불필요
}

// ==========================================
// [화면 제어 함수]
// ==========================================

/**
 * @brief 플립 상태를 모든 OLED 하드웨어에 적용 (SSD1306 세그먼트/COM 반전)
 */
void applyFlipState(bool state) {
    uint8_t seg = state ? 0xA1 : 0xA0;
    uint8_t com = state ? 0xC8 : 0xC0;
    for (int i = 0; i < NUM_SCREENS; i++) {
        DisplayEngine::screens[i]->sendF("c", seg);
        DisplayEngine::screens[i]->sendF("c", com);
    }
}

/**
 * @brief 화면 플립 토글 + NVS 저장
 */
void toggleFlip() {
    is_flipped = !is_flipped;
    applyFlipState(is_flipped);
    // 스트리밍 중이 아닐 때 즉시 현재 UI 화면을 다시 그려서 순서 변경 반영
    if (is_help_mode) drawHelpScreen();
    else if (!is_streaming) drawWaitingScreen();
    
    prefs.begin("sto_set", false);
    prefs.putBool("flip", is_flipped);
    prefs.end();
    Serial.printf("[BTN1] Flip: %s\n", is_flipped ? "ON (180°)" : "OFF (Normal)");
}

/**
 * @brief 밝기 순환 (1 -> 50 -> 100 -> 240)
 */
void cycleBrightness() {
    if (brightness_val < 50)       brightness_val = 50;
    else if (brightness_val < 100) brightness_val = 100;
    else if (brightness_val < 240) brightness_val = 240;
    else                           brightness_val = 1;

    // 모든 OLED에 밝기 명령 전송 (SSD1306: 0x81 = Set Contrast)
    for (int i = 0; i < NUM_SCREENS; i++) {
        DisplayEngine::screens[i]->sendF("ca", 0x81, brightness_val);
    }
    if (is_help_mode) drawHelpScreen();
    
    prefs.begin("sto_set", false);
    prefs.putUChar("bright", brightness_val);
    prefs.end();
    Serial.printf("[BTN] Brightness Cycled: %d\n", brightness_val);
}

/**
 * @brief 버튼 도움말 화면 렌더링 (4개 OLED 영역에 각 버튼 기능 표시)
 */
void drawHelpScreen() {
    LGFX_Sprite& canvas = display.getCanvas();
    canvas.clear();
    canvas.setFont(&fonts::Font0);  // 기본 6x8 고정폭 폰트

    // OLED 1 영역 (x: 0~127) — BTN1 (GPIO 1)
    canvas.drawCenterString("- BTN 1 -", 64, 2);
    canvas.drawLine(0, 12, 127, 12, COLOR_WHITE);
    canvas.drawString(" L: FLIP SCREEN", 4, 22);
    canvas.drawString("   (180 ROTATE)", 4, 34);

    // OLED 2 영역 (x: 128~255) — BTN2 (GPIO 4)
    canvas.drawCenterString("- BTN 2 -", 128 + 64, 2);
    canvas.drawLine(128, 12, 255, 12, COLOR_WHITE);
    canvas.drawString(" S: BRIGHT CYCLE", 132, 22);
    
    // 밝기 상태 사각형 표시 (다른 프로젝트 방식 참고)
    int level = 0;
    if (brightness_val == 50) level = 1;
    else if (brightness_val == 100) level = 2;
    else if (brightness_val == 240) level = 3;

    int boxSize = 8;
    int gap = 4;
    int startX = 128 + (128 - (boxSize * 4 + gap * 3)) / 2;
    int startY = 38;
    for (int j = 0; j < 4; j++) {
        if (j <= level) {
            canvas.fillRect(startX + j * (boxSize + gap), startY, boxSize, boxSize, COLOR_WHITE);
        } else {
            canvas.drawRect(startX + j * (boxSize + gap), startY, boxSize, boxSize, COLOR_WHITE);
        }
    }
    canvas.drawString(" L: THIS HELP", 132, 52);

    // OLED 3 영역 (x: 256~383) — BTN3 (GPIO 10)
    canvas.drawCenterString("- BTN 3 -", 256 + 64, 2);
    canvas.drawLine(256, 12, 383, 12, COLOR_WHITE);
    canvas.drawString(" (NOT USED)", 260, 22);
    canvas.drawString(" ", 260, 34);

    // OLED 4 영역 (x: 384~511) — BTN4 (GPIO 9)
    canvas.drawCenterString("- BTN 4 -", 384 + 64, 2);
    canvas.drawLine(384, 12, 511, 12, COLOR_WHITE);
    canvas.drawString(" (RESERVED)", 388, 22);

    display.pushCanvas(true); // 도움말은 플립과 무관하게 버튼 위치에 맞춰 고정 출력
    Serial.println("[HELP] Screen drawn");
}

/**
 * @brief 연결 대기 상태 화면 (IP 표시)
 */
void drawWaitingScreen() {
    LGFX_Sprite& canvas = display.getCanvas();
    canvas.clear();
    
    // IP 주소를 4개 부분으로 나누어 각 OLED(128px 단위) 중앙에 표시
    IPAddress ip = WiFi.localIP();
    canvas.setFont(&fonts::FreeSansBold18pt7b);
    
    for (int i = 0; i < 4; i++) {
        // 각 화면의 중앙 x 좌표 계산
        int centerX = (i * 128) + 64;
        canvas.drawCenterString(String(ip[i]).c_str(), centerX, 12);
        
        // 옥텟 사이에 점(.) 표시 (선택 사항, 필요시)
        if (i < 3) {
            canvas.setFont(&fonts::Font0); // 작은 폰트로 점 표시
            canvas.drawCenterString(".", (i * 128) + 128, 30);
            canvas.setFont(&fonts::FreeSansBold18pt7b);
        }
    }
    
    canvas.setFont(&fonts::Font0);
    canvas.drawCenterString("Waiting for stream...", CANVAS_WIDTH / 2, 50);
    display.pushCanvas();
}

// ==========================================
// [UDP 네트워크 함수]
// ==========================================

void sendDiscoveryPacket() {
    IPAddress broadcastIP = WiFi.localIP();
    broadcastIP[3] = 255;
    discoveryUDP.beginPacket(broadcastIP, DISCOVERY_PORT);
    discoveryUDP.print(DISCOVERY_MSG);
    discoveryUDP.endPacket();
}

void reset_assembly() {
    is_assembling        = false;
    received_chunk_mask  = 0;
    current_data_len     = 0;
}

/**
 * @brief UDP 패킷 처리 루프 (큐에 있는 모든 패킷 묶음 처리)
 * 
 * [F1+F2 개선] 핵심 변경사항:
 *   이전: while 루프 내부에서 pushCanvas() 호용 → 32ms 블로킹 중 UDP 누락
 *   이후: 모든 UDP 패킷 소진 시까지 recv_buf에 조립 → 완성 시 스왜 → while 끝에서 pushCanvas()
 *   → pushCanvas() 32ms 동안 lwIP 버퍼에 쌓인 다음 프레임 패킷들을 다음 호출에서 모두 획득
 */
void processUDP() {
    unsigned long currentMillis = millis();

    // WiFi 상태 체크 (2초 주기)
    if (currentMillis - last_wifi_check_time >= WIFI_CHECK_INTERVAL) {
        last_wifi_check_time = currentMillis;
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WIFI] Disconnected. Restarting...");
            ESP.restart();
        }
    }

    int packetSize = udp.parsePacket();

    if (!packetSize) {
        // 패킷 없을 때: Discovery 브로드캐스트 또는 타임아웃 처리
        if (!is_streaming) {
            if (currentMillis - last_discovery_time >= DISCOVERY_INTERVAL) {
                last_discovery_time = currentMillis;
                sendDiscoveryPacket();
            }
        } else {
            if (currentMillis - last_packet_time > FRAME_RECV_TIMEOUT_MS) {
                Serial.println("[UDP] Frame timeout. Reset.");
                is_streaming = false;
                reset_assembly();
                drawWaitingScreen(); // 스트리밍 끊기면 다시 대기화면(IP) 표시
            }
        }
        return;
    }

    // 큐에 있는 모든 패킷 먼저 소진 (한 루프 안에서 묶음 수신)
    // ★ pushCanvas()는 절대 이 루프 안에서 호출하지 않음
    while (packetSize > 0) {
        int len = udp.read(packet_buffer, MAX_PACKET_SIZE);
        if (len < HEADER_SIZE) {
            packetSize = udp.parsePacket();
            continue;
        }

        last_packet_time = currentMillis;
        is_streaming     = true;

        uint8_t frame_id     = packet_buffer[0];
        uint8_t total_chunks = packet_buffer[1];
        uint8_t chunk_index  = packet_buffer[2];

        // 새 프레임 시작 감지
        if (current_frame_id != frame_id) {
            current_frame_id        = frame_id;
            total_chunks_for_frame  = total_chunks;
            reset_assembly();
            is_assembling = true;
        }

        if (is_assembling) {
            uint8_t chunk_bit = (1 << chunk_index);
            if (!(received_chunk_mask & chunk_bit)) {
                received_chunk_mask |= chunk_bit;
                int data_len = len - HEADER_SIZE;
                int offset   = chunk_index * CHUNK_PAYLOAD_SIZE;
                if (offset + data_len <= (int)sizeof(frameBufferA)) {
                    // recv_buf(조립 중 버퍼)에 첨부
                    memcpy(&recv_buf[offset], &packet_buffer[HEADER_SIZE], data_len);
                    if (offset + data_len > current_data_len)
                        current_data_len = offset + data_len;
                }

                // 모든 청크 수신 완료 확인
                uint8_t all_mask = (total_chunks_for_frame >= 8)
                                   ? 0xFF
                                   : (uint8_t)((1 << total_chunks_for_frame) - 1);

                if (received_chunk_mask == all_mask) {
                    is_assembling = false;

                    if (!is_help_mode && current_data_len == CANVAS_BYTES) {
                        // 순쉘: recv_buf 스왜 → ready_buf로 이동
                        // recv_buf는 즉시 다음 프레임 조립에 사용 가능
                        uint8_t* tmp = recv_buf;
                        recv_buf  = ready_buf;
                        ready_buf = tmp;
                        frame_ready = true;  // while 루프 밖에서 pushCanvas() 호출
                        display.incrementFrameCounter();
                    }
                    reset_assembly();
                }
            }
        }

        packetSize = udp.parsePacket();
    }

    // ★ [F1] UDP 패킷을 모두 소진한 후 렌더링
    // pushCanvas() 32ms 블로킹이 다음 while 루프에 영향을 주지 않음
    if (frame_ready && !is_help_mode) {
        frame_ready = false;
        uint8_t* canvas_buf = (uint8_t*)display.getCanvas().getBuffer();
        if (canvas_buf) {
            memcpy(canvas_buf, ready_buf, CANVAS_BYTES);
            display.pushCanvas();
        }
    }
}

// ==========================================
// [버튼 처리 함수]
// ==========================================

void processButtons() {
    unsigned long now = millis();

    // ── BTN1: Long=화면 플립 ──
    bool b1 = digitalRead(BTN1_PIN);
    if (btn1_last_state == HIGH && b1 == LOW) {
        btn1_fall_time        = now;
        btn1_long_press_fired = false;
    } else if (b1 == LOW && !btn1_long_press_fired) {
        if (now - btn1_fall_time > BTN_LONG_PRESS_MS) {
            btn1_long_press_fired = true;
            toggleFlip();
            beepAsync(20, 2000);
        }
    }
    btn1_last_state = b1;

    // ── BTN2: Short=밝기 순환 / Long=도움말 토글 ──
    bool b2 = digitalRead(BTN2_PIN);
    if (btn2_last_state == HIGH && b2 == LOW) {
        btn2_fall_time        = now;
        btn2_long_press_fired = false;
    } else if (b2 == LOW && !btn2_long_press_fired) {
        if (now - btn2_fall_time > BTN_LONG_PRESS_MS) {
            btn2_long_press_fired = true;
            is_help_mode = !is_help_mode;
            beepAsync(30, 2000);
            if (is_help_mode) drawHelpScreen();
            else { display.clearCanvas(); display.pushCanvas(); }
        }
    } else if (btn2_last_state == LOW && b2 == HIGH) {
        // 떼질 때 숏프레스 판정
        if (!btn2_long_press_fired && (now - btn2_fall_time) > BTN_DEBOUNCE_MS) {
            cycleBrightness();
            beepAsync(20, 3000);
        }
    }
    btn2_last_state = b2;

    // ── BTN3: 미사용 ──
    bool b3 = digitalRead(BTN3_PIN);
    btn3_last_state = b3;

    // ── BTN4: 예비 (현재 미할당) ──
    bool b4 = digitalRead(BTN4_PIN);
    btn4_last_state = b4;
}

// ==========================================
// [FPS 보고 콜백]
// ==========================================
void onFpsReport() {
    Serial.printf("FPS: %u\n", display.getFrameCounter());
    display.resetFrameCounter();
}

// ==========================================
// [setup()]
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(500);

    // 디스플레이 엔진 초기화 (I2C, 캔버스, 백그라운드 태스크)
    display.begin();

    // NVS에서 설정값 복원
    prefs.begin("sto_set", true);
    is_flipped     = prefs.getBool("flip",   true);
    brightness_val = prefs.getUChar("bright", 128);
    prefs.end();

    applyFlipState(is_flipped);
    // 밝기 초기 적용
    for (int i = 0; i < NUM_SCREENS; i++) {
        DisplayEngine::screens[i]->sendF("ca", 0x81, brightness_val);
    }

    // 버튼 핀 초기화
    pinMode(BTN1_PIN,   INPUT_PULLUP);
    pinMode(BTN2_PIN,   INPUT_PULLUP);
    pinMode(BTN3_PIN,   INPUT_PULLUP);
    pinMode(BTN4_PIN,   INPUT_PULLUP);

    // 부저 초기화
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    // FPS 타이머
    fps_ticker.attach(1.0, onFpsReport);

    // 부팅 메시지 + AP 연결 안내문 (연결 전)
    LGFX_Sprite& canvas = display.getCanvas();
    canvas.clear();
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.drawCenterString("Screen To OLEDs", CANVAS_WIDTH / 2, 10);
    canvas.setFont(&fonts::Font0);
    canvas.drawCenterString("Connecting WiFi...", CANVAS_WIDTH / 2, 34);
    canvas.drawCenterString("AP: ESP_SCREEN_OLED", CANVAS_WIDTH / 2, 48);
    display.pushCanvas();

    // WiFi 연결 (WiFiManager: 자동 AP 포털)
    WiFiManager wm;
    wm.setConfigPortalTimeout(120);
    if (!wm.autoConnect("ESP_SCREEN_OLED")) {
        Serial.println("[WIFI] Connection Failed. Restarting...");
        ESP.restart();
    }
    WiFi.setSleep(false);

    // 연결 성공 후 IP 주소 표시 (함수로 통일)
    drawWaitingScreen();
    Serial.println("[WIFI] Connected: " + WiFi.localIP().toString());

    // UDP 시작
    udp.begin(UDP_PORT);
    discoveryUDP.begin(DISCOVERY_PORT);

    // 연결음 짧게
    beepAsync(30, 3000);
    delay(BOOT_MESSAGE_DELAY_MS);
}

// ==========================================
// [loop()]
// ==========================================
void loop() {
    // 비차단 부저 업데이트 (매 사이클 필수)
    updateTone();

    // 버튼 처리
    processButtons();

    // UDP 수신 및 화면 갱신
    processUDP();
}
