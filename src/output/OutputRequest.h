#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../app/RuntimeDefaults.h"

/*
OutputRequest / OutputStatus (myspec 7.2)

The vocabulary between Behavior, the Node and SoundOutput, with no device in
it: Behavior names the kind of emission it wants, the Node composes one
OutputRequest from that and the output.* params, SoundOutput executes it and
keeps the OutputStatus. ChirpOutput and the ToneOutput devices are not
included here on purpose; Behavior must compile without them (myspec 7.1).
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

constexpr size_t kMaxHopSteps = runtime::kMaxHopSteps;
constexpr size_t kMaxTrainPulses = runtime::kMaxTrainPulses;

struct OutputRequest {
    enum class Kind : uint8_t {
        Pattern,   // a ChirpOutput pattern at the chirp tone, timed by ChirpOutput
        HopTrain,  // the network signal: hopSteps pulses on hopHz
        IdleCall,  // announceHz for announceMs, announceGapMs of silence, then the call
        Clip,      // a recorded clip played back as it was (ECHO-001, the echo room's replay)
    };
    Kind kind = Kind::Pattern;
    ChirpPattern pattern = ChirpPattern::Single;
    // HopTrain, and an IdleCall's call (hopSteps 0: the call is the plain chirp). One entry per pulse: a trill's
    // repeats of the base bin, then the climb, so hopSteps counts pulses, not distinct bins.
    uint32_t hopHz[kMaxTrainPulses] = {};
    size_t hopSteps = 0;
    unsigned long pulseMs = 0;
    unsigned long gapMs = 0;
    // The first trillPulses pulses are each followed by trillGapMs instead of gapMs (the trill before the climb).
    size_t trillPulses = 0;
    unsigned long trillGapMs = 0;
    // A burst of high-passed noise at the start of the pulses (output.broadband_*): broadbandPct of the tone's peak,
    // 0 = none; on the trill's pulses only when broadbandTrillOnly; above broadbandLowHz.
    uint32_t broadbandPct = 0;
    bool broadbandTrillOnly = false;
    uint32_t broadbandLowHz = 0;
    // The trill's pulses shaped like a cricket's (output.trill_*, output/TrillShape.h); all 0 = the plain trill.
    uint32_t trillFadeMs = 0;
    uint32_t trillDroopHz = 0;
    uint32_t trillSwellPulses = 0;
    uint32_t trillShapeScope = 0;  // trill::kScope*: the trill's pulses only, or the rise's steps too
    // The trill's pulse lengths (output.trill_pulse_*, trill::Length); both 0 = every pulse plays pulseMs.
    uint32_t trillPulseEndMs = 0;
    uint32_t trillPulseJitterMs = 0;
    // IdleCall.
    uint32_t announceHz = 0;
    uint32_t announceEndHz = 0;  // 0: a steady tone; else the announcement glides from announceHz to it
    unsigned long announceMs = 0;
    unsigned long announceGapMs = 0;
    // Clip: int16 samples at the port's rate, owned by the caller and left untouched until the request ends;
    // clipMs is their length (for the span checks), clipGain the level (1.0 = as recorded).
    const int16_t* clipSamples = nullptr;
    size_t clipCount = 0;
    unsigned long clipMs = 0;
    float clipGain = 1.0f;
};

// What the device can play: the I2S tone runs at 16 kHz (Nyquist 8 kHz). The
// output.* params are bound so that every train and idle call the Node composes
// fits (ResonantNodeParams.cpp), so a reject means a composition bug;
// RB emit rejected ... fallback= on the Node says so if it happens.
struct OutputLimits {
    uint32_t minHz = 100;
    uint32_t maxHz = runtime::kMaxToneHz;
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
    NoClip,
    DeviceRefused,
};

enum class OutputPhase : uint8_t {
    Idle,
    Playing,  // a Pattern or a HopTrain
    Announce,  // IdleCall: the announcement tone
    AnnounceGap,  // IdleCall: silence before the call
    Call,  // IdleCall: the call
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
