/**
 * @file display_handler.h
 * @author Antigravity
 * @brief 4개의 OLED 디스플레이 제어를 위한 전용 헤더 파일
 */

#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

#include <U8g2lib.h>
#include <Wire.h>

/**
 * @brief 대시보드 화면에 필요한 데이터를 한번에 묶어 전달하기 위한 구조체
 */
struct DashboardData {
    const char* timeStr;
    float speedVal;
    float courseVal;
    int usedSats;
    int visibleSats;
    const char* hdopStr;
    const char* statusStr;
    float latVal;
    float lonVal;
    float altVal;
    int calYear;
    int calMonth;
    int calDay;
    unsigned long tripTimeSec; // 주행 시간 (초)
    float tripDistance;        // 주행 거리 (km) - 차후 트립미터용
    bool showHelp;             // 전체 화면에 도움말 표시 플래그
};

/**
 * @brief 각 화면에 표시될 수 있는 정보들의 모드
 */
enum DisplayMode {
    MODE_TIME = 0,
    MODE_SPEED = 1,
    MODE_COMPASS = 2,
    MODE_SATS = 3,
    MODE_LATLON = 4,
    MODE_ALTITUDE = 5,
    MODE_DATE = 6,
    MODE_TRIP_STATS = 7
};
#include "driver/gpio.h"
#include "soc/gpio_struct.h"    // GPIO 구조체 정의 (GPIO.out_w1ts 사용을 위해 필수)
#include "soc/gpio_reg.h"       // GPIO 레지스터 정의

// --- I2C 핀 설정 ---
const uint8_t hw_sda_pin = 5;
const uint8_t hw_scl_pin = 6;
const uint8_t sw_sda_pin = 2;
const uint8_t sw_scl_pin = 3;

