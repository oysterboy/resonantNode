#pragma once

namespace detection {

// Core pattern labels and reasons.
enum class PatternType {
    None,
    SinglePulse,
    DuplicateAfterPrimary,
    UnexpectedNoise,
    Invalid,
    Ambiguous,
};

enum class PatternReasonCode {
    None,
    FromFrequencyMatch,
    FromOccurrence,
    DetectorRejected,
    AmbiguousEvidence,
    UnsupportedPattern,
};

// Pattern rejection reasons are kept separate from result kinds.
enum class PatternRejectReason {
    None,
    NoProposal,
    InvalidOccurrence,
    NoFrequencyEvidence,
    FrequencyWindowInvalid,
    FrequencyScoreTooLow,
    FrequencyContrastTooLow,
    FrequencyScoreAndContrastTooLow,
    MissingSupport,
    SupportTooLow,
    DuplicateAfterPrimary,
    UnexpectedTiming,
    UnexpectedNoise,
};

// Result kinds describe the rule-level outcome.
enum class PatternResultKind {
    Unknown,
    Valid,
    Invalid,
    TooDense,
    Rejected,
};

} // namespace detection
