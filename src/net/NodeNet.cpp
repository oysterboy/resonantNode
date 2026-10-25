#include "NodeNet.h"

#if !defined(ANALYZER_MODE) && !defined(EMITTER_MODE) && defined(ARDUINO_ARCH_ESP32)

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string.h>

namespace {

struct RawFrame {
    uint8_t mac[6];
    uint8_t len;
    uint8_t data[net::kMaxFrameBytes];
};

constexpr int kQueueDepth = 32;  // a gateway takes the replies of every node at once
QueueHandle_t gRxQueue = nullptr;
volatile uint32_t gRxQueueFull = 0;
volatile uint32_t gTxFailed = 0;
const uint8_t kBroadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// WiFi task: copy only.
void onReceive(const uint8_t* mac, const uint8_t* data, int len) {
    if (gRxQueue == nullptr || mac == nullptr || data == nullptr || len <= 0 ||
        len > static_cast<int>(net::kMaxFrameBytes)) {
        return;
    }
    RawFrame raw;
    memcpy(raw.mac, mac, 6);
    raw.len = static_cast<uint8_t>(len);
    memcpy(raw.data, data, static_cast<size_t>(len));
    if (xQueueSend(gRxQueue, &raw, 0) != pdTRUE) {
        ++gRxQueueFull;
    }
}

void onSent(const uint8_t*, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        ++gTxFailed;
    }
}

}  // namespace

bool NodeNet::begin() {
    if (_started) {
        return true;
    }
    gRxQueue = xQueueCreate(kQueueDepth, sizeof(RawFrame));
    if (gRxQueue == nullptr) {
        return false;
    }
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    // A fixed channel without an access point.
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(NET_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    if (esp_now_init() != ESP_OK) {
        return false;
    }
    esp_now_register_recv_cb(onReceive);
    esp_now_register_send_cb(onSent);
    if (!ensurePeer(kBroadcast)) {
        return false;
    }
    _started = true;
    return true;
}

bool NodeNet::ensurePeer(const uint8_t mac[6]) {
    if (esp_now_is_peer_exist(mac)) {
        return true;
    }
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0;  // the current channel
    peer.encrypt = false;
    return esp_now_add_peer(&peer) == ESP_OK;
}

bool NodeNet::enqueue(const uint8_t mac[6], const net::Frame& frame) {
    if (_outCount >= kOutbox) {
        ++_stats.txOutboxFull;
        return false;
    }
    Outgoing& o = _outbox[(_outHead + _outCount) % kOutbox];
    memcpy(o.mac, mac, 6);
    o.frame = frame;
    ++_outCount;
    return true;
}

void NodeNet::service(unsigned long nowMs) {
    if (!_started || _outCount == 0 || nowMs - _lastTxMs < kTxSpacingMs) {
        return;
    }
    Outgoing& o = _outbox[_outHead];
    _outHead = (_outHead + 1) % kOutbox;
    --_outCount;
    _lastTxMs = nowMs;
    uint8_t buf[net::kMaxFrameBytes];
    const size_t len = net::encode(o.frame, buf, sizeof(buf));
    if (len == 0 || !ensurePeer(o.mac)) {
        ++_stats.txFailed;
        return;
    }
    ++_stats.txFrames;
    if (esp_now_send(o.mac, buf, len) != ESP_OK) {
        ++_stats.txFailed;
    }
}

uint16_t NodeNet::sendCommand(const char* target, const char* text) {
    net::Frame frame;
    frame.type = net::FrameType::Command;
    frame.netId = NET_ID;
    frame.seq = _nextSeq++;
    if (_nextSeq == 0) {
        _nextSeq = 1;
    }
    net::setTarget(frame, target);
    net::setText(frame, text, strlen(text));
    if (!_started) {
        return frame.seq;
    }
    for (int i = 0; i < kCommandRepeats; ++i) {
        enqueue(kBroadcast, frame);
    }
    return frame.seq;
}

bool NodeNet::sendReply(const uint8_t mac[6], uint16_t seq, uint8_t part, bool last, const char* myName,
                        const char* text, size_t len) {
    if (!_started) {
        return false;
    }
    net::Frame frame;
    frame.type = net::FrameType::Reply;
    frame.netId = NET_ID;
    frame.seq = seq;
    frame.part = part;
    frame.last = last;
    net::setTarget(frame, myName);
    net::setText(frame, text, len);
    return enqueue(mac, frame);
}

bool NodeNet::poll(Received& out) {
    _stats.rxQueueFull = gRxQueueFull;
    _stats.txFailed += gTxFailed;
    gTxFailed = 0;
    if (gRxQueue == nullptr) {
        return false;
    }
    RawFrame raw;
    while (xQueueReceive(gRxQueue, &raw, 0) == pdTRUE) {
        ++_stats.rxFrames;
        net::Frame frame;
        if (!net::decode(raw.data, raw.len, frame) || frame.netId != NET_ID) {
            ++_stats.rxRejected;
            continue;
        }
        // Commands repeat (kCommandRepeats); replies are unicast and do not.
        if (frame.type == net::FrameType::Command && !_filter.accept(raw.mac, frame.seq)) {
            ++_stats.rxRejected;
            continue;
        }
        memcpy(out.mac, raw.mac, 6);
        out.frame = frame;
        return true;
    }
    return false;
}


#endif