// --- OLED 객체 선언 ---
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_1(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_3(U8G2_R0, /* clock=*/ sw_scl_pin, /* data=*/ sw_sda_pin, /* reset=*/ U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2_4(U8G2_R0, /* clock=*/ sw_scl_pin, /* data=*/ sw_sda_pin, /* reset=*/ U8X8_PIN_NONE);

// --- 애니메이션용 전역 변수 ---
float smoothedHeading = 0; // 부드럽게 보간된 현재 각도
float smoothedSpeed = 0;   // 부드럽게 보간된 현재 속도
int currentFlipMode = 2;   // 0:Normal, 1:Mirror H, 2:180(HV), 3:Mirror V(180+H)

// 고속 SW I2C 콜백
extern "C" uint8_t u8x8_gpio_and_delay_esp32_c3_fast(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    uint8_t pin;
    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            pin = u8x8->pins[U8X8_PIN_I2C_CLOCK];
            if (pin != U8X8_PIN_NONE) { pinMode(pin, OUTPUT_OPEN_DRAIN); gpio_set_level((gpio_num_t)pin, 1); }
            pin = u8x8->pins[U8X8_PIN_I2C_DATA];
            if (pin != U8X8_PIN_NONE) { pinMode(pin, OUTPUT_OPEN_DRAIN); gpio_set_level((gpio_num_t)pin, 1); }
            break;
        case U8X8_MSG_DELAY_MILLI: delay(arg_int); break;
        case U8X8_MSG_GPIO_I2C_CLOCK:
            if (arg_int) GPIO.out_w1ts.val = (1 << sw_scl_pin);
            else         GPIO.out_w1tc.val = (1 << sw_scl_pin);
            break;
        case U8X8_MSG_GPIO_I2C_DATA:
            if (arg_int) GPIO.out_w1ts.val = (1 << sw_sda_pin);
            else         GPIO.out_w1tc.val = (1 << sw_sda_pin);
            break;
        default: u8x8_SetGPIOResult(u8x8, 1); break;
    }
    return 1;
}

/**
 * @brief 위성 모양 아이콘 그리기
 */
void drawSatelliteIcon(U8G2 &u8g2, int x, int y) {
    // 메인 바디 (대각선 사각형)
    u8g2.drawLine(x+10, y+4, x+16, y+10);
    u8g2.drawLine(x+16, y+10, x+10, y+16);
    u8g2.drawLine(x+10, y+16, x+4, y+10);
    u8g2.drawLine(x+4, y+10, x+10, y+4);
    
    // 태양전지판 1 (좌상단)
    u8g2.drawFrame(x+1, y+1, 6, 4);
    u8g2.drawLine(x+5, y+5, x+7, y+7);
    
    // 태양전지판 2 (우하단)
    u8g2.drawFrame(x+16, y+15, 6, 4);
    u8g2.drawLine(x+14, y+13, x+16, y+15);
    
    // 안테나 접시 (좌하단)
    u8g2.drawCircle(x+6, y+14, 3, U8G2_DRAW_LOWER_LEFT);
    u8g2.drawPixel(x+6, y+14); // 피드 포인트
    
    // 전파 신호
    u8g2.drawCircle(x+4, y+16, 5, U8G2_DRAW_LOWER_LEFT);
    u8g2.drawCircle(x+2, y+18, 7, U8G2_DRAW_LOWER_LEFT);
}

/**
 * @brief 텍스트 중앙 정렬 출력 헬퍼 함수
 */
void drawCenteredText(U8G2 &u8g2, const char* text, int y, const uint8_t *font = u8g2_font_6x10_tf) {
    u8g2.setFont(font);
    int textWidth = u8g2.getUTF8Width(text);
    int x = (u8g2.getDisplayWidth() - textWidth) / 2;
    if (x < 0) x = 0;
    u8g2.drawUTF8(x, y, text);
}

/**
 * @brief 현재 반전 모드를 OLED 하드웨어 레지스터에 적용
 */
void updateDisplayFlip() {
    uint8_t seg, com;
    switch(currentFlipMode) {
        case 1: seg = 0xA1; com = 0xC0; break; // 좌우반전 (Mirror H)
        case 2: seg = 0xA1; com = 0xC8; break; // 180도 회전 (H+V Flip)
        case 3: seg = 0xA0; com = 0xC8; break; // 상하반전 (Mirror V)
        default: seg = 0xA0; com = 0xC0; break; // 원래화면
    }
    u8g2_1.sendF("c", seg); u8g2_1.sendF("c", com);
    u8g2_2.sendF("c", seg); u8g2_2.sendF("c", com);
    u8g2_3.sendF("c", seg); u8g2_3.sendF("c", com);
    u8g2_4.sendF("c", seg); u8g2_4.sendF("c", com);
}

/**
 * @brief 디스플레이 초기화
 */
void initDisplay() {
    Wire.begin(hw_sda_pin, hw_scl_pin);
    Wire.setClock(1000000);
    
    // SW I2C 콜백 주입
    u8g2_3.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;
    u8g2_4.getU8x8()->gpio_and_delay_cb = u8x8_gpio_and_delay_esp32_c3_fast;

    // 개별 시작
    u8g2_1.setBusClock(1000000); u8g2_1.setI2CAddress(0x3C * 2); u8g2_1.begin();
    u8g2_2.setBusClock(1000000); u8g2_2.setI2CAddress(0x3D * 2); u8g2_2.begin();
    u8g2_3.setI2CAddress(0x3C * 2); u8g2_3.begin();
    u8g2_4.setI2CAddress(0x3D * 2); u8g2_4.begin();

    updateDisplayFlip(); // 저장된 반전 모드 적용
}

/**
 * @brief OTA 진행률 표시
 */
void displayOTAStatus(int progress) {
    u8g2_1.clearBuffer();
    u8g2_1.setFont(u8g2_font_6x10_tf);
    u8g2_1.drawStr(0, 10, "FIRMWARE UPDATING...");
    char prog[8];
    snprintf(prog, sizeof(prog), "%d%%", progress);
    drawCenteredText(u8g2_1, prog, 40, u8g2_font_maniac_tr);
    u8g2_1.sendBuffer();
}

/**
 * @brief GPS 시간 표시 (하단에 날짜 표시 추가)
 */
void drawTime(U8G2 &u8g2, const char* time, int year, int month, int day) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, "GPS TIME (KST)");
    
    // 메인 중앙 시간 (y를 48->44 쯤으로 살짝 올려서 하단 공간 확보)
    drawCenteredText(u8g2, time, 44, u8g2_font_maniac_tr);
    
    // 하단 중앙 날짜 (폰트를 조금 더 크게 변경)
    char dateBuf[16];
    snprintf(dateBuf, sizeof(dateBuf), "%d.%02d.%02d", year, month, day);
    drawCenteredText(u8g2, dateBuf, 64, u8g2_font_bpixeldouble_tr);
    
    u8g2.sendBuffer();
}

