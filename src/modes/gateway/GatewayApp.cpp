#include "GatewayApp.h"

#include <Arduino.h>
#include <string.h>

#include "../../app/BuildInfo.h"
#include "../../app/FirmwareOta.h"
#include "../../app/NodeRoster.h"
#include "../../app/SerialOut.h"
#include "../../app/TextParse.h"
#include "../../app/TimeBase.h"
#include "../../net/RemoteCommandPolicy.h"

using text_parse::equalsIgnoreCase;
using text_parse::startsWithWordIgnoreCase;

namespace {

// A command line as it went by radio, with an OTA token's value hidden (the transcripts are bench evidence).
void printCommandText(const char* text) {
    const char* token = strstr(text, " token=");
    if (token == nullptr) {
        serialOut().println(text);
        return;
    }
    token += 7;
    serialOut().write(reinterpret_cast<const uint8_t*>(text), static_cast<size_t>(token - text));
    serialOut().print("<hidden>");
    const char* after = strchr(token, ' ');
    serialOut().println(after != nullptr ? after : "");
}

}  // namespace

void GatewayApp::begin() {
    printBuildIdentity(serialOut(), "gateway");
    const roster::Entry* me = roster::find(ESP.getEfuseMac());
    _nodeName = me != nullptr ? me->name : "unknown";
    // The value a roster line takes (NodeRoster.h): the eFuse MAC as ESP.getEfuseMac() reads it.
    serialOut().print("GATEWAY efuse_mac=0x");
    const uint64_t mac = ESP.getEfuseMac();
    char hex[17];
    snprintf(hex, sizeof(hex), "%012llX", static_cast<unsigned long long>(mac));
    serialOut().println(hex);
    const bool up = _net.begin();
    _gateway.begin();
    serialOut().print("GATEWAY node=");
    serialOut().print(_nodeName);
    serialOut().print(" net_started=");
    serialOut().print(up ? 1 : 0);
    serialOut().print(" channel=");
    serialOut().println(NET_CHANNEL);
    serialOut().println("GATEWAY CMD: NET <name|*> <command> / NET PING / NET STATUS / OTA url=... token=... / HELP");
}

void GatewayApp::update() {
    pollUsb();
    pollNet();
    ota::confirmRunningImage(serialOut(), _net.started());
}

void GatewayApp::pollUsb() {
    serial_line::pollLines(Serial, _usbLine, serialOut(),
                           "ERR line_too_long max=", [this](const char* line) { handleLine(line); });
}

// What the radio brought: a board's reply or a mirrored console line, printed whole. A command from another sender is
// not run here (the gateway has no commands of its own beyond NET PING, which the console answers).
void GatewayApp::pollNet() {
    if (!_net.started()) {
        return;
    }
    _net.service(timebase::nowMs());
    NodeNet::Received received;
    // Many frames per pass: nothing here waits for audio. The UART ring is the only limit.
    for (int handled = 0; handled < 8 && Serial.availableForWrite() > 2048 && _net.poll(received); ++handled) {
        const net::Frame& f = received.frame;
        if (f.type == net::FrameType::Reply) {
            _gateway.handleReplyFrame(f);
        } else if (f.type == net::FrameType::Log) {
            _gateway.handleLogFrame(f);
        }
    }
}

void GatewayApp::handleLine(const char* line) {
    if (startsWithWordIgnoreCase(line, "NET")) {
        handleNetCommand(line);
    } else if (startsWithWordIgnoreCase(line, "OTA")) {
        handleOtaCommand(line);
    } else if (equalsIgnoreCase(line, "HELP")) {
        printHelp();
    } else {
        serialOut().println("GATEWAY ERR unknown_command (HELP)");
    }
}

// NET STATUS / NET PING (the gateway's own counters), or NET <target|*> <command>: broadcast to the boards.
void GatewayApp::handleNetCommand(const char* line) {
    const char* rest = text_parse::rest(line, "NET");
    if (*rest == '\0' || equalsIgnoreCase(rest, "STATUS") || equalsIgnoreCase(rest, "PING")) {
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
        serialOut().println("NET ERR usage=NET <target|*> PARAM ...|RB ...|ECHO ...|WHISPER ...|OTA ...|NET PING");
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
    if (seq == 0) {
        serialOut().println("NET ERR outbox_full (nothing sent)");
        return;
    }
    serialOut().print("NET sent seq=");
    serialOut().print(seq);
    serialOut().print(" target=");
    serialOut().print(target);
    serialOut().print(" text=");
    printCommandText(text);
}

// The gateway's own firmware update, as on the other modes: send what the outbox holds before WiFi takes the radio
// off its channel, pull and reboot; on failure back to the channel.
void GatewayApp::handleOtaCommand(const char* line) {
    if (_net.started()) {
        _net.flush(1500);
    }
    ota::runCommand(line, serialOut());
    if (_net.started()) {
        _net.restoreChannel();
    }
}

void GatewayApp::printNetStatus() {
    const NodeNet::Stats& st = _net.stats();
    serialOut().print("NET node=");
    serialOut().print(_nodeName);
    serialOut().print(" role=gateway started=");
    serialOut().print(_net.started() ? 1 : 0);
    serialOut().print(" build=");
    serialOut().print(BUILD_GIT_SHA);
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
    serialOut().println(st.txOutboxFull);
}

void GatewayApp::printHelp() {
    serialOut().println(
        "GATEWAY CMD: NET <name|*> <command>   PARAM / RB / ECHO / WHISPER / SELFTEST / OTA / NET PING");
    serialOut().println("GATEWAY CMD: NET STATUS (the gateway's counters)");
    serialOut().println("GATEWAY CMD: OTA url=... token=... (the gateway's own update)");
    serialOut().println("GATEWAY out: NET reply from=<name> ... and NET log from=<name> <line> (node.radio_log)");
}
