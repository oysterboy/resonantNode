#pragma once

#include "../../io/AudioSignal.h"
#include "../inspector/InspectorTypes.h"
#include "FeatureHistory.h"

/*
FeatureExtractor

Small helper namespace that derives feature-history samples from AudioSignalFrame.
It measures feature streams only; it does not emit candidates or classify patterns.
*/
namespace detection::FeatureExtractor {

inline void observeFrame(const AudioSignalFrame& frame, FeatureHistory& history) {
    if (!frame.valid) {
        return;
    }

    history.record(FeatureStreamId::AmpEnvelope, frame.sampleTimeMs, frame.centeredMagnitude);
    history.record(FeatureStreamId::AmbientFloor, frame.sampleTimeMs, frame.baseline);
}

inline void observeFrequencyFeatureFrame(const FrequencyFeatureFrame& evidence, unsigned long nowMs, FeatureHistory& history) {
    if (!evidence.present) {
        return;
    }

    const unsigned long sampleTimeMs = nowMs != 0 ? nowMs : evidence.observedAtMs;
    history.record(FeatureStreamId::FrequencyScore, sampleTimeMs, evidence.score);
    history.record(FeatureStreamId::FrequencyContrast, sampleTimeMs, evidence.spectralContrast);
    // Temporarily disabled to reduce analyzer memory pressure during the current pass.
    // history.record(FeatureStreamId::FrequencyTargetPower, sampleTimeMs, evidence.targetPower);
    // history.record(FeatureStreamId::FrequencyNeighborPower, sampleTimeMs, evidence.neighborPower);
    // history.record(FeatureStreamId::FrequencyTotalEnergy, sampleTimeMs, evidence.totalEnergy);
    // history.record(FeatureStreamId::FrequencyWindowValid, sampleTimeMs, evidence.validWindow ? 1.0f : 0.0f);
}

} // namespace detection::FeatureExtractor
