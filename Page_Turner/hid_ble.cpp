// worynim@gmail.com
/**
 * @file hid_ble.cpp
 * @brief BLE HID 장치와 신호 전송 태스크
 * @details 신호(휠 30칸 × 250ms 등)는 길게는 수 초 걸린다. 별도 태스크가 보내므로 버튼·화면은 멈추지 않고,
 *          보내는 중에 들어온 신호는 버린다 (쌓아 두면 손을 뗀 뒤에도 계속 넘어간다 — DESIGN §6.3).
 */
#include "hid_ble.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <Preferences.h>
#include <string.h>

#include "config.h"
#include "slots.h"

namespace hid {

enum : uint8_t { RID_KEY = 1, RID_CONSUMER = 2, RID_MOUSE = 3 };

static const uint8_t kReportMap[] = {
    // 키보드 (ID 1): 수식키 1B + 예약 1B + 키 6B
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, RID_KEY,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,
    0xC0,
    // 미디어 키 (ID 2): 16비트 Consumer 사용 코드 1개
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, RID_CONSUMER,
    0x15, 0x00, 0x26, 0xFF, 0x03, 0x19, 0x00, 0x2A, 0xFF, 0x03, 0x75, 0x10, 0x95, 0x01, 0x81, 0x00,
    0xC0,
    // 마우스 (ID 3): 버튼 3비트 + 패딩, X, Y, 휠 (상대값 int8)
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, RID_MOUSE, 0x09, 0x01, 0xA1, 0x00,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x05, 0x81, 0x03,
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06,
    0xC0, 0xC0,
};

// HID 사용 코드
constexpr uint8_t KEY_PAGE_UP = 0x4B, KEY_PAGE_DOWN = 0x4E, KEY_UP = 0x52, KEY_DOWN = 0x51;
constexpr uint16_t CC_VOLUME_UP = 0xE9, CC_VOLUME_DOWN = 0xEA;

static NimBLEServer* server = nullptr;
static NimBLECharacteristic* inKey = nullptr;
static NimBLECharacteristic* inConsumer = nullptr;
static NimBLECharacteristic* inMouse = nullptr;
static QueueHandle_t jobs = nullptr;
static volatile bool isConnected = false;
static volatile bool busy = false;
static volatile bool stateChanged = true;

// ---------- 리포트 전송 ----------
static void send(NimBLECharacteristic* c, const uint8_t* data, size_t len) {
    if (!isConnected) return;
    c->setValue(data, len);
    c->notify();
}

static void key(uint8_t code) {
    uint8_t down[8] = {0, 0, code, 0, 0, 0, 0, 0};
    uint8_t up[8] = {0};
    send(inKey, down, 8);
    delay(15);
    send(inKey, up, 8);
}

static void consumer(uint16_t usage) {
    uint8_t down[2] = {(uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8)};
    uint8_t up[2] = {0, 0};
    send(inConsumer, down, 2);
    delay(15);
    send(inConsumer, up, 2);
}

static void mouse(int8_t x, int8_t wheel) {
    uint8_t r[4] = {0, (uint8_t)x, 0, (uint8_t)wheel};
    send(inMouse, r, 4);
}

// 커서를 화면 오른쪽 끝으로 치운다 (y는 그대로)
static void parkCursor() {
    for (int i = 0; i < 20; i++) {
        mouse(127, 0);
        delay(5);
    }
}

static void run(const Job& j) {
    for (int i = 0; i < j.count; i++) {
        if (i) delay(j.gapMs);
        switch (j.signal) {
            case Signal::Volume:
            case Signal::Camera: consumer(j.up ? CC_VOLUME_UP : CC_VOLUME_DOWN); break;
            case Signal::Page:   key(j.up ? KEY_PAGE_UP : KEY_PAGE_DOWN); break;
            case Signal::Arrow:  key(j.up ? KEY_UP : KEY_DOWN); break;
            case Signal::Wheel:
                if (i == 0 && j.parkCursor) parkCursor();
                mouse(0, j.up ? 1 : -1);
                break;
        }
    }
}

static void senderTask(void*) {
    Job j;
    for (;;) {
        if (xQueueReceive(jobs, &j, portMAX_DELAY) == pdTRUE) {
            run(j);
            busy = false;
        }
    }
}

// ---------- 슬롯 ----------
// active   = 쓰는 슬롯 (페어링 대기 중엔 돌아갈 슬롯)
// pairSlot = 새 기기를 받을 슬롯 (-1 = 대기 아님). 기존 기기는 새 기기가 본딩될 때에야 지운다.
static SlotTable slots;
static int active = 0;
static int pairSlot = -1;
static uint32_t pairStartMs = 0;

static void saveSlots() {
    Preferences prefs;
    prefs.begin("pt_slots", false);
    for (int s = 0; s < kNumSlots; s++) {
        char key[3] = {'s', (char)('0' + s), '\0'};
        prefs.putString(key, slots.addr(s));
    }
    prefs.putUChar("act", (uint8_t)active);
    prefs.end();
}

// 슬롯 주소와 같은 본딩 주소(주소 종류 포함)를 찾는다
static bool bondAddress(int s, NimBLEAddress& out) {
    for (int i = 0; i < NimBLEDevice::getNumBonds(); i++) {
        NimBLEAddress a = NimBLEDevice::getBondedAddress(i);
        if (slots.find(a.toString().c_str()) == s) {
            out = a;
            return true;
        }
    }
    return false;
}

// 저장된 슬롯과 실제 본딩을 맞춘다: 본딩이 없는 슬롯은 비우고, 슬롯에 없는 본딩은 빈 슬롯에 넣는다.
// 쓰던 슬롯이 비어 있으면 다른 페어링 슬롯을 쓰고, 페어링된 기기가 없으면 그 슬롯에서 새 기기를 기다린다.
static void loadSlots() {
    Preferences prefs;
    prefs.begin("pt_slots", true);
    for (int s = 0; s < kNumSlots; s++) {
        char key[3] = {'s', (char)('0' + s), '\0'};
        String a = prefs.getString(key, "");
        if (a.length()) slots.assign(s, a.c_str());
    }
    active = prefs.getUChar("act", 0);
    prefs.end();
    if (active >= kNumSlots) active = 0;

    NimBLEAddress tmp;
    for (int s = 0; s < kNumSlots; s++) {
        if (!slots.isEmpty(s) && !bondAddress(s, tmp)) slots.clear(s);
    }
    for (int i = 0; i < NimBLEDevice::getNumBonds(); i++) slots.adopt(NimBLEDevice::getBondedAddress(i).toString().c_str());
    if (slots.isEmpty(active)) {
        int other = slots.returnTarget(active, -1);
        if (other >= 0) active = other;
        else pairSlot = active;   // 처음 쓰는 보드: 돌아갈 곳 없이 기다린다
    }
    saveSlots();
}

// 광고 필터: 대기 중이면 누구나(슬롯 기기는 연결 때 거른다), 아니면 쓰는 슬롯 기기만(화이트리스트).
// 컨트롤러가 화이트리스트 변경을 거부하면(끊김 처리 중 광고가 다시 켜진 순간 등) false — loop()에서 다시 시도한다.
static volatile bool filterRetry = false;

static bool applyFilter() {
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->stop();
    for (size_t n = NimBLEDevice::getWhiteListCount(); n > 0; n--) {   // 실패하면 NimBLE가 주소를 되돌려 넣으므로 횟수를 제한한다
        if (!NimBLEDevice::whiteListRemove(NimBLEDevice::getWhiteListAddress(0))) {
            filterRetry = true;
            return false;
        }
    }
    NimBLEAddress a;
    if (pairSlot < 0 && bondAddress(active, a)) {
        if (!NimBLEDevice::whiteListAdd(a)) {
            filterRetry = true;
            return false;
        }
        adv->setScanFilter(false, true);
    } else {
        adv->setScanFilter(false, false);
    }
    if (server->getConnectedCount() == 0) adv->start();
    filterRetry = false;
    return true;
}

static void disconnectAll() {
    for (uint16_t h : server->getPeerDevices()) server->disconnect(h);
}

// 받아도 되는 기기인가 (신원 주소 기준): 대기 중엔 슬롯에 없는 새 기기만, 아니면 쓰는 슬롯 기기만
static bool allowed(const NimBLEConnInfo& info) {
    int s = slots.find(info.getIdAddress().toString().c_str());
    return pairSlot >= 0 ? s < 0 : s == active;
}

// ---------- 콜백 → 메인 넘기기 ----------
// NimBLE 콜백은 호스트 태스크에서 돈다. 슬롯 상태는 메인 루프만 바꾸도록, 콜백은 할 일만 기록한다.
static portMUX_TYPE pendMux = portMUX_INITIALIZER_UNLOCKED;
static int pendAdoptSlot = -1;                       // 새 기기를 넣을 슬롯 (-1 = 없음)
static char pendAdoptAddr[SlotTable::kAddrLen] = {};
static int pendClearSlot = -1;                       // 저장 공간이 차서 본딩을 지운 슬롯 (-1 = 없음)

// [메인] 새 기기가 본딩됐다: 그 슬롯의 기존 기기를 지우고 교체한다
static bool takePendingAdopt() {
    int s;
    char addr[SlotTable::kAddrLen];
    portENTER_CRITICAL(&pendMux);
    s = pendAdoptSlot;
    memcpy(addr, pendAdoptAddr, sizeof(addr));
    pendAdoptSlot = -1;
    portEXIT_CRITICAL(&pendMux);
    if (s < 0) return false;
    NimBLEAddress old;
    if (bondAddress(s, old)) NimBLEDevice::deleteBond(old);
    slots.assign(s, addr);
    active = s;
    pairSlot = -1;
    saveSlots();
    applyFilter();
    stateChanged = true;
    return true;
}

// [메인] 저장 공간 넘침으로 본딩이 지워진 슬롯을 비운다
static void takePendingClear() {
    portENTER_CRITICAL(&pendMux);
    int s = pendClearSlot;
    pendClearSlot = -1;
    portEXIT_CRITICAL(&pendMux);
    if (s < 0) return;
    slots.clear(s);
    saveSlots();
    stateChanged = true;
}

class ServerCb : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* srv, NimBLEConnInfo& info) override {
        if (!allowed(info)) {
            srv->disconnect(info);
            return;
        }
        isConnected = true;
        stateChanged = true;
    }
    // 본딩된 기기는 암호화가 끝나야 신원 주소가 확정되므로 여기서 한 번 더 거른다
    void onAuthenticationComplete(NimBLEConnInfo& info) override {
        if (!allowed(info)) {
            server->disconnect(info);
            return;
        }
        const int s = pairSlot;   // 한 번만 읽는다
        if (s >= 0 && info.isBonded()) {   // 교체는 메인 루프(hid::loop)가 한다
            std::string id = info.getIdAddress().toString();
            portENTER_CRITICAL(&pendMux);
            strncpy(pendAdoptAddr, id.c_str(), sizeof(pendAdoptAddr) - 1);
            pendAdoptAddr[sizeof(pendAdoptAddr) - 1] = '\0';
            pendAdoptSlot = s;
            portEXIT_CRITICAL(&pendMux);
        }
    }
    void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
        isConnected = server->getConnectedCount() > 0;
        busy = false;
        stateChanged = true;
    }
};

