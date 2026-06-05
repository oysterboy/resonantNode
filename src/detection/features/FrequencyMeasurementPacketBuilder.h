#pragma once

#include "../../io/AudioSignal.h"
#include "FreqBandStream.h"
#include "../inspector/InspectorTypes.h"

namespace detection {

inline FrequencyBandMeasurementPacket buildFrequencyMeasurementPacket(
    const FreqBandStream& freqBandStream,
    const AudioSamplePacket& audioSamplePacket
) {
    FrequencyBandMeasurementPacket evidence = {};
    const bool present = freqBandStream.windowReady();

    evidence.present = present;
    evidence.matched = false;
    evidence.fresh = freqBandStream.producedFreshPacketOnLastObserve();
    evidence.targetHz = present ? freqBandStream.targetFrequencyHz() : 0;
    evidence.observedAtMs = audioSamplePacket.timeMs;
    evidence.ageSamples = freqBandStream.lastPacketAgeSamples();
    evidence.targetBandScoreValue = freqBandStream.lastTargetBandScoreValue();
    evidence.confidence = 0.0f;
    evidence.targetBandPowerValue = freqBandStream.lastTargetBandPowerValue();
    evidence.neighborBandPowerValue = freqBandStream.lastNeighborBandPowerValue();
    evidence.totalEnergyValue = freqBandStream.lastTotalEnergyValue();
    evidence.targetBandContrastValue = freqBandStream.lastTargetBandContrastValue();
    return evidence;
}

} // namespace detection
