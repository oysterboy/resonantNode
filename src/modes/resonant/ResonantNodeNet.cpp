// Node radio transport (step 11a, PAR-013; owner 2026-10-25 split by role): NET commands, the
// remote command path and its replies. Interim: VEKTOR replaces it
// (docs/decisions/2026-10-25-vektor-replaces-net.md). Moved verbatim from ResonantNodeApp.cpp.
#include "ResonantNodeApp.h"
#include <Arduino.h>
#include <string.h>
#include <new>
#include "../../app/NodeRoster.h"
#include <ParamRegistry.h>
#include "../../app/ParamIds.h"
#include "../../app/TimeBase.h"
#include "../../app/SerialOut.h"
#include "../../app/TextParse.h"
#include "ResonantNodeInternal.h"

using namespace node_internal;

namespace {

// The reply of one remote command: every line it prints goes back as a reply
// frame. A line is held until the next one arrives, so the last frame carries
// the "last" flag.
// The reply of one remote command is everything it prints, as a byte stream
// cut into frames: a newline travels as net::kLineEnd, a long line (RB STATUS)
// runs on into the next frame, and the console's 1 KB flushes of one line join
// up again. The gateway's LineJoiner prints whole lines.
struct NetReply {
    NodeNet* net = nullptr;
    uint8_t mac[6] = {};
    uint16_t seq = 0;
    const char* name = nullptr;
    uint8_t part = 0;
    char held[net::kMaxText];
    size_t heldLen = 0;

    void send(bool last) {
        net->sendReply(mac, seq, part++, last, name, held, heldLen);
        heldLen = 0;
    }

    static void tee(void* ctx, const uint8_t* data, size_t len) {
        NetReply& r = *static_cast<NetReply*>(ctx);
        for (size_t i = 0; i < len; ++i) {
            const char ch = static_cast<char>(data[i]);
            if (ch == '\r') {
                continue;
            }
            if (r.heldLen == sizeof(r.held)) {
                r.send(false);
            }
            r.held[r.heldLen++] = ch == '\n' ? net::kLineEnd : ch;
        }
    }
};

// A command that runs on after its call returns (SELFTEST: a state machine over
// many loop passes) keeps its reply open until it is done; pollNet() closes it.
NetReply gOpenReply;
bool gOpenReplyActive = false;

// The gateway's reassembly, one per replying node (by name).
struct NetJoinSlot {
    char name[net::kTargetLen + 1] = {};
    uint16_t seq = 0;
    net::LineJoiner<2560> joiner;
};
// On the heap at the first reply (only a gateway needs them; static RAM is full).
constexpr size_t kJoinSlots = 5;
NetJoinSlot* gJoin = nullptr;

NetJoinSlot* joinSlotFor(const char* name, uint16_t seq) {
    if (gJoin == nullptr) {
        gJoin = new (std::nothrow) NetJoinSlot[kJoinSlots];
        if (gJoin == nullptr) {
            return nullptr;
        }
    }
    for (size_t i = 0; i < kJoinSlots; ++i) {
        NetJoinSlot& s = gJoin[i];
        if (strcmp(s.name, name) == 0) {
            if (s.seq != seq) {
                s.seq = seq;
                s.joiner.reset();
            }
            return &s;
        }
    }
    static size_t next = 0;
    NetJoinSlot& s = gJoin[next];
    next = (next + 1) % kJoinSlots;
    strncpy(s.name, name, net::kTargetLen);
    s.name[net::kTargetLen] = '\0';
    s.seq = seq;
    s.joiner.reset();
    return &s;
}

void printMac(const uint8_t mac[6]) {
    static const char* kHex = "0123456789ABCDEF";
    for (int i = 0; i < 6; ++i) {
        if (i > 0) {
            serialOut().print(':');
        }
        serialOut().print(kHex[mac[i] >> 4]);
        serialOut().print(kHex[mac[i] & 0x0F]);
    }
}

}  // namespace

// NET STATUS, or NET <target> <command line>: this node is the gateway. The
// command is broadcast; a target that includes this node runs it here as well.
void Node::handleNetCommand(const char* line) {
    const char* rest = text_parse::rest(line, "NET");
    if (*rest == '\0' || equalsIgnoreCase(rest, "STATUS")) {
        printNetStatus();
        return;
    }
    char target[net::kTargetLen + 1] = {};
    size_t n = 0;
    while (rest[n] != '\0' && rest[n] != ' ' && n < net::kTargetLen) {
        target[n] = rest[n];
        ++n;
    }
    const char* text = rest + n;
    while (*text == ' ') {
        ++text;
    }
    if (!net::remoteCommandAllowed(text)) {
        serialOut().println("NET ERR usage=NET <target|*> PARAM ...|RB ...|SELFTEST ...|NET PING");
        return;
    }
    if (net::remoteCommandNeedsOneTarget(text) && strcmp(target, "*") == 0) {
        serialOut().println("NET ERR selftest_needs_one_target (one node at a time, the others silent)");
        return;
    }
    if (strlen(text) > net::kMaxText) {
        serialOut().println("NET ERR command_too_long");
        return;
    }
    if (!_net.started()) {
        serialOut().println("NET ERR radio_off");
        return;
    }
    const uint16_t seq = _net.sendCommand(target, text);
    serialOut().print("NET sent seq=");
    serialOut().print(seq);
    serialOut().print(" target=");
    serialOut().print(target);
    serialOut().print(" text=");
    serialOut().println(text);
    if (net::targetMatches(target, _nodeName)) {
        if (equalsIgnoreCase(text, "NET PING")) {
            printNetStatus();
        } else {
            handleSerialLine(text);
        }
    }
}

