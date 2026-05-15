#pragma once

#include <Arduino.h>

#include "../patterns/PatternTypes.h"

namespace detection {

enum class SignalKind {
    None,
    AmpTransient,
    FrequencyMatch,
    BroadbandTransient
};

enum class SignalSource {
    None,
    Amp,
    Frequency,
    Broadband
};

enum class SignalDetectorKind {
    Unknown,
    Transient,
    FrequencyMatch,
    Dip,
    Plateau,
    ThresholdCrossing
};

struct SignalCandidate {
    SignalKind kind = SignalKind::None;
    SignalSource source = SignalSource::None;

    bool present = false;
    bool valid = false;
    SignalDetectorKind detectorKind = SignalDetectorKind::Unknown;

    uint64_t startSample = 0;
    uint64_t peakSample = 0;
    uint64_t releaseSample = 0;

    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long releaseMs = 0;
    unsigned long endMs = 0;
    unsigned long durationMs = 0;

    float strength = 0.0f;
    float score = 0.0f;
    float contrast = 0.0f;
    float confidence = 0.0f;
    float signalConfidence = 0.0f;
    float frequencyConfidence = 0.0f;
    float ampLevel = 0.0f;
    float ampBaseline = 0.0f;
    bool ampEvidencePresent = false;
    AmpSupportClass ampSupport = AmpSupportClass::Unknown;
    LocalityClass locality = LocalityClass::Unknown;
    bool duplicateRisk = false;
    float duplicateRiskScore = 0.0f;

    TransientEvidence transient = {};
    FrequencyEvidence frequency = {};
};

} // namespace detection
