// worynim@gmail.com
/**
 * @file menu.h
 * @brief 설정 메뉴 상태 기계 (하드웨어 무관 — 호스트 테스트 대상)
 * @details 항목 = 모드 설정 + 공통 Beep(마지막). 고르는 값(pending)은 저장 전까지 Setting::idx를 바꾸지 않는다.
 */
#pragma once
#include "model.h"

class SettingsMenu {
public:
    static constexpr int kMaxItems = 4;   // 모드 설정 최대 3 + Beep

    void open(Mode& m);
    bool isOpen() const { return _open; }
    int itemCount() const { return _count; }
    int current() const { return _cur; }
    Setting& item(int k) const;
    uint8_t pending(int k) const { return _pending[k]; }

    void dec();     // 현재 항목 값을 목록 안에서 하나 앞으로 (처음에서 멈춤)
    void inc();     // 하나 뒤로 (끝에서 멈춤)
    bool next();    // 다음 항목으로 가면 false, 마지막 항목에서 저장하고 닫으면 true
    void cancel();  // 저장하지 않고 닫기

private:
    Mode* _mode = nullptr;
    bool _open = false;
    int _count = 0;
    int _cur = 0;
    uint8_t _pending[kMaxItems] = {};
};