/**
 * @brief 아날로그 스피도미터 게이지
 */
void drawSpeedGauge(U8G2 &u8g2, float speed) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, "SPEED");

    float targetSpeed = speed;
    // 속도 보간 (게이지 바늘의 부드러운 움직임)
    smoothedSpeed += (targetSpeed - smoothedSpeed) * 0.2;
    if (smoothedSpeed < 0) smoothedSpeed = 0;
    if (smoothedSpeed > 180) smoothedSpeed = 180;  // 게이지 범위 상한 클램핑

    int scx = 64, scy = 60; 
    int sr = 54; // 대형 아크

    // 1. 게이지 배경 아크
    u8g2.drawCircle(scx, scy, sr, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    u8g2.drawCircle(scx, scy, sr-1, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);

    // 2. 눈금 표시 (0 ~ 180 km/h)
    u8g2.setFont(u8g2_font_5x7_tr);
    for(int i=0; i<=180; i+=30) {
        float angle = (i + 180.0) * 3.14159 / 180.0;
        int x1 = scx + cos(angle) * sr;
        int y1 = scy + sin(angle) * sr;
        int x2 = scx + cos(angle) * (sr - 6);
        int y2 = scy + sin(angle) * (sr - 6);
        u8g2.drawLine(x1, y1, x2, y2);
        
        // 수치 라벨 (0, 60, 120, 180 위주로)
        if (i % 30 == 0) {
            int lx = scx + cos(angle) * (sr - 14);
            int ly = scy + sin(angle) * (sr - 14);
            u8g2.setCursor(lx - 5, ly + 3);
            u8g2.print(i);
        }
    }

    // 3. 바늘 (Needle) - 0km/h는 180도 위치
    float needleAngle = (smoothedSpeed + 180.0);
    if (needleAngle > 360) needleAngle = 360; 
    float nRad = needleAngle * 3.14159 / 180.0;
    
    int nx = scx + cos(nRad) * (sr - 4);
    int ny = scy + sin(nRad) * (sr - 4);
    
    // 굵고 뾰족한 바늘 (삼각형 + 라인 조합)
    u8g2.drawLine(scx, scy, nx, ny);
    u8g2.drawLine(scx-1, scy, nx, ny);
    u8g2.drawLine(scx+1, scy, nx, ny);

    // 4. 중앙 하단 디지털 속도 표시
    u8g2.setFont(u8g2_font_maniac_tr);
    char dSpd[8];
    snprintf(dSpd, sizeof(dSpd), "%d", (int)targetSpeed);
    int dsw = u8g2.getUTF8Width(dSpd);
    int dsx = 64 - (dsw / 2);
    u8g2.drawStr(dsx, 62, dSpd);
    
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(dsx + dsw + 2, 53, "km/h");

    u8g2.sendBuffer();
}

/**
 * @brief 반원 나침반 (Semi-Circle Arc Compass)
 */
