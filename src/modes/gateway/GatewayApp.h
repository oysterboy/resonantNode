#pragma once

#include <stdint.h>

#include "../../app/SerialLine.h"
#include "../../net/NetGateway.h"
#include "../../net/NodeNet.h"

/*
GatewayApp (2026-11-05, decisions/2026-11-05-radio-gateway-board.md)

A bare ESP32 on a USB cable as the bench's radio relay: no mic, no amp, no audio, nothing played. It sends the
`NET <target|*> <command>` lines typed on its console to the boards (Node, echo, whisper) and prints what comes
back: replies as `NET reply from=E1 ...` and the boards' mirrored console lines (node.radio_log) as `NET log from=E1
...`. So every board can play, in any mode, while one cheap board holds the data connection.
Owns: the console, the radio link (NodeNet) and the printing of what it hears (NetGateway).
Does not: run other boards' commands, hear, play, detect or decide anything; keep params (it has none).
Build: esp32dev-gateway, -D GATEWAY_MODE; its name comes from the roster by MAC (E0).
*/
class GatewayApp {
public:
    void begin();
    void update();

private:
    void pollUsb();
    void pollNet();
    void handleLine(const char* line);
    void handleNetCommand(const char* line);
    void handleOtaCommand(const char* line);
    void printNetStatus();
    void printHelp();

    NodeNet _net;
    NetGateway _gateway{_net};
    const char* _nodeName = "unknown";
    serial_line::LineReader<serial_line::kCommandLineMax> _usbLine;
};
