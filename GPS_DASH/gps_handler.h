// worynim@gmail.com
/**
 * @file gps_handler.h
 * @brief TinyGPS++를 이용한 GPS 데이터 파싱 및 관리
 * @details 시리얼 포트로부터 GPS 원시 데이터를 수신하여 유효성 검사 및 정보 추출 처리
 */

#ifndef GPS_HANDLER_H
#define GPS_HANDLER_H

#include <Adafruit_GPS.h>
#include <HardwareSerial.h>

// GPS 연결 핀 정의 (20번 Rx, 21번 Tx)
const int GPS_RX_PIN = 20;
const int GPS_TX_PIN = 21;
const uint32_t GPS_BAUD = 9600; // 대부분의 GPS 모듈 기본 보드레이트

// 로그 콜백 함수 포인터 (include 순서 의존성 제거용)
typedef void (*LogCallback)(const char* msg);
LogCallback _gpsLogCallback = nullptr;

void setGPSLogCallback(LogCallback cb) { _gpsLogCallback = cb; }

// GPS 상태 변화 콜백 (Lock/Unlock)
typedef void (*SimpleCallback)();
SimpleCallback _onGPSLock = nullptr;
SimpleCallback _onGPSUnlock = nullptr;

void setGPSFixCallbacks(SimpleCallback lockCb, SimpleCallback unlockCb) {
    _onGPSLock = lockCb;
    _onGPSUnlock = unlockCb;
}

// Adafruit_GPS 및 Serial 객체 선언
HardwareSerial gpsSerial(0); // Hardware UART0 사용 (ESP32-C3 기본 핀 20, 21에 매핑됨)
Adafruit_GPS GPS(&gpsSerial);

// GSV 파싱을 통한 가시 위성 수 추적
int _satVisible = 0;             // 가시 위성 수 (GSV에서 파싱, 단일 스레드 접근)
bool _lastFixState = false;      // Fix 상태 변화 감지용

/**
 * @brief UBX 체크섬 계산 (Fletcher 알고리즘)
 */
void calcUBXChecksum(byte* msg, int len, byte& ck_a, byte& ck_b) {
    ck_a = 0; ck_b = 0;
    for (int i = 2; i < len - 2; i++) {  // class~payload까지 (sync 제외, checksum 제외)
        ck_a += msg[i];
        ck_b += ck_a;
    }
}

/**
 * @brief UBX 명령 전송 후 간단한 대기
 */
void sendUBXCommand(byte* cmd, int len) {
    gpsSerial.write(cmd, len);
    gpsSerial.flush();  // 전송 완료 대기
    delay(100);         // 모듈 처리 시간 확보
}

/**
 * @brief GSV 문장에서 가시 위성 수를 직접 파싱
 * @param nmea NMEA 문장 포인터
 * @return 파싱 성공 여부
 * 
 * GSV 형식: $GPGSV,총문장수,현재문장번호,가시위성수,...*CS
 * 예: $GPGSV,3,1,12,01,40,083,46,...*7B
 *                     ^^ 이 값(12)이 가시 위성 수
 */
bool parseGSVSatCount(const char* nmea) {
    if (strncmp(nmea, "$GPGSV", 6) != 0 && strncmp(nmea, "$GNGSV", 6) != 0) {
        return false;  // GSV 문장이 아님
    }
    
    // 첫 번째 GSV 문장(문장번호=1)에서만 가시위성 수를 읽음
    int commaCount = 0;
    const char* p = nmea;
    int field2 = 0, field3 = 0;
    
    while (*p && commaCount < 4) {
        if (*p == ',') {
            commaCount++;
            p++;
            if (commaCount == 2) field2 = atoi(p);       // 현재 문장 번호
            else if (commaCount == 3) field3 = atoi(p);   // 가시 위성 수
        } else {
            p++;
        }
    }
    
    // 첫 번째 문장일 때만 가시 위성 수 업데이트 (중복 방지)
    if (field2 == 1 && field3 > 0) {
        _satVisible = field3;
        return true;
    }
    return false;
}

/**
 * GPS 모듈 초기화 함수
 */
