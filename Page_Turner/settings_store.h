// worynim@gmail.com
/**
 * @file settings_store.h
 * @brief 모드별 설정·Beep·마지막 모드를 NVS("pturner")에 저장 — 값 목록의 위치(U8)로 저장한다
 */
#pragma once

namespace store {

void begin();                 // NVS에서 읽어 각 Setting::idx에 넣는다 (목록 밖이면 기본값)
int lastMode();               // 0..kNumModes-1
void saveSoon(int modeIdx);   // 5초 뒤 저장 (그 사이 또 바뀌면 다시 미룸)
void saveNow(int modeIdx);    // 슬립 직전

}  // namespace store
