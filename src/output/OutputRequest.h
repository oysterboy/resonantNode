#pragma once

#include <stddef.h>
#include <stdint.h>

/*
OutputRequest / OutputStatus (myspec 7.2)

The vocabulary between Behavior, the Node and SoundOutput, with no device in
it: Behavior names the kind of emission it wants, the Node composes one
OutputRequest from that and the output.* params, SoundOutput executes it and
keeps the OutputStatus. ChirpOutput and the ToneOutput devices are not
included here on purpose; Behavior must compile without them (myspec 7.1).
Moved out of SoundOutput.h / ChirpOutput.h in step 11e item 1.
*/

// The ChirpOutput patterns Behavior can ask for (Idle: the two-part idle
// voice, kept for the IdleCall's plain-chirp fallback).
enum class ChirpPattern : uint8_t {
    Single,
    Triple,
    Idle,
};

inline const char* chirpPatternName(ChirpPattern pattern) {
    switch (pattern) {
        case ChirpPattern::Single:
            return "single";
        case ChirpPattern::Triple:
            return "triple";
        case ChirpPattern::Idle:
            return "idle";
    }
    return "unknown";
}

constexpr size_t kMaxHopSteps = 8;

struct OutputRequest {
    enum class Kind : uint8_t {
        Pattern,   // a ChirpOutput pattern at the chirp tone, timed by ChirpOutput
        HopTrain,  // the network signal: hopSteps pulses on hopHz
        IdleCall,  // announceHz for announceMs, announceGapMs of silence, then the call
    };
    Kind kind = Kind::Pattern;
    ChirpPattern pattern = ChirpPattern::Single;
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
