#pragma once

namespace detection {

// Core pattern labels and reasons.
enum class VerdictType {
    None,
    SinglePulse,
    DuplicateAfterPrimary,
    UnexpectedNoise,
    Invalid,
    Ambiguous,
};

enum class VerdictReasonCode {
    None,
    FromFrequencyMatch,
    FromOccurrence,
    DetectorRejected,
    AmbiguousEvidence,
    SupportRequirementFailed,
};

// Pattern rejection reasons are kept separate from result kinds.
enum class VerdictRejectReason {
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

} // namespace detection