void initGPS() {
    // 하드웨어 시리얼 초기화 (핀 매핑 포함)
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    
    // GPS 모듈 부팅 완료 대기 (Cold Start 시 최대 1초 소요)
    delay(500);
    Serial.println("[GPS] 모듈 부팅 대기 완료.");
    
    // 수신 버퍼 초기 클리어 (부팅 중 쌓인 잔여 데이터 제거)
    while (gpsSerial.available()) gpsSerial.read();
    
    // 1. [u-blox 전용] 업데이트 속도를 5Hz(200ms)로 설정 (UBX-CFG-RATE)
    byte ubx_5hz[] = {0xB5, 0x62, 0x06, 0x08, 0x06, 0x00,
                      0xC8, 0x00,  // measRate: 200ms
                      0x01, 0x00,  // navRate: 1 cycle
                      0x01, 0x00,  // timeRef: GPS time
                      0x00, 0x00}; // checksum (자동 계산)
    calcUBXChecksum(ubx_5hz, sizeof(ubx_5hz), ubx_5hz[12], ubx_5hz[13]);
    sendUBXCommand(ubx_5hz, sizeof(ubx_5hz));
    
    // 2. [u-blox 전용] NMEA 출력 설정 (UBX-CFG-MSG)
    //    GGA(Fix data) 활성화 — 매 네비게이션 주기마다 출력
    byte ubx_gga_on[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00,
                         0xF0, 0x00,  // NMEA GGA
                         0x00, 0x01, 0x00, 0x00, 0x00, 0x00, // UART1만 ON
                         0x00, 0x00};
    calcUBXChecksum(ubx_gga_on, sizeof(ubx_gga_on), ubx_gga_on[14], ubx_gga_on[15]);
    sendUBXCommand(ubx_gga_on, sizeof(ubx_gga_on));
    
    //    RMC(Recommended Minimum) 활성화
    byte ubx_rmc_on[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00,
                         0xF0, 0x04,  // NMEA RMC
                         0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00};
    calcUBXChecksum(ubx_rmc_on, sizeof(ubx_rmc_on), ubx_rmc_on[14], ubx_rmc_on[15]);
    sendUBXCommand(ubx_rmc_on, sizeof(ubx_rmc_on));
    
    //    GSV(위성 가시정보) 활성화 — 5번에 1번(1Hz) 출력으로 대역폭 절약
    byte ubx_gsv_on[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00,
                         0xF0, 0x03,  // NMEA GSV
                         0x00, 0x05, 0x00, 0x00, 0x00, 0x00, // 5 nav cycle마다
                         0x00, 0x00};
    calcUBXChecksum(ubx_gsv_on, sizeof(ubx_gsv_on), ubx_gsv_on[14], ubx_gsv_on[15]);
    sendUBXCommand(ubx_gsv_on, sizeof(ubx_gsv_on));
    
    //    불필요한 NMEA 메시지 비활성화 (GLL, GSA, VTG)
    const byte msgIds[] = {0x01, 0x02, 0x05}; // GLL, GSA, VTG
    for (int i = 0; i < 3; i++) {
        byte ubx_off[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00,
                          0xF0, msgIds[i],
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00};
        calcUBXChecksum(ubx_off, sizeof(ubx_off), ubx_off[14], ubx_off[15]);
        sendUBXCommand(ubx_off, sizeof(ubx_off));
    }
    
    Serial.println("[GPS] NEO-6M UBX 설정 완료: 5Hz, GGA+RMC+GSV(1Hz)");
}

/**
 * GPS 데이터를 읽어 파싱하는 함수 (Loop에서 매번 호출 권장)
 */
