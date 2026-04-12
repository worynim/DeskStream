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