void drawCompass(U8G2 &u8g2, float course) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, "HEADING");

    // 목표 각도 수치 보간
    float targetAngle = course;
    float diff = targetAngle - smoothedHeading;
    if (diff > 180) diff -= 360;
    if (diff < -180) diff += 360;
    smoothedHeading += diff * 0.3; 
    
    if (smoothedHeading >= 360) smoothedHeading -= 360;
    if (smoothedHeading < 0) smoothedHeading += 360;

    int cx = 64, cy = 64; // 중심점을 바닥으로 이동하여 아크를 최대화
    int r = 44;           // 아크 반지름 크게 확대 (기존 32)

    // 1. 대형 반원(Arc) 그리기
    u8g2.drawCircle(cx, cy, r, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    u8g2.drawCircle(cx, cy, r-1, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    
    // 2. 더욱 굵고 강력한 중앙 고정 화살표
    u8g2.drawTriangle(cx, cy - r + 3, cx - 10, cy - r + 17, cx + 10, cy - r + 17); // 대형 화살촉

    // 3. 회전하는 방위 표시 (가독성 및 노출 범위 개선)
    const char* labels[] = {"N", "E", "S", "W"};
    float angles[] = {0, 90, 180, 270};
    
    u8g2.setFont(u8g2_font_maniac_tr);
    for (int i = 0; i < 4; i++) {
        // 상대 각도 계산
        float angleRad = (angles[i] - smoothedHeading - 90.0) * 3.14159 / 180.0;
        
        // 방위 기호 위치 계산 (아크 바깥쪽)
        int lx = cx + cos(angleRad) * (r + 4);
        int ly = cy + sin(angleRad) * (r + 4);
        
        // 화면 범위 내에 있다면 가급적 사라지지 않도록 렌더링 (하단 제외)
        if (ly < 80 && lx > -20 && lx < 150) {
            u8g2.drawStr(lx - 8, ly+8, labels[i]);
        }
    }

    // 4. 하단 중앙 각도 표시 (정갈한 스타일 유지)
    u8g2.setFont(u8g2_font_maniac_tr);
    char valStr[8];
    snprintf(valStr, sizeof(valStr), "%d", (int)targetAngle);
    int tw = u8g2.getUTF8Width(valStr);
    int startX = (128 - (tw + 6)) / 2;
    
    u8g2.drawStr(startX, 63, valStr); // 숫자
    u8g2.drawCircle(startX + tw + 3, 42,2);  // 작은 도(°) 기호
    
    u8g2.sendBuffer();
}

/**
 * @brief 위성 수신 상세 상태
 */
void drawSatellites(U8G2 &u8g2, int usedSats, int visibleSats, const char* hdop, const char* satStatus) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, "SATELLITES");
    
    // 1. 현재 고정에 사용 중인 위성수 (Fix된 위성) - 중앙 메인
    u8g2.setFont(u8g2_font_maniac_tr);
    char satStr[8];
    snprintf(satStr, sizeof(satStr), "%d", usedSats);
    int satWidth = u8g2.getUTF8Width(satStr);
    int satX = (128 - satWidth) / 2;
    u8g2.drawUTF8(satX, 45, satStr);
    
    // 2. 위성 아이콘 구역
    drawSatelliteIcon(u8g2, 105, 0); // 우측 상단 아이콘 (y=0~20)
    
    // 3. 가시 위성수 (In-View) - 아이콘 바로 아래 (작은 폰트)
    u8g2.setFont(u8g2_font_6x10_tf);
    char visStr[16];
    snprintf(visStr, sizeof(visStr), "%d", visibleSats);
    int visWidth = u8g2.getUTF8Width(visStr);
    u8g2.drawStr(128 - visWidth, 30, visStr); // 아이콘 아래쪽 x=105~128 사이 배치
    
    // 4. 하단 상태 및 정밀도 (Status 대신 DOP 표시)
    u8g2.setFont(u8g2_font_6x10_tf);
    if (!strcmp(satStatus, "SEARCHING")) {
        // 데이터 수집 전이면 DOP 대신 SEARCHING 표시 (기본 상태 유지)
        drawCenteredText(u8g2, hdop, 62); 
    } else {
        // GPS 수신 중이면 "LOCKED [DOP]" 조합으로 표시 (공간 효율성)
        char footer[32];
        snprintf(footer, sizeof(footer), "%s  %s", satStatus, hdop);
        drawCenteredText(u8g2, footer, 62);
    }
    
    u8g2.sendBuffer();
}

/**
 * @brief 위도 및 경도 표시
 */
void drawLatLon(U8G2 &u8g2, float lat, float lon) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, "COORDINATES");
    
    // Latitude (위도)
    u8g2.setFont(u8g2_font_bpixeldouble_tr);
    char buf[16];
    snprintf(buf, sizeof(buf), "N: %.4f", lat);
    u8g2.drawStr(5, 35, buf);
    
    // Longitude (경도)
    snprintf(buf, sizeof(buf), "E: %.4f", lon);
    u8g2.drawStr(5, 55, buf);
    
    u8g2.sendBuffer();
}

