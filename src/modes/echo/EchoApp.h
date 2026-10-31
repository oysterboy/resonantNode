#pragma once

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "../../app/RuntimeDefaults.h"
#include "../../app/SerialLine.h"
#include "../../audio/CaptureRing.h"
#include "../../behavior/ResonantBehavior.h"
#include "../../detection/detectors/scalar/ScalarTransientDetector.h"
#include "../../detection/features/FeatureHistory.h"
#include "../../detection/inspection/OccurrenceInspector.h"
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

The detection chain is myspec's, with the echo room's own plan: per DMA block the envelope's Strength16 level is
recorded into a FeatureHistory (sized for this build: FEATURE_HISTORY_* in platformio.ini) and fed to the
ScalarTransientDetector (the occurrence: onset, release, duration, strength); an occurrence is held until its
inspection window is complete, the OccurrenceInspector runs the one-module plan (AttackRise: how fast the envelope
rose into the onset) and makeVerdict judges it; a valid verdict goes to ResonantBehavior with the echo room's config
(no idle call, a high first-answer chance, a short rest, a wait long enough for the clip's tail to be recorded). When Behavior
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
Does not: decide when to answer (Behavior), judge an occurrence (the inspector and makeVerdict), load or save params (the Node's
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
    void detectBlock(const AudioBlock& block, uint64_t ringIndex, uint32_t blockMs);
    void judgePending(uint32_t nowMs);
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
        // Strength16 levels (the desk 2026-10-31: the quiet room reads 40-100, a click from the Emitter at ~1 m ~2,000)
        float floor = 300.0f;  // the noise: the attack's low point never below it
        float onset = 1000.0f;  // the detector's onset
        float release = 700.0f;  // the detector's release
        uint32_t maxAttackMs = 10;  // a clap rises (10 % -> 90 % of its peak) within this
        uint32_t preMs = 30;  // the clip from the onset back
        uint32_t postMs = 270;  // and forward (clip <= 300 ms)
        float gain = 1.0f;  // the replay's level
        bool enabled = true;
    };
    struct EchoCounts {
        unsigned long occurrences = 0;
        unsigned long claps = 0;  // valid verdicts
        unsigned long slow = 0;  // rejected: the attack too slow or not measured
        unsigned long replays = 0;
        unsigned long clipMissing = 0;  // the clip no longer (or not yet) whole in the ring
    };
    Gate _gate;
    EchoCounts _echo;
    detection::FeatureHistory _history;
    detection::OccurrenceInspector _inspector;
    detection::InspectionPlan _plan;
    ScalarTransientDetector _detector;
    detection::Occurrence _pending;  // held until its inspection window is complete
    bool _havePending = false;
    uint64_t _lastBlockIndex = 0;  // the newest block's first sample in the ring, and its time: ms -> ring index
    uint32_t _lastBlockMs = 0;
    ResonantBehavior _behavior;
    ChirpOutput _chirpOutput{_toneOutput};
    SoundOutput _soundOutput{_chirpOutput};
    std::unique_ptr<int16_t[]> _clip;
    size_t _clipCapacity = 0;
    // The clap Behavior is answering: copied out of the ring as soon as its tail is recorded (the answer can come
    // later than the ring holds its start: wait plus jitter reach ~700 ms, the ring 500).
    uint64_t _clipOnset = 0;
    size_t _clipCount = 0;
    bool _clipWanted = false;
    bool _clipReady = false;
    void takeClip();
    bool _detectionPaused = false;
    int32_t _envelope = 0;  // smoothed |sample|, canonical PCM units (24-bit)
    int32_t _envelopePeak = 0;  // since the last status

    Load _load;
    unsigned long _toneOffAtMs = 0;
    bool _toneOn = false;
    unsigned long _logEveryMs = 10000;
    unsigned long _nextLogMs = 0;
};
