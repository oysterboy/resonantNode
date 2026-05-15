#pragma once

#include "InspectedSignal.h"

namespace detection {

struct SignalWindowStats {
    unsigned long durationMs = 0;
    float strength = 0.0f;
    float signalConfidence = 0.0f;
    float frequencyConfidence = 0.0f;
    float ampLevel = 0.0f;
    float ampBaseline = 0.0f;
    float ampLift = 0.0f;
    float ampNormalized = 0.0f;
    bool hasAmp = false;
    bool hasFrequency = false;
};

inline SignalWindowStats evaluateSignalWindow(const SignalCandidate& candidate) {
    SignalWindowStats out = {};
    out.durationMs = candidate.durationMs;
    out.strength = candidate.strength;
    out.signalConfidence = candidate.signalConfidence;
    out.frequencyConfidence = candidate.frequencyConfidence;
    out.ampLevel = candidate.ampLevel;
    out.ampBaseline = candidate.ampBaseline;
    out.hasAmp = candidate.ampEvidencePresent;
    out.hasFrequency = candidate.frequency.present;
    out.ampLift = candidate.ampLevel - candidate.ampBaseline;
    out.ampNormalized = candidate.ampBaseline > 0.0f ? out.ampLift / candidate.ampBaseline : out.ampLift;
    return out;
}

inline AmpSupportClass classifyAmpSupport(const SignalWindowStats& stats) {
    if (!stats.hasAmp) {
        return AmpSupportClass::Unknown;
    }

    if (stats.ampLift >= 1200.0f || stats.ampNormalized >= 0.60f) {
        return AmpSupportClass::Strong;
    }

    if (stats.ampLift >= 500.0f || stats.ampNormalized >= 0.25f) {
        return AmpSupportClass::Medium;
    }

    if (stats.ampLift >= 120.0f || stats.ampNormalized > 0.0f) {
        return AmpSupportClass::Weak;
    }

    return AmpSupportClass::None;
}

inline LocalityClass classifyLocality(AmpSupportClass support) {
    switch (support) {
        case AmpSupportClass::Strong:
            return LocalityClass::Near;
        case AmpSupportClass::Medium:
            return LocalityClass::Mid;
        case AmpSupportClass::Weak:
        case AmpSupportClass::None:
            return LocalityClass::Far;
        case AmpSupportClass::Unknown:
        default:
            return LocalityClass::Unknown;
    }
}

} // namespace detection