void updateGPS() {
    // 수신 데이터 존재 여부 추적 (디버그용)
    static uint32_t lastDataTime = 0;      // 마지막으로 데이터가 수신된 시각
    static uint32_t lastNoDataWarn = 0;     // 마지막 경고 출력 시각
    static uint32_t nmeaCount = 0;         // 수신된 NMEA 문장 수 (누적)
    
    // 시리얼 버퍼에 쌓인 모든 문자를 읽어 처리
    bool dataReceived = false;
    while (gpsSerial.available()) {
        char c = GPS.read();
        if (DEBUG_GPS) Serial.write(c);  // 디버그: GPS 원시 데이터 그대로 출력
        dataReceived = true;
        
        // 새로운 NMEA 문장이 완성되었는지 확인
        if (GPS.newNMEAreceived()) {
            char* lastNMEA = GPS.lastNMEA();
            nmeaCount++;
            
            // GSV 문장이면 가시 위성 수 직접 파싱 (Adafruit 미지원 보완)
            parseGSVSatCount(lastNMEA);
            
            if (!GPS.parse(lastNMEA)) {
                continue; // 파싱 실패 시 다음 문장 처리
            }
            
            // Fix 상태 변화 감지 및 로깅
            if (GPS.fix != _lastFixState) {
                _lastFixState = GPS.fix;
                char fixBuf[64];
                snprintf(fixBuf, sizeof(fixBuf), "[GPS] Fix %s! (Sat:%d, HDOP:%.1f)",
                    GPS.fix ? "ACQUIRED" : "LOST", (int)GPS.satellites, GPS.HDOP);
                Serial.println(fixBuf);
                if (_gpsLogCallback) _gpsLogCallback(fixBuf);

                // 상위 레이어에 상태 변화 알림
                if (GPS.fix) {
                    if (_onGPSLock) _onGPSLock();
                } else {
                    if (_onGPSUnlock) _onGPSUnlock();
                }
            }

            // 주요 정보 디버그 출력 (1초 단위)
            static uint32_t lastDebug = 0;
            if (millis() - lastDebug > 1000) {
                lastDebug = millis();
                char buf[128];
                snprintf(buf, sizeof(buf), "Fix:%d, Sat:%d/%d, HDOP:%.1f, Lat:%.6f, Lon:%.6f, Spd:%.1f", 
                    GPS.fix, (int)GPS.satellites, _satVisible, GPS.HDOP,
                    GPS.latitudeDegrees, GPS.longitudeDegrees, GPS.speed * 1.852);
                Serial.println(buf);
                if (_gpsLogCallback) _gpsLogCallback(buf);
            }
        }
    }
    
    // 데이터 수신 타이밍 추적
    if (dataReceived) {
        lastDataTime = millis();
    }
    
    // 5초 이상 데이터 미수신 시 경고 (배선/모듈 문제 감지)
    if (lastDataTime > 0 && millis() - lastDataTime > 5000) {
        if (millis() - lastNoDataWarn > 10000) {  // 10초마다 경고
            lastNoDataWarn = millis();
            Serial.printf("[GPS] WARNING: %lu초간 데이터 미수신! (총 NMEA: %lu)\n",
                (millis() - lastDataTime) / 1000, nmeaCount);
            if (_gpsLogCallback) {
                char warnBuf[80];
                snprintf(warnBuf, sizeof(warnBuf), "GPS 데이터 미수신 %lus", (millis() - lastDataTime) / 1000);
                _gpsLogCallback(warnBuf);
            }
        }
    }
    
    // 부팅 후 최초 데이터 수신 확인 로그 (1회만)
    static bool firstDataLogged = false;
    if (!firstDataLogged && lastDataTime > 0) {
        firstDataLogged = true;
        Serial.printf("[GPS] 첫 NMEA 수신 확인 (부팅 후 %lums)\n", lastDataTime);
    }
}

/**
 * 첫 번째 OLED: 통합 시간 문자열 생성 (GPS 우선, 미수신 시 가상 시간)
 * @param forceDemo 데모 모드 강제 여부
 * @return String HH:MM:SS format
 */
// 시간 문자열용 스택 버퍼 (힙 할당 방지)
char _timeBuf[12];

/**
 * @return const char* HH:MM:SS format (내부 버퍼 포인터 반환, 복사 불필요)
 */
const char* getGPSTime(bool forceDemo = false) {
    // GPS 연도 정보가 0이 아니면, 위치 고정(Fix) 전이라도 시간 정보는 수신된 것으로 간주
    if (!forceDemo && GPS.year != 0) {
        // 1. 실제 GPS 시간 사용 (GMT+9) — Fix 상태가 아니어도 시간 데이터는 유효함
        int hour = GPS.hour + 9;
        if (hour >= 24) hour -= 24;
        snprintf(_timeBuf, sizeof(_timeBuf), "%02d:%02d:%02d", hour, GPS.minute, GPS.seconds);
    } else {
        // 2. 가상 시간 (시스템 Uptime 기반) — 데이터가 아예 없을 때만 가상 시간 표시
        unsigned long totalSeconds = millis() / 1000;
        int h = (12 + (totalSeconds / 3600)) % 24;
        int m = (totalSeconds / 60) % 60;
        int s = totalSeconds % 60;
        snprintf(_timeBuf, sizeof(_timeBuf), "%02d:%02d:%02d", h, m, s);
    }
    return _timeBuf;
}

