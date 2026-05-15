#pragma once

#include <Arduino.h>

#include "../DetectionPipeline.h"

namespace detection {

enum class SignalKind {
    None,
    AmpTransient,
    FrequencyMatch
};

enum class SignalSource {
    None,
    Amp,
    Frequency
};

struct SignalCandidate {
    SignalKind kind = SignalKind::None;
    SignalSource source = SignalSource::None;

    bool present = false;
    bool valid = false;

    uint64_t startSample = 0;
    uint64_t peakSample = 0;
    uint64_t releaseSample = 0;

    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long releaseMs = 0;
    unsigned long durationMs = 0;

    float strength = 0.0f;
    float score = 0.0f;
    float contrast = 0.0f;

    DetectionPipeline::TransientEvidence transient = {};
    DetectionPipeline::FrequencyEvidence frequency = {};
};

} // namespace detection
