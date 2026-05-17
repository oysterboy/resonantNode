#pragma once

#include <stdint.h>

namespace detection {

enum class PatternType {
    None,
    ValidTransient,
    ValidTonalTransient,
    TransientOnly,
    FrequencyWeak,
    DuplicateAfterPrimary,
    UnexpectedNoise,
    Invalid,
    Ambiguous,
};

enum class PatternReasonCode {
    None,
    FromAcceptedTransient,
    DetectorRejected,
    AmbiguousEvidence,
    UnsupportedPattern,
};

enum class PatternSource {
    ComparisonOnly,
    AmpFallback,
    FrequencyPrimary,
};

enum class AmpSupportClass {
    Unknown,
    None,
    Weak,
    Medium,
    Strong,
};

enum class LocalityClass {
    Unknown,
    Near,
    Mid,
    Far,
};

// Observation-only retrospective AMP window evidence for a candidate.
// This stays lightweight and is carried through PatternResult for reporting.
struct AmpWindowEvidence {
    bool available = false;
    bool observedOnly = true;

    int16_t windowStartMs = -20;
    int16_t windowEndMs = 120;

    float peak = 0.0f;
    float baseline = 0.0f;
    float lift = 0.0f;
    float norm = 0.0f;

    AmpSupportClass supportClass = AmpSupportClass::Unknown;
    LocalityClass localityClass = LocalityClass::Unknown;
};

enum class PatternRejectReason {
    None,
    NoCandidate,
    NoFrequencyEvidence,
    FrequencyWindowInvalid,
    FrequencyScoreTooLow,
    FrequencyContrastTooLow,
    FrequencyScoreAndContrastTooLow,
    TransientOnly,
    DuplicateAfterPrimary,
    UnexpectedTiming,
    UnexpectedNoise,
};

enum class PatternCandidateKind {
    Unknown,
    SinglePulse,
    PulseSequence,
    NoiseBurst,
    ObjectHit,
};

enum class PatternResultKind {
    Unknown,
    TonalPulse,
    TonalPulseNear,
    TonalPulseMid,
    TonalPulseFar,
    ValidChirp,
    InvalidChirp,
    TooDense,
    Residual,
    Rejected,
};

// Raw detector evidence captured for transient-trigger analysis.
// This remains a compatibility payload for legacy and analyzer paths.
struct TransientEvidence {
    bool present = false;

    uint64_t onsetSample = 0;
    uint64_t peakSample = 0;
    uint64_t releaseSample = 0;

    unsigned long startMs = 0;
    unsigned long heardAtMs = 0;
    unsigned long acceptedMs = 0;
    unsigned long durationMs = 0;

    float onsetStrength = 0.0f;
    float peakStrength = 0.0f;
    float releaseStrength = 0.0f;
    float ambientBaseline = 0.0f;

    bool audioOverflowDuringCandidate = false;
};

// Frequency evidence carried alongside a candidate for tonal classification.
// Roadmap code prefers the dedicated signal / pattern layers, but this
// container stays here as the transitional compatibility payload.
struct FrequencyEvidence {
    bool present = false;
    bool matched = false;

    unsigned long targetHz = 0;
    unsigned long observedAtMs = 0;
    uint64_t windowStartSample = 0;
    uint64_t windowEndSample = 0;
    unsigned long windowSampleCount = 0;
    bool windowAvailable = false;

    float score = 0.0f;
    float confidence = 0.0f;

    float targetPower = 0.0f;
    float neighborPower = 0.0f;
    float totalEnergy = 0.0f;
    float spectralContrast = 0.0f;
    bool validWindow = false;
};

} // namespace detection
