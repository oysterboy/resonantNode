#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ChirpOutput.h"

/*
SoundOutput (OUT-002/003, step 10f; myspec 7.2)

Executes one OutputRequest at a time on the ChirpOutput and keeps an
OutputStatus. It owns the sequencing of a request: a hop train, a chirp
pattern, or the idle call (announcement tone, gap, then the node's call). It
checks a request against the output's physical limits before playing it.

Does NOT decide what to play or when: Behavior decides (shouldStartChirp(),
chirpPattern(), signalShape()) and the Node composes the concrete request
from that decision and the output.* params (the hop grid comes from the
detection band, which Behavior does not know). Moved here from the Node glue
(startEmission / updateEmissions) unchanged in timing.
*/

constexpr size_t kMaxHopSteps = 8;

struct OutputRequest {
    enum class Kind : uint8_t {
        Pattern,   // a ChirpOutput pattern at the chirp tone, timed by ChirpOutput
        HopTrain,  // the network signal: hopSteps pulses on hopHz
        IdleCall,  // announceHz for announceMs, announceGapMs of silence, then the call
    };
    Kind kind = Kind::Pattern;
    ChirpOutput::ChirpPattern pattern = ChirpOutput::ChirpPattern::Single;
    // HopTrain, and an IdleCall's call (hopSteps 0: the call is the plain chirp).
    uint32_t hopHz[kMaxHopSteps] = {};
    size_t hopSteps = 0;
    unsigned long pulseMs = 0;
    unsigned long gapMs = 0;
    // IdleCall.
    uint32_t announceHz = 0;
    unsigned long announceMs = 0;
    unsigned long announceGapMs = 0;
};

// What the device can play: the I2S tone runs at 16 kHz (Nyquist 8 kHz), the
// output.* params are bound inside these, so a reject means a composition bug.
struct OutputLimits {
    uint32_t minHz = 100;
    uint32_t maxHz = 7900;
    unsigned long maxPulseMs = 1000;
    unsigned long maxGapMs = 2000;
    unsigned long minAnnounceMs = 20;
    unsigned long maxAnnounceMs = 2000;
    unsigned long maxSpanMs = 6000;
};

enum class OutputReject : uint8_t {
    None,
    Busy,
    NoSteps,
    FrequencyOutOfRange,
    PulseOutOfRange,
    GapOutOfRange,
    AnnounceOutOfRange,
    SpanTooLong,
    DeviceRefused,
};

enum class OutputPhase : uint8_t {
    Idle,
    Playing,      // a Pattern or a HopTrain
    Announce,     // IdleCall: the announcement tone
    AnnounceGap,  // IdleCall: silence before the call
    Call,         // IdleCall: the call
};

struct OutputStatus {
    OutputPhase phase = OutputPhase::Idle;
    OutputRequest::Kind kind = OutputRequest::Kind::Pattern;
    unsigned long lastStartMs = 0;
    unsigned long lastDoneMs = 0;
    unsigned long spanMs = 0;  // planned span of the current or last request (0: a Pattern)
    uint32_t started = 0;
    uint32_t done = 0;
    uint32_t rejected = 0;
    OutputReject lastReject = OutputReject::None;

    bool busy() const { return phase != OutputPhase::Idle; }
};

const char* outputRejectName(OutputReject reject);
const char* outputPhaseName(OutputPhase phase);

class SoundOutput {
public:
    // update() events, as bits.
    static constexpr uint8_t kEventCallStarted = 1;  // an IdleCall's call started (hop or plain chirp)
    static constexpr uint8_t kEventFinished = 2;     // the request ended (finishedAtMs())

    explicit SoundOutput(ChirpOutput& chirp, const OutputLimits& limits = OutputLimits());

    static OutputReject validate(const OutputRequest& request, const OutputLimits& limits);
    // The request's planned span: announcement + gap + call for an IdleCall
    // (plainCallMs when its call is the plain chirp), the train for a
    // HopTrain, 0 for a Pattern (ChirpOutput times it).
    static unsigned long plannedSpanMs(const OutputRequest& request, unsigned long plainCallMs);

    // Starts the request. False when rejected (status().lastReject says why);
    // nothing sounds then.
    bool start(const OutputRequest& request);
    // Advances the request once per loop; returns kEvent* bits.
    uint8_t update();
    // Ends whatever plays, also an IdleCall waiting in its gap. No event.
    void stop();

    bool busy() const { return _status.busy(); }
    unsigned long finishedAtMs() const { return _status.lastDoneMs; }
    const OutputStatus& status() const { return _status; }
    const OutputRequest& current() const { return _request; }
    // The hop train the call played or will play (for the log line).
    bool callIsHop() const { return _request.hopSteps > 0; }

private:
    bool startHop(const OutputRequest& request);
    bool startCall();
    void reject(OutputReject reason);
    void finish();

    ChirpOutput& _chirp;
    OutputLimits _limits;
    OutputRequest _request;
    OutputStatus _status;
    unsigned long _callAtMs = 0;
};
