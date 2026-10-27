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

    history.record(FeatureStreamId::AmpMagnitude, audioSamplePacket.timeMs, audioSamplePacket.audioMagnitudeValue);
    history.record(FeatureStreamId::AmpEnvelope, audioSamplePacket.timeMs, audioSamplePacket.smoothedLevel);
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
    history.record(FeatureStreamId::FrequencyTarget, sampleTimeMs, evidence.targetBandValue);
    history.record(FeatureStreamId::FrequencyContrast, sampleTimeMs, evidence.targetBandContrastValue);
}

} // namespace detection::FeatureExtractor
