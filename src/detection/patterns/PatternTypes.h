#pragma once

namespace detection {

// Core pattern labels and reasons.
enum class PatternType {
    None,
    ValidPattern,
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

// Pattern rejection reasons are kept separate from result kinds.
enum class PatternRejectReason {
    None,
    NoCandidate,
    NoFrequencyEvidence,
    FrequencyWindowInvalid,
    FrequencyScoreTooLow,
    FrequencyContrastTooLow,
    FrequencyScoreAndContrastTooLow,
    MissingSupport,
    SupportTooLow,
    TransientOnly,
    DuplicateAfterPrimary,
    UnexpectedTiming,
    UnexpectedNoise,
};

// Candidate kinds describe the pre-rule shape of the candidate.
enum class PatternCandidateKind {
    Unknown,
    SinglePulse,
    PulseSequence,
    NoiseBurst,
    ObjectHit,
};

// Result kinds describe the rule-level outcome.
enum class PatternResultKind {
    Unknown,
    Pattern,
    ValidChirp,
    InvalidChirp,
    TooDense,
    Rejected,
};

} // namespace detection
