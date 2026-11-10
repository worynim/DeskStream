// worynim@gmail.com
/**
 * @file agent_ble.cpp
 * @brief agent_ble.h 구현
 */
#include "agent_ble.h"

#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#include <string.h>

namespace am {

namespace {

// Nordic UART Service (Stats_Monitor 와 같은 UUID — 데몬은 이름 "DeskStream Agent" 로 구분)
const char* const kServiceUuid = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
const char* const kRxUuid = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";   // PC → 장치 (write)
const char* const kTxUuid = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";   // 장치 → PC (notify)
constexpr size_t kRxBufBytes = 4096;   // 상세 답변이 든 줄(약 2KB)이 통째로 들어간다
constexpr size_t kNotifyChunk = 20;   // 기본 MTU(23) 에서도 되는 크기

NimBLECharacteristic* tx = nullptr;
StreamBufferHandle_t rxBuf = nullptr;
volatile bool connected = false;

// 연결 직후 LE 데이터 길이 확장을 요청한다 — 한 무선 패킷에 더 많이 실어 2KB 상세 답변을 빨리 보낸다 (지원하지 않으면 무시).
// 연결 간격(updateConnParams)은 요청하지 않는다: Windows 는 장치가 요청한 값을 레지스트리
// (BTHPORT\Parameters\Devices\<주소>\LERemoteConnParams*)에 저장해 다음 연결부터 처음부터 적용하는데,
// 15~30ms 로 요청했더니 노트북 내장 Bluetooth 에서 새 연결의 서비스 탐색이 끝나지 않고 BT 마우스까지 멈췄다 (0.6.1).
constexpr uint16_t kDataLenOctets = 251;

class ServerCb : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override {
        connected = true;
        s->setDataLen(info.getConnHandle(), kDataLenOctets);
    }
    void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
        connected = false;
        NimBLEDevice::startAdvertising();
    }
};

class RxCb : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
        NimBLEAttValue v = c->getValue();
        xStreamBufferSend(rxBuf, v.data(), v.length(), 0);   // 가득 차면 버림 (줄 조립에서 걸러짐)
    }
};

}  // namespace

void bleBegin(const char* name) {
    rxBuf = xStreamBufferCreate(kRxBufBytes, 1);
    NimBLEDevice::init(name);
    NimBLEServer* server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCb());
    NimBLEService* svc = server->createService(kServiceUuid);
    NimBLECharacteristic* rx = svc->createCharacteristic(kRxUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    rx->setCallbacks(new RxCb());
    tx = svc->createCharacteristic(kTxUuid, NIMBLE_PROPERTY::NOTIFY);
    svc->start();
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->setName(name);
    adv->addServiceUUID(kServiceUuid);
    adv->enableScanResponse(true);
    adv->start();
}

bool bleConnected() { return connected; }

size_t bleRead(char* buf, size_t cap) { return rxBuf ? xStreamBufferReceive(rxBuf, buf, cap, 0) : 0; }

void bleWriteLine(const char* line) {
    if (!connected || !tx) return;
    size_t n = strlen(line);
    for (size_t off = 0; off < n; off += kNotifyChunk) {
        size_t len = n - off < kNotifyChunk ? n - off : kNotifyChunk;
        tx->setValue((const uint8_t*)line + off, len);
        tx->notify();
    }
    tx->setValue((const uint8_t*)"\n", 1);
    tx->notify();
}

}  // namespace am