void Node::pollNet() {
    if (gOpenReplyActive && _selfTest.phase == SelfTestState::Phase::Idle) {
        serialOut().setTee(nullptr, nullptr);
        gOpenReply.send(true);
        gOpenReplyActive = false;
    }
    _net.service(timebase::nowMs());
    NodeNet::Received received;
    // One frame per loop pass, and only with 4 KB free in the UART ring: a
    // gateway printing a whole NET * PARAM DUMP (~160 lines) at once waited on
    // the UART and dropped 52 audio DMA buffers (2026-10-25). Frames wait in the
    // receive queue meanwhile; a full queue loses replies, never audio.
    for (int handled = 0; handled < 1 && Serial.availableForWrite() > 4096 && _net.poll(received); ++handled) {
        const net::Frame& f = received.frame;
        if (f.type == net::FrameType::Reply) {
            NetJoinSlot* slot = joinSlotFor(f.target, f.seq);
            if (slot == nullptr) {
                continue;
            }
            slot->joiner.feed(f.text, f.textLen, f.last, [&](const char* line, size_t n) {
                serialOut().print("NET reply from=");
                serialOut().print(f.target);
                serialOut().print(" seq=");
                serialOut().print(f.seq);
                serialOut().print(" part=");
                serialOut().print(f.part);
                serialOut().print(' ');
                serialOut().write(reinterpret_cast<const uint8_t*>(line), n);
                serialOut().println();
            });
            if (f.last) {
                serialOut().print("NET reply from=");
                serialOut().print(f.target);
                serialOut().print(" seq=");
                serialOut().print(f.seq);
                serialOut().print(" done parts=");
                serialOut().println(static_cast<unsigned>(f.part) + 1U);
            }
            continue;
        }
        if (!net::targetMatches(f.target, _nodeName)) {
            continue;
        }
        runRemoteCommand(received);
    }
}

void Node::runRemoteCommand(const NodeNet::Received& received) {
    const net::Frame& f = received.frame;
    serialOut().print("NET cmd from=");
    printMac(received.mac);
    serialOut().print(" seq=");
    serialOut().print(f.seq);
    serialOut().print(" target=");
    serialOut().print(f.target);
    serialOut().print(" text=");
    serialOut().println(f.text);

    if (gOpenReplyActive) {
        // A new command ends a reply still open (its output stops here).
        serialOut().setTee(nullptr, nullptr);
        gOpenReply.send(true);
        gOpenReplyActive = false;
    }
    NetReply& reply = gOpenReply;
    reply = NetReply();
    reply.net = &_net;
    memcpy(reply.mac, received.mac, 6);
    reply.seq = f.seq;
    reply.name = _nodeName;
    serialOut().setTee(&NetReply::tee, &reply);
    if (!net::remoteCommandAllowed(f.text)) {
        serialOut().println("NET ERR not_allowed");
    } else if (net::remoteCommandNeedsOneTarget(f.text) && strcmp(f.target, "*") == 0) {
        serialOut().println("NET ERR selftest_needs_one_target (one node at a time, the others silent)");
    } else if (equalsIgnoreCase(f.text, "NET PING")) {
        printNetStatus();
    } else {
        if (net::remoteCommandNeedsOneTarget(f.text)) {
            serialOut().println("NET note=selftest_by_radio rule=one_node_at_a_time_others_silent");
        }
        handleSerialLine(f.text);
    }
    ++_net.stats().commandsRun;
    if (_selfTest.phase != SelfTestState::Phase::Idle) {
        gOpenReplyActive = true;  // the self-test reports over the next seconds
        return;
    }
    serialOut().setTee(nullptr, nullptr);
    reply.send(true);
}

void Node::printNetStatus() {
    const NodeNet::Stats& st = _net.stats();
    serialOut().print("NET node=");
    serialOut().print(_nodeName);
    serialOut().print(" started=");
    serialOut().print(_net.started() ? 1 : 0);
    serialOut().print(" build=");
    serialOut().print(BUILD_GIT_SHA);
    serialOut().print(" silent=");
    serialOut().print(_silent ? 1 : 0);
    serialOut().print(" uptime_s=");
    serialOut().print(timebase::nowMs() / 1000UL);
    serialOut().print(" rx=");
    serialOut().print(st.rxFrames);
    serialOut().print(" rejected=");
    serialOut().print(st.rxRejected);
    serialOut().print(" queue_full=");
    serialOut().print(st.rxQueueFull);
    serialOut().print(" tx=");
    serialOut().print(st.txFrames);
    serialOut().print(" tx_failed=");
    serialOut().print(st.txFailed);
    serialOut().print(" outbox_full=");
    serialOut().print(st.txOutboxFull);
    serialOut().print(" commands=");
    serialOut().println(st.commandsRun);
}
