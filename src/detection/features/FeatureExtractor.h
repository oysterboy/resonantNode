#pragma once

#include "../../audio/AudioSignal.h"
#include "../inspection/InspectorTypes.h"
#include "FeatureHistory.h"

/*
FeatureExtractor

Small helper namespace that derives feature-history samples from AudioSamplePacket.
It measures feature streams only; it does not emit occurrences or classify patterns.
Producer emits a fresh feature sample or packet, sends it to FeatureHistory, and
sends it to the selected detector in parallel. FeatureHistory is not the live
pipe into detector occurrence state.
*/
namespace detection::FeatureExtractor {

inline void observeFrame(const AudioSamplePacket& audioSamplePacket, FeatureHistory& history) {
    if (!audioSamplePacket.valid) {
        return;
    }

    history.record(FeatureStreamId::AmpEnvelope, audioSamplePacket.timeMs, audioSamplePacket.audioMagnitudeValue);
}

inline void observeFrequencyMeasurementPacket(const FrequencyBandMeasurementPacket& evidence, unsigned long nowMs, FeatureHistory& history) {
    if (!evidence.present) {
        return;
    }

    if (!evidence.fresh) {
        return;
    }

    const unsigned long sampleTimeMs = evidence.observedAtMs != 0 ? evidence.observedAtMs : nowMs;
    // Frequency target, score, and contrast are first-class scalar samples in history.
    // FrequencyBandMeasurementPacket stays the compound packet used by FrequencyMatch.
    history.record(FeatureStreamId::FrequencyTarget, sampleTimeMs, evidence.targetBandScoreValue);
    history.record(FeatureStreamId::FrequencyScore, sampleTimeMs, evidence.targetBandScoreValue);
    history.record(FeatureStreamId::FrequencyTargetBand, sampleTimeMs, evidence.targetBandScoreValue);
    history.record(FeatureStreamId::FrequencyContrast, sampleTimeMs, evidence.targetBandContrastValue);
    // Temporarily disabled to reduce analyzer memory pressure during the current pass.
    // history.record(FeatureStreamId::FrequencyTargetPower, sampleTimeMs, evidence.targetBandPowerValue);
    // history.record(FeatureStreamId::FrequencyNeighborPower, sampleTimeMs, evidence.neighborBandPowerValue);
    // history.record(FeatureStreamId::FrequencyTotalEnergy, sampleTimeMs, evidence.totalEnergyValue);
    // history.record(FeatureStreamId::FrequencyWindowValid, sampleTimeMs, evidence.validWindow ? 1.0f : 0.0f);
}

} // namespace detection::FeatureExtractor
