#pragma once

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "../../app/RuntimeDefaults.h"
#include "../../app/SerialLine.h"
#include "../../hal/AudioSourceI2S.h"
#include "../../hal/I2sToneOutput.h"
#include "../../net/NetGateway.h"
#include "../../net/NodeNet.h"

#ifndef ECHO_RING_MS
#define ECHO_RING_MS 500
#endif

/*
EchoApp (ECHO-003, step 20.1): the echo modes' shell on the Node hardware, before either mode exists.

Runs the per-sample path both modes will run at their sample rate (48 kHz by default in esp32dev-echo): the mic
block, an envelope (rectify and a one-pole smoother; the source already removed DC) and an int16 capture ring of
ECHO_RING_MS, with the radio up and the serial console live. What it measures is the go / no-go of
docs/roadmaps/roadmap-echo.md "Plan to 2026-12-13", condition 3: dropped DMA buffers, the loop's pass time and the
heap left with the ring allocated. TONE plays a test tone through the shared port, so the writer task's load is in
the numbers too.

Owns: the I2S port and the tone writer, the capture ring, the load counters, the radio's receive side (OTA,
NET PING and ECHO STATUS by radio, replies through NetGateway).
Does not: detect, decide or replay (ECHO-001 / ECHO-002 add that), load or save params (the Node's NVS store stays
untouched, so a board that passes through this image comes back with its Node saves), act as a NET gateway.
*/
class EchoApp {
public:
    EchoApp();

    void begin();
    void update();

private:
    struct Load {
        uint32_t passes = 0;
        uint32_t passUsMax = 0;    // one loop pass, without the delay(1) between passes
        uint64_t passUsSum = 0;
        uint32_t blockUsMax = 0;   // the per-sample path for one block
        uint64_t samples = 0;
        uint32_t droppedAtStart = 0;
        unsigned long startMs = 0;
    };

    void pollAudio();
    void pollUsb();
    void pollNet();
    void handleLine(const char* line);
    void handleToneCommand(const char* line);
    void handleOtaCommand(const char* line);
    void handleLogCommand(const char* line);
    void printStatus();
    void printNetStatus();
    void printHelp();
    void resetLoad();

    AudioSourceI2S _i2sPort;
    I2sToneOutput _toneOutput;
    NodeNet _net;
    NetGateway _netGateway{_net};
    serial_line::LineReader<serial_line::kCommandLineMax> _usbLine;
    const char* _nodeName = "unknown";

    std::unique_ptr<int16_t[]> _ring;
    size_t _ringSamples = 0;
    size_t _ringHead = 0;
    int32_t _envelope = 0;   // smoothed |sample|, canonical PCM units (24-bit)
    int32_t _envelopePeak = 0;  // since the last status

    Load _load;
    unsigned long _toneOffAtMs = 0;
    bool _toneOn = false;
    unsigned long _logEveryMs = 10000;
    unsigned long _nextLogMs = 0;
};
