#pragma once

#include <Arduino.h>

#include "../DetectionTypes.h"
#include "../inspector/InspectorTypes.h"

namespace detection {

/*
Occurrence

Low-level source-tagged occurrence event proposed by a OccurrenceSource.
It is not a pattern result and must not drive behavior directly.
*/
// Legacy accepted-event kind name retained during Pass B migration.
// Canonical target vocabulary: OccurrenceType.
enum class OccurrenceKind {
    None,
    AmpTransient,
    FrequencyMatch,
    BroadbandTransient
};

// Legacy detector identity name retained during Pass B migration.
// Canonical target vocabulary: DetectorId.
enum class OccurrenceSource {
    None,
    Amp,
    Frequency,
    Broadband
};

// Legacy detector-local subtype tag.
// No canonical shared replacement is chosen yet; keep this detector-facing.
enum class OccurrenceDetectorKind {
    Unknown,
    Transient,
    FrequencyMatch,
    Dip,
    Plateau,
    ThresholdCrossing
};

struct Occurrence {
    OccurrenceKind kind = OccurrenceKind::None;
    OccurrenceSource source = OccurrenceSource::None;

    bool present = false;
    bool valid = false;
    OccurrenceDetectorKind detectorKind = OccurrenceDetectorKind::Unknown;

    uint64_t startSample = 0;
    uint64_t peakSample = 0;
    uint64_t releaseSample = 0;

    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long releaseMs = 0;
    unsigned long endMs = 0;
    unsigned long durationMs = 0;
    unsigned long candidateHoldWindows = 0;

    float strength = 0.0f;
    float score = 0.0f;
    float contrast = 0.0f;
    float confidence = 0.0f;
    float ampLevel = 0.0f;
    float ampBaseline = 0.0f;
    bool ampEvidencePresent = false;
    StrengthClass ampStrength = StrengthClass::Unknown;
    ScalarEvidence scalarEvidence = {};
    StrengthClass frequencyScoreStrength = StrengthClass::Unknown;
    StrengthClass frequencyContrastQuality = StrengthClass::Unknown;
    StrengthClass targetBandStrength = StrengthClass::Unknown;

    TransientEvidence transient = {};
    FrequencyBandMeasurementPacket frequency = {};
};

// Pass B bridge helpers: map legacy occurrence/source names into canonical
// contract vocabulary without changing active runtime behavior.
inline DetectorId detectorIdFromLegacyOccurrenceSource(OccurrenceSource source) {
    switch (source) {
        case OccurrenceSource::Amp:
            return DetectorId::ScalarTransient;
        case OccurrenceSource::Frequency:
            return DetectorId::FrequencyMatch;
        case OccurrenceSource::Broadband:
        case OccurrenceSource::None:
        default:
            return DetectorId::Unknown;
    }
}

inline OccurrenceType occurrenceTypeFromLegacyOccurrenceKind(OccurrenceKind kind) {
    switch (kind) {
        case OccurrenceKind::AmpTransient:
            return OccurrenceType::AmpTransient;
        case OccurrenceKind::FrequencyMatch:
            return OccurrenceType::FrequencyMatch;
        case OccurrenceKind::BroadbandTransient:
            return OccurrenceType::BroadbandTransient;
        case OccurrenceKind::None:
        default:
            return OccurrenceType::None;
    }
}

inline OccurrenceDetailKind occurrenceDetailKindFromLegacyOccurrenceKind(OccurrenceKind kind) {
    switch (kind) {
        case OccurrenceKind::AmpTransient:
            return OccurrenceDetailKind::ScalarTransient;
        case OccurrenceKind::FrequencyMatch:
            return OccurrenceDetailKind::FrequencyBand;
        case OccurrenceKind::BroadbandTransient:
            return OccurrenceDetailKind::BroadbandTransient;
        case OccurrenceKind::None:
        default:
            return OccurrenceDetailKind::None;
    }
}

} // namespace detection

