// worynim@gmail.com
/**
 * @file youtube.h
 * @brief YouTube Data API를 이용한 채널 구독자 수 정보 처리
 * @details 지정된 채널 ID의 실시간 구독자 데이터를 수집하여 통계 위젯에 제공
 */
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
