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

// Canonical scalar accepted-event detail.
//
// This shape is intentionally carrier-agnostic: the current scalar carrier may
// be AMP envelope, frequency score, frequency contrast, or another scalar
// stream. Do not rename this back to an AMP-specific public detail type.
struct ScalarOccurrenceDetail {
    bool present = false;
    float value = 0.0f;
    float baseline = 0.0f;
    float lift = 0.0f;
    float strength = 0.0f;
};

struct Occurrence {
    // Canonical generic accepted-event shell.
    DetectorId detectorId = DetectorId::Unknown;
    OccurrenceType occurrenceType = OccurrenceType::None;
    bool present = false;
    bool valid = false;

    uint64_t startSample = 0;
    uint64_t peakSample = 0;
    uint64_t releaseSample = 0;

    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long releaseMs = 0;
    unsigned long endMs = 0;
    unsigned long durationMs = 0;

    float strength = 0.0f;
    float confidence = 0.0f;

    // Canonical scalar accepted-event detail. This is the first compact
    // reusable detail shape for scalar-transient output.
    ScalarOccurrenceDetail scalar = {};

    // Everything below this line is transitional compatibility payload.
    // The FrequencyMatch migration should follow the canonical shell plus
    // scoped detail-block model above, not extend this legacy spillover area.

    // LEGACY_OCCURRENCE_IDENTITY_COMPAT:
    // Retained until active runtime/analyzer code stops reading the older
    // occurrence identity names directly.
    OccurrenceKind kind = OccurrenceKind::None;
    OccurrenceSource source = OccurrenceSource::None;
    OccurrenceDetectorKind detectorKind = OccurrenceDetectorKind::Unknown;

    // LEGACY_ACCEPTED_EVENT_PAYLOAD_COMPAT:
    // Retained while PatternAssembler and analyzer-side accepted-event readers
    // still read the older wide occurrence payload directly.
    unsigned long candidateHoldWindows = 0;

    // LEGACY_SCALAR_FREQUENCY_VALUE_COMPAT:
    // Transitional detector-specific accepted-event measurements that have not
    // yet been pulled behind compact canonical detail blocks.
    float score = 0.0f;
    float contrast = 0.0f;
    StrengthClass ampStrength = StrengthClass::Unknown;
    ScalarEvidence scalarEvidence = {};
    StrengthClass frequencyScoreStrength = StrengthClass::Unknown;
    StrengthClass frequencyContrastQuality = StrengthClass::Unknown;
    StrengthClass targetBandStrength = StrengthClass::Unknown;

    // TEMP_SCALAR_AMP_COMPAT:
    // Transitional AMP-era public names preserved only for compile/runtime
    // compatibility while the pipeline migrates to the neutral scalar detail
    // above.
    bool ampEvidencePresent = false;
    float ampLevel = 0.0f;
    float ampBaseline = 0.0f;

    // LEGACY_TYPED_OCCURRENCE_PAYLOAD_COMPAT:
    // These payload blocks are still read directly by current pattern and
    // inspection code. Keep them until later trimming passes move those
    // consumers onto the compact generic shell plus scoped detail blocks.
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
        case OccurrenceKind::BroadbandTransient:
            return OccurrenceType::Transient;
        case OccurrenceKind::FrequencyMatch:
            return OccurrenceType::FrequencyMatch;
        case OccurrenceKind::None:
        default:
            return OccurrenceType::None;
    }
}

} // namespace detection

