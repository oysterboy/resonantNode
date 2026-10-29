#pragma once

#include <Arduino.h>

#include "../../audio/AudioPcm.h"

namespace detection {

/*
FeatureStream

Shared identifiers and values for measured occurrence features.
Feature streams are measurements, not occurrences and not pattern meanings.
*/
enum class FeatureStreamId {
    Unknown,
    AmpMagnitude,
    AmpEnvelope,
    FrequencyTarget,
    FrequencyContrast,
};

inline bool streamRequiresFreshFrequency(FeatureStreamId stream) {
    return stream == FeatureStreamId::FrequencyTarget || stream == FeatureStreamId::FrequencyContrast;
}

inline const char* featureStreamName(FeatureStreamId value) {
    switch (value) {
        case FeatureStreamId::AmpEnvelope:
            return "amp_envelope";
        case FeatureStreamId::AmpMagnitude:
            return "amp_magnitude";
        case FeatureStreamId::FrequencyTarget:
            return "frequency_target";
        case FeatureStreamId::FrequencyContrast:
            return "frequency_contrast";
        case FeatureStreamId::Unknown:
        default:
            return "unknown";
    }
}

// The frequency band measurement of one frame (FreqBandStream, through buildFrequencyMeasurementPacket):
// the runtime's frequency feature input, recorded and reported; the inspector does not read it.
// Decision fields belong to the detector / gate result, not the measurement packet.
struct FrequencyBandMeasurementPacket {
    bool present = false;
    bool fresh = false;

    unsigned long observedAtMs = 0;

    // Frequency target value on the shared 0..32767 magnitude-like scale.
    audio::FrequencyScore16 targetBandValue = 0;

    // Raw Goertzel power retained for diagnostics and debugging.
    float targetBandPowerValue = 0.0f;
    float neighborBandPowerValue = 0.0f;
    float lowerBandPowerValue = 0.0f;
    float upperBandPowerValue = 0.0f;
    float totalEnergyValue = 0.0f;
    float targetBandContrastValue = 0.0f;

    // Band mode: the centre of the strongest band bin at the last update, 0 outside band mode (DET-015, the
    // ShapeInspector's bin track).
    unsigned long peakBinHz = 0;
    // Band mode: the band's bin count (0 outside band mode); BinShape's wrap rule (review 12.0 L3).
    unsigned long bandBins = 0;
};

} // namespace detection
