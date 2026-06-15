#pragma once

#include "../../audio/AudioSignal.h"
#include "FreqBandStream.h"
#include "../inspection/InspectorTypes.h"

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
    evidence.targetBandScoreValue = freqBandStream.lastTargetBandPowerValue();
    evidence.confidence = 0.0f;
    evidence.targetBandPowerValue = freqBandStream.lastTargetBandPowerValue();
    evidence.neighborBandPowerValue = freqBandStream.lastNeighborBandPowerValue();
    evidence.lowerBandPowerValue = freqBandStream.lastLowerBandPowerValue();
    evidence.upperBandPowerValue = freqBandStream.lastUpperBandPowerValue();
    evidence.lowerBandScoreValue = freqBandStream.lastLowerBandScoreValue();
    evidence.upperBandScoreValue = freqBandStream.lastUpperBandScoreValue();
    evidence.neighborBandPowerMaxValue = freqBandStream.lastNeighborBandPowerMaxValue();
    evidence.totalEnergyValue = freqBandStream.lastTotalEnergyValue();
    evidence.targetBandContrastValue = freqBandStream.lastTargetBandContrastValue();
    return evidence;
}

} // namespace detection
