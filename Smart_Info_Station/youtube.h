// worynim@gmail.com
/**
 * @file youtube.h
 * @brief YouTube 채널 구독자 수 데이터 선언
 * @details YouTube 웹 페이지가 쓰는 내부 요청(youtubei, API 키 불필요)으로 채널 헤더의 구독자 수를 읽는다
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
bool get_subscribe();  // 성공 시 true

#endif
