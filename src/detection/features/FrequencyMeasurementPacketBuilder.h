#pragma once

#include "../../audio/AudioSignal.h"
#include "FreqBandStream.h"
#include "FeatureStream.h"

namespace detection {

inline FrequencyBandMeasurementPacket buildFrequencyMeasurementPacket(
    const FreqBandStream& freqBandStream,
    const AudioSamplePacket& audioSamplePacket
) {
    FrequencyBandMeasurementPacket evidence = {};
    const bool present = freqBandStream.windowReady();

    evidence.present = present;
    evidence.fresh = freqBandStream.producedFreshPacketOnLastObserve();
    evidence.observedAtMs = audioSamplePacket.timeMs;
    evidence.targetBandValue = freqBandStream.lastTargetBandValue();
    evidence.targetBandPowerValue = freqBandStream.lastTargetBandPowerValue();
    evidence.neighborBandPowerValue = freqBandStream.lastNeighborBandPowerValue();
    evidence.lowerBandPowerValue = freqBandStream.lastLowerBandPowerValue();
    evidence.upperBandPowerValue = freqBandStream.lastUpperBandPowerValue();
    evidence.totalEnergyValue = freqBandStream.lastTotalEnergyValue();
    evidence.targetBandContrastValue = freqBandStream.lastTargetBandContrastValue();
    evidence.peakBinHz = present ? freqBandStream.lastPeakBinHz() : 0;
    return evidence;
}

} // namespace detection
