// Node radio transport (step 11a, PAR-013; owner 2026-10-25 split by role): NET commands, the
// remote command path and its replies. Interim: VEKTOR replaces it
// (docs/decisions/2026-10-25-vektor-replaces-net.md). Moved verbatim from ResonantNodeApp.cpp. The reply and join state
// live in NetGateway (ResonantNodeNetGateway.h, step 11i N3).
#include "ResonantNodeApp.h"
#include <Arduino.h>
#include <string.h>
#include "../../app/NodeRoster.h"
#include <ParamRegistry.h>
#include "../../app/ParamIds.h"
#include "../../app/TimeBase.h"
#include "../../app/SerialOut.h"
#include "../../app/TextParse.h"
#include "ResonantNodeInternal.h"
#include "../../net/RemoteCommandPolicy.h"

using namespace node_internal;

namespace {

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
    if (_netGateway.replyOpen() && !_selfTest.active()) {
        _netGateway.closeOpenReply();
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
            _netGateway.handleReplyFrame(f);
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

    // Everything printed from here goes back as the reply (a reply still open
    // is closed first); the scope's end sends the last frame.
    NetGateway::ReplyScope reply(_netGateway, received.mac, f.seq, _nodeName);
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
    if (_selfTest.active()) {
        reply.keepOpen();  // the self-test reports over the next seconds; pollNet() closes it
    }
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
