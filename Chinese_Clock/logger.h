// worynim@gmail.com
/**
 * @file logger.h
 * @brief 통합 시스템 로깅 클래스 정의
 * @details OLED 1번 및 시리얼 모니터에 대한 상태 로그 기록 및 도트 애니메이션 관리
 * @note [SYNC] 원본: ENG_Clock/logger.h — 현재 바이트 단위로 동일. 원본 수정 시 함께 반영할 것.
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

class Logger {
public:
    Logger();
    void addLog(const String& msg);
    void updateLastLog(const String& msg);

private:
    String log_lines[4];
    int log_count;
    void render();
};

extern Logger logger;

#endif
