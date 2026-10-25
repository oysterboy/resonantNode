#pragma once

namespace detection {

// The verdict's vocabulary, only what makeVerdict() produces (step 11e item 3;
// DET-018 7b). A pattern stage over several occurrences, if it comes, adds its
// own values downstream (myspec 5.7).
enum class VerdictType {
    None,
    SinglePulse,
};

enum class VerdictReasonCode {
    None,
    FromOccurrence,
    SupportRequirementFailed,
};

enum class VerdictRejectReason {
    None,
    MissingSupport,
    SupportTooLow,
};

} // namespace detection
