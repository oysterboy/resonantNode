#pragma once

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "../../app/RuntimeDefaults.h"
#include "../../app/SerialLine.h"
#include "../../audio/CaptureRing.h"
#include "../../behavior/ResonantBehavior.h"
#include "../../detection/detectors/scalar/ScalarTransientDetector.h"
#include "../../detection/features/AttackSlope.h"
#include "../../hal/AudioSourceI2S.h"
#include "../../hal/I2sToneOutput.h"
#include "../../net/NetGateway.h"
#include "../../net/NodeNet.h"
#include "../../output/ChirpOutput.h"
#include "../../output/SoundOutput.h"

#ifndef ECHO_RING_MS
#define ECHO_RING_MS 500
#endif

/*
EchoApp (ECHO-003, step 20.1; the echo room ECHO-001, step 20.2): the echo modes' shell on the Node hardware, and
the echo room in it: a clap (a fast-rising transient) is answered by a replay of the sound itself, cut from the
capture ring around its onset.

Per DMA block: the envelope's Strength16 level into AttackSlope (how fast it rose) and ScalarTransientDetector (the
occurrence: onset, release, duration, strength), both on the envelope alone; an accepted occurrence becomes a verdict
by makeClapVerdict (valid when the attack was fast) and goes to ResonantBehavior with the echo room's config (no idle
call, a high first-answer chance, a short rest, a wait long enough for the clip's tail to be recorded). When Behavior
asks to emit, the clip (preMs before the onset to postMs after it) is copied out of the ring and played through
SoundOutput as an OutputRequest::Kind::Clip; detection pauses while it sounds and for the tail after it.

Runs the per-sample path both modes will run at their sample rate (48 kHz by default in esp32dev-echo): the mic
block, an envelope (rectify and a one-pole smoother; the source already removed DC) and an int16 capture ring of
ECHO_RING_MS, with the radio up and the serial console live. What it measures is the go / no-go of
docs/roadmaps/roadmap-echo.md "Plan to 2026-12-13", condition 3: dropped DMA buffers, the loop's pass time and the
heap left with the ring allocated. TONE plays a test tone through the shared port, so the writer task's load is in
the numbers too.

Owns: the I2S port and the tone writer, the capture ring and the clip buffer, the echo room's gate values and
counters, the load counters, the radio's receive side (OTA, NET PING and ECHO by radio, replies through NetGateway).
Does not: decide when to answer (Behavior), judge an occurrence (makeClapVerdict), load or save params (the Node's
NVS store stays untouched, so a board that passes through this image comes back with its Node saves), act as a NET
gateway. Whispernet (ECHO-002) is not in it yet.
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
    void handleGateCommand(const char* line);
    void detectBlock(const AudioBlock& block, uint32_t blockMs);
    void runBehavior(unsigned long now);
    void startReplay(unsigned long now);
    void configureEcho();
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

    CaptureRing _ring;

    // The echo room (ECHO-001): gate values (ECHO GATE sets them live; params come with ECHO-003's ids).
    struct Gate {
        float floor = 7000.0f;     // Strength16: quiet below this
        float onset = 12000.0f;    // Strength16: the detector's onset and AttackSlope's crossing
        float release = 9000.0f;   // Strength16: the detector's release
        uint32_t maxAttackMs = 10; // a clap rises within this
        uint32_t preMs = 30;       // the clip from the onset back
        uint32_t postMs = 270;     // and forward (clip <= 300 ms)
        float gain = 1.0f;         // the replay's level
        bool enabled = true;
    };
    struct EchoCounts {
        unsigned long occurrences = 0;
        unsigned long claps = 0;        // valid verdicts
        unsigned long slow = 0;         // rejected: the attack too slow or unmeasured
        unsigned long replays = 0;
        unsigned long clipMissing = 0;  // the clip no longer (or not yet) whole in the ring
    };
    Gate _gate;
    EchoCounts _echo;
    detection::AttackSlope _attack;
    ScalarTransientDetector _detector;
    ResonantBehavior _behavior;
    ChirpOutput _chirpOutput{_toneOutput};
    SoundOutput _soundOutput{_chirpOutput};
    std::unique_ptr<int16_t[]> _clip;
    size_t _clipCapacity = 0;
    uint64_t _crossingIndex = 0;  // the sample index of the last onset crossing
    long _crossingAttackMs = -1;
    uint64_t _heardOnsetIndex = 0;  // the onset of the clap Behavior is answering
    bool _detectionPaused = false;
    int32_t _envelope = 0;   // smoothed |sample|, canonical PCM units (24-bit)
    int32_t _envelopePeak = 0;  // since the last status

    Load _load;
    unsigned long _toneOffAtMs = 0;
    bool _toneOn = false;
    unsigned long _logEveryMs = 10000;
    unsigned long _nextLogMs = 0;
};