// 본딩 저장 공간이 차면 NimBLE 기본 처리는 가장 오래된 기기를 지운다 — 교체 중이면 그 슬롯의 기존 기기를 지운다
class DeviceCb : public NimBLEDeviceCallbacks {
    int onStoreStatus(struct ble_store_status_event* event, void* arg) override {
        const int s = pairSlot;   // 한 번만 읽는다
        NimBLEAddress old;
        if (s >= 0 && bondAddress(s, old)) {
            NimBLEDevice::deleteBond(old);   // 저장 공간은 지금 비워야 한다. 슬롯 표는 메인 루프가 비운다
            portENTER_CRITICAL(&pendMux);
            pendClearSlot = s;
            portEXIT_CRITICAL(&pendMux);
            return 0;
        }
        return NimBLEDeviceCallbacks::onStoreStatus(event, arg);
    }
};

void begin() {
    jobs = xQueueCreate(1, sizeof(Job));
    xTaskCreate(senderTask, "hid_send", 4096, nullptr, 2, nullptr);

    NimBLEDevice::init(PT_BLE_NAME);
    NimBLEDevice::setDeviceCallbacks(new DeviceCb());
    NimBLEDevice::setSecurityAuth(true, false, true);   // 본딩, MITM 없음(Just Works), SC
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCb());
    server->advertiseOnDisconnect(true);   // NimBLE 2.x 기본값은 false — 끊긴 뒤 다시 검색·재연결되게 한다

    NimBLEHIDDevice* dev = new NimBLEHIDDevice(server);
    inKey = dev->getInputReport(RID_KEY);
    inConsumer = dev->getInputReport(RID_CONSUMER);
    inMouse = dev->getInputReport(RID_MOUSE);
    dev->setManufacturer("DeskStream");
    dev->setPnp(0x02, 0x303A, 0x8001, 0x0100);
    dev->setHidInfo(0x00, 0x01);
    dev->setReportMap((uint8_t*)kReportMap, sizeof(kReportMap));
    dev->setBatteryLevel(100);
    server->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->setAppearance(0x03C1);   // HID Keyboard
    adv->addServiceUUID(dev->getHidService()->getUUID());
    adv->setName(PT_BLE_NAME);
    adv->enableScanResponse(true);

    loadSlots();
    applyFilter();
}

