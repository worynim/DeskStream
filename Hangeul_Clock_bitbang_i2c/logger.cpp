// worynim@gmail.com
/**
 * @file logger.cpp
 * @brief 통합 시스템 로깅 클래스 구현
 * @details 시리얼 도트 애니메이션 한 줄 출력 알고리즘 및 OLED 1번 다중 라인 로그 렌더링 구현
 */
#include "logger.h"
#include "display_manager.h"

Logger logger;

Logger::Logger() : log_count(0) {
    for (int i = 0; i < 4; i++) log_lines[i] = "";
}

/**
 * @brief 새로운 로그 메시지 추가 및 출력
 */
void Logger::addLog(const String& msg) {
    Serial.println("[LOG] " + msg);
    
    if (log_count < 4) {
        log_lines[log_count++] = msg;
    } else {
        // 기존 로그 위로 밀기
        for (int i = 0; i < 3; i++) log_lines[i] = log_lines[i+1];
        log_lines[3] = msg;
    }
    render();
}

/**
 * @brief 가장 최근에 추가된 로그 메시지 내용 수정
 */
void Logger::updateLastLog(const String& msg) {
    if (log_count > 0) {
        String oldMsg = log_lines[log_count - 1];
        log_lines[log_count - 1] = msg;
        
        // 시리얼 출력 최적화: 메시지가 이전 메시지를 포함하고 있으면 줄바꿈 없이 추가분만 출력
        if (msg.startsWith(oldMsg) && msg.length() > oldMsg.length()) {
            Serial.print(msg.substring(oldMsg.length()));
        } 
        // 반대의 경우(도트 리셋 등)에도 한 줄에 이어서 출력하기 위해 베이스 체크
        else {
            int lastDotIdx = oldMsg.indexOf('.');
            String base = (lastDotIdx != -1) ? oldMsg.substring(0, lastDotIdx) : oldMsg;
            if (msg.startsWith(base)) {
                // 베이스가 같으면 줄바꿈 없이 점만 계속 찍음
                int dotsInNew = 0;
                for(int i=0; i<msg.length(); i++) if(msg[i] == '.') dotsInNew++;
                int dotsInOld = 0;
                for(int i=0; i<oldMsg.length(); i++) if(oldMsg[i] == '.') dotsInOld++;
                
                if (dotsInNew > dotsInOld) {
                    for(int d=0; d < (dotsInNew - dotsInOld); d++) Serial.print(".");
                } else if (dotsInNew == 1 && dotsInOld >= 4) {
                    // 점이 리셋된 경우(..... -> .)에도 이어서 한 점 더 찍음
                    Serial.print(".");
                }
            } else {
                Serial.println("\n[LOG] " + msg);
            }
        }
        render();
    }
}

/**
 * @brief OLED 1번 화면에 로그 메시지 렌더링
 */
void Logger::render() {
    display.u8g2_1.clearBuffer();
    display.u8g2_1.setFont(STATUS_FONT);
    for (int i = 0; i < log_count; i++) {
        // UTF8 지원으로 한글 로그 출력 가능 보장
        display.u8g2_1.drawUTF8(0, (i + 1) * 15, log_lines[i].c_str());
    }
    // [Step 5.1] 하드웨어 섀도우 버퍼와 동기화하기 위해 pushParallel 사용 (잔상 해결)
    display.pushParallel();
}
