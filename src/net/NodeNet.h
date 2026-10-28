#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../app/RingQueue.h"
#include "NetFrame.h"

/*
NodeNet (PAR-013, step 11a): the ESP-NOW transport of the Node's remote
command path. No router: every node listens on one fixed channel. Commands are
broadcast (sent kCommandRepeats times; receivers drop repeats by seq), replies
go back unicast to the sender, which gets MAC-level retries.

Nothing here blocks the loop: frames to send wait in an outbox and service()
sends at most one per call, kTxSpacingMs apart (sending ~40 frames back to back
dropped 9-20 audio DMA buffers per node, 2026-10-25). The receive callback runs
in the WiFi task and only copies the frame into a queue; decoding, running the
command and replying happen in the Node's loop through poll(). Node build only.
*/
#ifndef NET_ID
#define NET_ID 0x52
#endif
#ifndef NET_CHANNEL
#define NET_CHANNEL 1
#endif

class NodeNet {
public:
    static constexpr int kCommandRepeats = 3;
    static constexpr size_t kOutbox = 48;
    static constexpr unsigned long kTxSpacingMs = 4;
    // Parts after the first of one reply go this far apart: four nodes answering
    // a PARAM DUMP then reach the gateway at ~6 KB/s, under what its USB serial
    // prints (11.5 KB/s at 115200), so the gateway keeps its audio.
    static constexpr unsigned long kReplySpacingMs = 150;

    struct Received {
        uint8_t mac[6];
        net::Frame frame;
    };

    struct Stats {
        uint32_t rxFrames = 0;
        uint32_t rxQueueFull = 0;
        uint32_t rxRejected = 0;  // not a frame, another network, or a repeat
        uint32_t txFrames = 0;
        uint32_t txFailed = 0;
        uint32_t txOutboxFull = 0;
        uint32_t commandsRun = 0;
    };

    bool begin();
    // Back on NET_CHANNEL after WiFi joined an access point (a failed OTA firmware update, step 11d).
    void restoreChannel();
    bool started() const { return _started; }

    // Queues a command line to `target` ("*" or a node name), kCommandRepeats
    // broadcasts. Returns its seq.
    uint16_t sendCommand(const char* target, const char* text);
    // Queues one reply part, unicast to the command's sender.
    bool sendReply(const uint8_t mac[6], uint16_t seq, uint8_t part, bool last, const char* myName, const char* text,
                   size_t len);
    // Sends at most one queued frame; call once per loop pass.
    void service(unsigned long nowMs);

    // The next valid frame of this network (repeats dropped). False when none.
    bool poll(Received& out);

    Stats& stats() { return _stats; }

private:
    struct Outgoing {
        uint8_t mac[6];
        unsigned long notBeforeMs;
        net::Frame frame;
    };
    bool enqueue(const uint8_t mac[6], const net::Frame& frame, unsigned long notBeforeMs = 0);
    bool ensurePeer(const uint8_t mac[6]);

    bool _started = false;
    uint16_t _nextSeq = 1;
    net::SeqFilter _filter;
    Stats _stats;
    // kOutbox entries (~13 KB), on the heap at begin(): static RAM is full.
    RingQueue<Outgoing, kOutbox>* _outbox = nullptr;
    unsigned long _lastTxMs = 0;
    unsigned long _lastReplyAtMs = 0;
};
