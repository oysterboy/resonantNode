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
}

inline void observeFrequencyFeatureFrame(const FrequencyFeatureFrame& evidence, unsigned long nowMs, FeatureHistory& history) {
    if (!evidence.evidencePresent) {
        return;
    }

    if (!evidence.updatedThisFrame) {
        return;
    }

    const unsigned long sampleTimeMs = evidence.observedAtMs != 0 ? evidence.observedAtMs : nowMs;
    history.record(FeatureStreamId::FrequencyScore, sampleTimeMs, evidence.score);
    history.record(FeatureStreamId::FrequencyContrast, sampleTimeMs, evidence.spectralContrast);
    // Temporarily disabled to reduce analyzer memory pressure during the current pass.
    // history.record(FeatureStreamId::FrequencyTargetPower, sampleTimeMs, evidence.targetPower);
    // history.record(FeatureStreamId::FrequencyNeighborPower, sampleTimeMs, evidence.neighborPower);
    // history.record(FeatureStreamId::FrequencyTotalEnergy, sampleTimeMs, evidence.totalEnergy);
    // history.record(FeatureStreamId::FrequencyWindowValid, sampleTimeMs, evidence.validWindow ? 1.0f : 0.0f);
}

} // namespace detection::FeatureExtractor
