#pragma once

#include "../../io/AudioSignal.h"
#include "../patterns/PatternTypes.h"
#include "FeatureHistory.h"

namespace detection::FeatureExtractor {

inline void observeFrame(const AudioSignalFrame& frame, FeatureHistory& history) {
    if (!frame.valid) {
        return;
    }

    history.record(FeatureStreamId::AmpEnvelope, frame.sampleTimeMs, static_cast<float>(frame.level));
    history.record(FeatureStreamId::AmbientFloor, frame.sampleTimeMs, frame.baseline);
}

inline void observeFrequencyEvidence(const FrequencyEvidence& evidence, unsigned long nowMs, FeatureHistory& history) {
    if (!evidence.present) {
        return;
    }

    const unsigned long sampleTimeMs = nowMs != 0 ? nowMs : evidence.observedAtMs;
    history.record(FeatureStreamId::FrequencyScore, sampleTimeMs, evidence.score);
    history.record(FeatureStreamId::FrequencyContrast, sampleTimeMs, evidence.spectralContrast);
}

} // namespace detection::FeatureExtractor