/**
 * @brief 고도 표시 (산 그림과 고도 수치)
 */
void drawAltitude(U8G2 &u8g2, float altitude) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, "ALTITUDE");
    
    // 산 픽토그램 (선을 겹쳐 그려 굵게 표현)
    for (int i = 0; i <= 1; i++) {
        u8g2.drawLine(0+i, 63, 32+i, 25);
        u8g2.drawLine(32+i, 25, 50+i, 45);
        u8g2.drawLine(50+i, 45, 88+i, 5);
        u8g2.drawLine(88+i, 5, 127+i, 63);
    }

    // 고도 수치 우측 배치
    u8g2.setFont(u8g2_font_maniac_tr);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f", altitude); // 소수점 제외

    int dsw = u8g2.getUTF8Width(buf);
    int dsx = 64 - (dsw / 2);
    u8g2.drawStr(dsx, 64, buf);
    
    // 단위 (meters)
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(dsx + dsw + 2, 64, "meters");
    
    u8g2.sendBuffer();
}

/**
 * @brief 달력 표시 (격자형 캘린더 모양)
 */
void drawCalendar(U8G2 &u8g2, int year, int month, int day) {
    u8g2.clearBuffer();
    
    // 최상단 Header 날짜 (예: 2026.03)

    char buf[16];
    snprintf(buf, sizeof(buf), "%d.%02d", year, month);
    drawCenteredText(u8g2, buf, 14, u8g2_font_bpixeldouble_tr);
    
    // 해당 월의 1일 요일 계산 (Sakamoto Algorithm)
    static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    int y_calc = year;
    if (month < 3) y_calc -= 1;
    int firstDayOfWeek = ( y_calc + y_calc/4 - y_calc/100 + y_calc/400 + t[month-1] + 1 ) % 7; // 0=Sun, 6=Sat
    
    // 해당 월의 총 일수 자동 계산
    int daysInMonth = 31;
    if (month == 4 || month == 6 || month == 9 || month == 11) daysInMonth = 30;
    else if (month == 2) daysInMonth = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
    
    // 요일 헤더 그리기 (S M T W T F S)
    u8g2.setFont(u8g2_font_5x7_tr);
    const char* wdays[] = {"S", "M", "T", "W", "T", "F", "S"};
    int colW = 18; // 한 열(Day)이 차지하는 가로 너비
    int xOffset = 1;
    
    for (int i = 0; i < 7; i++) {
        int x = xOffset + i * colW + (colW - u8g2.getUTF8Width(wdays[i])) / 2;
        u8g2.drawStr(x, 22, wdays[i]);
    }
    
    u8g2.drawLine(0, 23, 128, 23); // 요일과 날짜 경계선
    
    // 일자 데이터 그리기 (그리드 레이아웃)
    int currentDay = 1;
    for (int row = 0; row < 6; row++) {
        for (int col = 0; col < 7; col++) {
            if (row == 0 && col < firstDayOfWeek) continue;
            if (currentDay > daysInMonth) break;
            
            snprintf(buf, sizeof(buf), "%d", currentDay);
            int dw = u8g2.getUTF8Width(buf);
            int cx = xOffset + col * colW + colW / 2; // 칸의 정중앙 x좌표
            int dx = cx - dw / 2; // 텍스트 렌더링 시작 좌표
            int dy = 31 + row * 8; // 날짜 행(Row)간 세로 간격(8px)
            
            if (currentDay == day) { // 오늘 날짜면 반전 하이라이트 박스 생성
                u8g2.setDrawColor(1); // 박스는 하얗게
                u8g2.drawBox(cx - 8, dy - 7, 16, 8); 
                u8g2.setDrawColor(0); // 글자는 까맣게(투명하게 파내기)
                u8g2.drawStr(dx, dy, buf);
                u8g2.setDrawColor(1); // 원래대로 복구
            } else {
                u8g2.drawStr(dx, dy, buf);
            }
            currentDay++;
        }
        if (currentDay > daysInMonth) break;
    }
    u8g2.sendBuffer();
}

