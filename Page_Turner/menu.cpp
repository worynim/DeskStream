// worynim@gmail.com
/**
 * @file menu.cpp
 * @brief 설정 메뉴 상태 기계
 */
#include "menu.h"

void SettingsMenu::open(Mode& m) {
    _mode = &m;
    _count = m.numSettings + 1;
    _cur = 0;
    for (int k = 0; k < _count; k++) _pending[k] = item(k).idx;
    _open = true;
}

Setting& SettingsMenu::item(int k) const {
    return k < _mode->numSettings ? _mode->settings[k] : beepSetting();
}

void SettingsMenu::dec() {
    if (_pending[_cur] > 0) _pending[_cur]--;
}

void SettingsMenu::inc() {
    if (_pending[_cur] + 1 < item(_cur).numValues) _pending[_cur]++;
}

bool SettingsMenu::next() {
    if (_cur + 1 < _count) {
        _cur++;
        return false;
    }
    for (int k = 0; k < _count; k++) item(k).idx = _pending[k];
    _open = false;
    return true;
}

void SettingsMenu::cancel() { _open = false; }
