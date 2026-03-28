#ifndef YOUTUBE_H
#define YOUTUBE_H

#include <Arduino.h>

// --- 유튜브 데이터 선언 (정의는 data_manager.cpp 에서 담당) ---
extern String youtube_channel; 
extern String subscribe_num;
extern const unsigned long youtube_timeout;

// --- 함수 프로토타입 ---
String formatSubscribers(String subscribers);
void get_subscribe();

#endif
