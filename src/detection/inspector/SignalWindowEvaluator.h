#pragma once

#include "InspectorTypes.h"
#include "../signals/InspectedSignal.h"

namespace detection {

struct SignalWindowStats {
    unsigned long durationMs = 0;
    float strength = 0.0f;
    float signalConfidence = 0.0f;
    float frequencyConfidence = 0.0f;
    float ampLevel = 0.0f;
    float ampBaseline = 0.0f;
    float ampLift = 0.0f;
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
    return out;
}

inline AmpSupportClass classifyAmpSupport(const SignalWindowStats& stats) {
    if (!stats.hasAmp) {
        return AmpSupportClass::Unknown;
    }

    const AmpSupportConfig config{};
    if (stats.ampLevel >= config.strongPeakThreshold) {
        return AmpSupportClass::Strong;
    }

    if (stats.ampLevel >= config.mediumPeakThreshold) {
        return AmpSupportClass::Medium;
    }

    if (stats.ampLevel >= config.weakPeakThreshold) {
        return AmpSupportClass::Weak;
    }

    return AmpSupportClass::None;
}

} // namespace detection