/**
 * @brief 주행 통계 표시 (타이머 및 트립미터)
 */
void drawTripStats(U8G2 &u8g2, unsigned long seconds, float distance) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, "TRIP TIME/DISTANCE");

    // 주행 타이머 포맷팅 (HH:MM:SS)
    int h = seconds / 3600;
    int m = (seconds / 60) % 60;
    int s = seconds % 60;
    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", h, m, s);
    drawCenteredText(u8g2, timeBuf, 42, u8g2_font_maniac_tr);

    // 하단 구분선
    u8g2.drawLine(0, 46, 128, 46);

    // 거리 정보 (나중에 트립미터 구현 시 값 반영)
    u8g2.setFont(u8g2_font_6x10_tf);
    char distBuf[24];
    if (distance < 1.0) {
        snprintf(distBuf, sizeof(distBuf), "%.0fm", distance * 1000.0);
    } else {
        snprintf(distBuf, sizeof(distBuf), "%.2fkm", distance);
    }
    drawCenteredText(u8g2, distBuf, 62, u8g2_font_bpixeldouble_tr);

    u8g2.sendBuffer();
}

/**
 * @brief 디스플레이별 버튼 동작 설명 화면 표시
 */
void drawHelpScreen(U8G2 &u8g2, int displayIndex) {
    u8g2.clearBuffer();
    
    // 상단 타이틀
    u8g2.setFont(u8g2_font_6x10_tf);
    char title[16];
    snprintf(title, sizeof(title), "- BTN %d INFO -", displayIndex + 1);
    drawCenteredText(u8g2, title, 10);
    u8g2.drawLine(0, 14, 128, 14);
    
    // 동작 설명 텍스트 준비
    const char* shortDesc = "";
    const char* longDesc = "";
    
    switch (displayIndex) {
        case 0:
            shortDesc = "S: NEXT MODE";
            longDesc  = "L: FLIP SCREEN";
            break;
        case 1:
            shortDesc = "S: NEXT MODE";
            longDesc  = "L: TOGGLE INFO"; // 2번 버튼 길게 누를 경우
            break;
        case 2:
            shortDesc = "S: NEXT MODE";
            longDesc  = "L: RESET TRIP"; 
            break;
        case 3:
            shortDesc = "S: NEXT MODE";
            longDesc  = "L: DEMO TOGGLE";
            break;
    }
    
    // 텍스트 좌측 정렬 안내
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(10, 35, shortDesc);
    u8g2.drawStr(10, 55, longDesc);
    
    u8g2.sendBuffer();
}

/**
 * @brief 동적 대시보드 화면 갱신: 각 화면의 모드 설정에 맞는 그리기 함수를 실행
 */
void updateDashboard(const DashboardData& data, int modes[4]) {
    // 4개의 디스플레이 객체를 포인터 배열로 묶어서 반복 처리
    U8G2* displays[4] = {&u8g2_1, &u8g2_2, &u8g2_3, &u8g2_4};
    
    for (int i = 0; i < 4; i++) {
        // 도움말 모드인 경우 기본 모드를 무시하고 화면 렌더링
        if (data.showHelp) {
            drawHelpScreen(*displays[i], i);
            continue;
        }
        
        switch (modes[i]) {
            case MODE_TIME: drawTime(*displays[i], data.timeStr, data.calYear, data.calMonth, data.calDay); break;
            case MODE_SPEED: drawSpeedGauge(*displays[i], data.speedVal); break;
            case MODE_COMPASS: drawCompass(*displays[i], data.courseVal); break;
            case MODE_SATS: drawSatellites(*displays[i], data.usedSats, data.visibleSats, data.hdopStr, data.statusStr); break;
            case MODE_LATLON: drawLatLon(*displays[i], data.latVal, data.lonVal); break;
            case MODE_ALTITUDE: drawAltitude(*displays[i], data.altVal); break;
            case MODE_DATE: drawCalendar(*displays[i], data.calYear, data.calMonth, data.calDay); break;
            case MODE_TRIP_STATS: drawTripStats(*displays[i], data.tripTimeSec, data.tripDistance); break;
        }
    }
}

#endif // DISPLAY_HANDLER_H