bool connected() { return isConnected; }

bool takeStateChanged() {
    bool c = stateChanged;
    stateChanged = false;
    return c;
}

bool post(const Job& job) {
    if (!isConnected || busy) return false;
    busy = true;
    if (xQueueSend(jobs, &job, 0) != pdTRUE) {
        busy = false;
        return false;
    }
    return true;
}

int activeSlot() { return pairSlot >= 0 ? pairSlot : active; }
bool slotPaired(int s) { return !slots.isEmpty(s); }
bool pairing() { return pairSlot >= 0; }

// 돌아갈 슬롯: 쓰던 슬롯이 페어링돼 있으면 그 슬롯(교체 중인 슬롯이어도 기존 기기로), 아니면 다른 페어링 슬롯
static int returnSlot() { return slots.isEmpty(active) ? slots.returnTarget(pairSlot, -1) : active; }

static void startPairing(int s, uint32_t nowMs) {
    pairSlot = s;
    pairStartMs = nowMs;
    disconnectAll();
    applyFilter();
    stateChanged = true;
}

void selectSlot(int s, uint32_t nowMs) {
    if (slots.isEmpty(s)) {
        startPairing(s, nowMs);
        return;
    }
    if (pairSlot < 0 && s == active && isConnected) return;   // 이미 그 기기에 연결됨
    pairSlot = -1;
    active = s;
    saveSlots();
    disconnectAll();
    applyFilter();
    stateChanged = true;
}

