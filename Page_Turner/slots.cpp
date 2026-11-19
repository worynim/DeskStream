// worynim@gmail.com
/**
 * @file slots.cpp
 * @brief 연결 기기 슬롯 표
 */
#include "slots.h"

#include <ctype.h>
#include <string.h>

static bool sameAddr(const char* a, const char* b) {
    for (; *a && *b; a++, b++) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    }
    return *a == *b;
}

int SlotTable::find(const char* a) const {
    for (int s = 0; s < kSlots; s++) {
        if (!isEmpty(s) && sameAddr(_addr[s], a)) return s;
    }
    return -1;
}

int SlotTable::firstEmpty() const {
    for (int s = 0; s < kSlots; s++) {
        if (isEmpty(s)) return s;
    }
    return -1;
}

void SlotTable::assign(int s, const char* a) {
    int old = find(a);
    if (old >= 0) clear(old);
    strncpy(_addr[s], a, kAddrLen - 1);
    _addr[s][kAddrLen - 1] = '\0';
}

void SlotTable::clear(int s) { _addr[s][0] = '\0'; }

int SlotTable::returnTarget(int pairingSlot, int preferred) const {
    if (preferred >= 0 && preferred != pairingSlot && !isEmpty(preferred)) return preferred;
    for (int s = 0; s < kSlots; s++) {
        if (s != pairingSlot && !isEmpty(s)) return s;
    }
    return -1;
}

void SlotTable::adopt(const char* a) {
    if (find(a) >= 0) return;
    int s = firstEmpty();
    if (s >= 0) assign(s, a);
}
