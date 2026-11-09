// worynim@gmail.com
/**
 * @file Game_Of_Life.ino
 * @brief 512x64 Conway's Game of Life — 화면 전송·버튼·부저는 DeskStreamCore
 */
#include <Arduino.h>
#include <DeskStreamCore.h>
#include "engine.h"

// 4-OLED 화면 엔진 (HW 1.1MHz·병합 전송·고속 SW I2C·셰도 diff) — DeskStreamCore
static ds::DisplayCore display;
static U8G2** screens = display.screens();
static ds::Buttons buttons;
static ds::Buzzer buzzer;

// UI State variables
bool isPlaying = true;
int fpsMode = 1; // 0=5fps, 1=10fps, 2=20fps
int frameDelay = 100; // approx 10fps

volatile bool btnFlag[4] = {false, false, false, false};

// 시작 멜로디 Do-Mi-Sol-Do (100ms 음 + 20ms 쉼) — ds::Buzzer가 타이머로 재생한다
static const ds::Note STARTUP_MELODY[] = {{2093, 100, 20}, {2637, 100, 20}, {3136, 100, 20}, {4186, 100, 20}};

// 버튼 PRESS(누르는 순간) → 플래그. 실제 처리는 메인 루프의 processButtonFlags()가 한다.
static void onPress0() { btnFlag[0] = true; }
static void onPress1() { btnFlag[1] = true; }
static void onPress2() { btnFlag[2] = true; }
static void onPress3() { btnFlag[3] = true; }

/**
 * @brief 버튼 전용 백그라운드 태스크 — 5ms마다 스캔 (렌더링 루프와 독립적)
 * @details 메인 루프의 세대 계산·전송 중에도 짧은 입력을 놓치지 않기 위해 별도 태스크에서 돈다.
 *          판정(20ms 안정 디바운스 + 누르는 순간 PRESS)은 ds::Buttons가 한다.
 */
void buttonTask(void* parameter) {
    while (true) {
        buttons.update();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void processButtonFlags() {
    if (btnFlag[0]) { // BTN1: Play/Pause
        btnFlag[0] = false;
        isPlaying = !isPlaying;
        buzzer.beep(50, isPlaying ? 2000 : 1000);
    }
    if (btnFlag[1]) { // BTN2: Step
        btnFlag[1] = false;
        if (!isPlaying) {
            engine.computeNextGeneration();
            mapEngineToBuffers();
            pushParallel();
            buzzer.beep(30, 1500);
        }
    }
    if (btnFlag[2]) { // BTN3: Speed Toggle
        btnFlag[2] = false;
        fpsMode = (fpsMode + 1) % 3;
        if (fpsMode == 0) frameDelay = 200; // 5fps
        else if (fpsMode == 1) frameDelay = 100;  // 10fps
        else frameDelay = 50;  // 20fps
        buzzer.beep(50, 1000 + fpsMode * 500);
    }
    if (btnFlag[3]) { // BTN4: Reset Pattern
        btnFlag[3] = false;
        engine.randomize();
        if (!isPlaying) {
            mapEngineToBuffers();
            pushParallel();
        }
        buzzer.beep(100, 800);
    }
}

/** 바뀐 페이지만 4화면에 병렬 전송한다 (HW 0·1은 태스크, SW 2·3은 이 태스크) */
void pushParallel() {
    display.push();
}

/**
 * @brief 엔진의 512x64 비트맵(행 우선·MSB 먼저)을 4화면의 U8g2 페이지 버퍼로 전치한다
 * @details 전치 규칙은 DeskStreamCore의 ds::canvasToScreenPages() (호스트 테스트 test_canvas)를 쓴다.
 */
void mapEngineToBuffers() {
    const uint8_t* current_gen = engine.getCurrentBuffer();
    for (int s = 0; s < ds::kNumScreens; s++) {
        ds::canvasToScreenPages(current_gen, GRID_WIDTH, (uint8_t)s, screens[s]->getBufferPtr());
    }
}

void setup() {
    Serial.begin(115200);

    // ESP32-C3 USB CDC(HWCDC): 호스트가 포트를 읽지 않아 TX 링(256B)이 차면
    // HWCDC::write()가 호출당 최대 2초(100ms × 20회)를 블로킹해 loop()가 멈춘다.
    // 버튼·부저 등 실시간 처리가 지연되지 않도록 TX 타임아웃을 0(비차단)으로.
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(0);
#endif
    delay(1000);
    Serial.println("\n[System] Game of Life - Booting...");

    // 버튼(20ms 안정 디바운스 · 누르는 순간 반응) 및 부저 초기화
    ds::ButtonTiming timing;
    timing.stableMs = 20;
    buttons.begin(timing);
    buttons.setCallback(0, ds::BUTTON_PRESS, onPress0);
    buttons.setCallback(1, ds::BUTTON_PRESS, onPress1);
    buttons.setCallback(2, ds::BUTTON_PRESS, onPress2);
    buttons.setCallback(3, ds::BUTTON_PRESS, onPress3);
    buzzer.begin();

    // 버튼 전용 백그라운드 태스크 시작
    xTaskCreate(buttonTask, "ButtonTask", 2048, NULL, 5, NULL);

    // 화면 엔진 초기화 (I2C 버스·병렬 전송 태스크·U8G2 4개)
    //   엔진 비트맵을 U8g2 버퍼에 **직접** 쓰므로(mapEngineToBuffers) 소프트웨어 회전 없이(R0)
    //   하드웨어 flip으로 상하를 뒤집는다 — 이전 구성(U8G2_R0 + setFlipMode(1)) 그대로다.
    Serial.println("[System] Initializing Displays...");
    ds::DisplayConfig cfg;
    cfg.rotation = U8G2_R0;
    display.begin(cfg);
    display.setFlip(true);

    const char* splash_texts[4] = {"Conway's", "GAME", "OF", "LIFE"};
    for (int i = 0; i < ds::kNumScreens; i++) {
        screens[i]->clearBuffer();
        if (i == 0) screens[i]->setFont(u8g2_font_ncenB14_tr);
        else screens[i]->setFont(u8g2_font_logisoso24_tr);
        
        int strWidth = screens[i]->getStrWidth(splash_texts[i]);
        int x = (128 - strWidth) / 2;
        int y = (i == 0) ? 40 : 45;
        
        screens[i]->setCursor(x, y);
        screens[i]->print(splash_texts[i]);
    }
    pushParallel(); // 병렬 전송으로 4개 화면 동시 출력
    
    buzzer.playMelody(STARTUP_MELODY, sizeof(STARTUP_MELODY) / sizeof(STARTUP_MELODY[0]));
    delay(2000); // 스플래시 화면 2초 대기 (멜로디는 그동안 백그라운드로 재생된다)

    Serial.println("[System] Initializing Game Engine...");
    engine.init();

    Serial.println("[System] Setup Complete!");
}

void loop() {
    unsigned long loop_start = millis();

    processButtonFlags();

    if (isPlaying) {
        // unsigned long start_time = micros();
        
        engine.computeNextGeneration();
        mapEngineToBuffers();
        pushParallel();
        
        // unsigned long duration = micros() - start_time;
        // Serial.print("Generation & Render in: ");
        // Serial.print(duration);
        // Serial.println(" us");
    }
    
    // FPS 조절 대기 로직 (버튼 반응성을 위해 짧게 나눠서 대기)
    while (millis() - loop_start < frameDelay) {
        processButtonFlags();
        delay(5);
    }
}