void repairSlot(int s, uint32_t nowMs) { startPairing(s, nowMs); }   // 기존 기기는 새 기기가 본딩될 때 지운다

uint8_t pairingSecondsLeft(uint32_t nowMs) {
    if (pairSlot < 0 || returnSlot() < 0) return 0;
    uint32_t elapsed = nowMs - pairStartMs;
    return elapsed >= kPairWaitMs ? 0 : (uint8_t)((kPairWaitMs - elapsed + 999) / 1000);
}

void cancelPairing() {
    if (pairSlot < 0) return;
    if (takePendingAdopt()) return;   // 그 사이 새 기기가 본딩됐다 — 취소보다 교체가 먼저
    int back = returnSlot();
    if (back < 0) return;   // 돌아갈 기기가 없다 — 계속 기다린다
    pairSlot = -1;
    active = back;
    saveSlots();
    disconnectAll();   // 대기 중 붙어 있던 낯선 기기를 끊는다
    applyFilter();
    stateChanged = true;
}

void loop(uint32_t nowMs) {
    takePendingClear();
    takePendingAdopt();
    if (filterRetry) applyFilter();
    if (pairSlot >= 0 && returnSlot() >= 0 && nowMs - pairStartMs >= kPairWaitMs) cancelPairing();   // 새 기기가 안 왔다
}

}  // namespace hid