// 날짜 정보 변환 (달력 렌더링용 구조적 데이터 반환)
void getGPSCalendar(bool forceDemo, int &outYear, int &outMonth, int &outDay) {
    if (!forceDemo && GPS.year != 0) {
        int y = 2000 + GPS.year; // Adafruit 라이브러리 year는 2자리
        int m = GPS.month;
        int d = GPS.day;
        int h = GPS.hour + 9; // KST 변환
        
        // 날짜 넘김 (Rollover) 처리 (시간이 KST로 변환되면서 날짜가 바뀔 수 있음)
        if (h >= 24) {
            d += 1;
            int daysInMonth = 31;
            if (m == 4 || m == 6 || m == 9 || m == 11) daysInMonth = 30;
            else if (m == 2) daysInMonth = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 29 : 28;
            
            if (d > daysInMonth) {
                d = 1; m += 1;
                if (m > 12) { m = 1; y += 1; }
            }
        }
        outYear = y;
        outMonth = m;
        outDay = d;
    } else {
        outYear = 2026;
        outMonth = 3;
        outDay = 25; // 데모 날짜
    }
}

/**
 * @brief 현재 속도 반환 (km/h, knots→km/h 변환 포함)
 * @return float 1.0km/h 미만은 0 반환 (정지 노이즈 제거)
 */
float getGPSSpeed() {
    if (!GPS.fix) return 0.0f;
    float speedKmph = GPS.speed * 1.852;  // knots → km/h
    return (speedKmph < 1.0f) ? 0.0f : speedKmph;
}

/**
 * 세 번째 OLED: 이동 방향 값 반환
 * @return float Course degrees
 */
float getGPSCourse() {
    if (!GPS.fix) return 0.0f; // 고정 안될 시 0도 유지
    return GPS.angle;
}

/**
 * 네 번째 OLED: 사용 중인 위성 개수 (Fix에 기여)
 */
int getGPSSatCount() {
    return (int)GPS.satellites;
}

/**
 * 네 번째 OLED: 하늘에 보이는 전체 가시 위성 수 (GSV 파싱 기반)
 */
int getGPSSatVisible() {
    return _satVisible;
}

// DOP 값 문자열용 스택 버퍼
char _hdopBuf[16];

/**
 * 네 번째 OLED: HDOP(정밀도) 값 반환
 */
const char* getGPSHDOP() {
    if (!GPS.fix) return "---";
    snprintf(_hdopBuf, sizeof(_hdopBuf), "DOP:%.1f", GPS.HDOP);
    return _hdopBuf;
}

// 위성 상태 문자열용 스택 버퍼 (힙 할당 방지)
char _satStatusBuf[16];

/**
 * 네 번째 OLED: 위성 고정 상태 문자열 반환
 * @return const char* 상태 문자열 (내부 버퍼 포인터)
 */
const char* getGPSSatStatus() {
    if (GPS.fix) {
        snprintf(_satStatusBuf, sizeof(_satStatusBuf), "LOCKED:%d", GPS.fixquality);
        return _satStatusBuf;
    }
    if ((int)GPS.satellites > 0) return "SEEKING";
    return "SEARCHING";
}

// getGPSSatInfo() 제거됨 — 외부에서 미사용 데드 코드 (v2.5.1 정리)

/**
 * @brief 신규 화면용: 위도(Latitude) 반환
 */
float getGPSLatitude() {
    return GPS.fix ? GPS.latitudeDegrees : 0.0;
}

/**
 * @brief 신규 화면용: 경도(Longitude) 반환
 */
float getGPSLongitude() {
    return GPS.fix ? GPS.longitudeDegrees : 0.0;
}

/**
 * @brief 신규 화면용: 고도(Altitude, 미터 단위) 반환
 */
float getGPSAltitude() {
    return GPS.fix ? GPS.altitude : 0.0;
}

#endif // GPS_HANDLER_H
